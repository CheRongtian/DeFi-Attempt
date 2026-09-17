#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "dlp/ethereum/RpcClient.hpp"
#include "dlp/messaging/JetStream.hpp"
#include "dlp/observability/Metrics.hpp"
#include "dlp/observability/RpcMetrics.hpp"
#include "dlp/risk/PostgresRiskRepository.hpp"
#include "dlp/risk/RiskWorker.hpp"

namespace
{

std::atomic_bool running{true};

void Stop(int)
{
    running.store(false);
}

[[nodiscard]] std::string EnvironmentOrDefault(const char* name, const char* fallback)
{
    const auto* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? std::string{fallback} : std::string{value};
}

[[nodiscard]] std::uint64_t UnsignedEnvironment(const char* name, const char* fallback)
{
    return std::stoull(EnvironmentOrDefault(name, fallback));
}

}

int main(int argc, char* argv[])
{
    const bool runOnce = argc == 2 && std::string{argv[1]} == "--once";
    if(argc > 2 || (argc == 2 && !runOnce))
    {
        std::cerr << "usage: dlp_risk_worker [--once]\n";
        return 2;
    }

    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);

    try
    {
        const auto databaseUrl = EnvironmentOrDefault(
            "DLP_DATABASE_URL",
            "postgresql://dlp:dlp@127.0.0.1:5432/dlp"
        );
        dlp::observability::ServiceMetrics metrics{
            "risk-engine",
            EnvironmentOrDefault("DLP_METRICS_ADDRESS", "0.0.0.0"),
            static_cast<std::uint16_t>(UnsignedEnvironment("DLP_METRICS_PORT", "9103"))
        };
        dlp::observability::RpcMetrics rpcMetrics{metrics};
        auto& scanned = metrics.AddCounter(
            "risk_positions_scanned_total",
            "Total positions evaluated by the risk engine"
        );
        auto& candidates = metrics.AddCounter(
            "liquidation_candidates_total",
            "Total liquidation candidates produced by the risk engine"
        );
        auto& currentCandidates = metrics.AddGauge(
            "liquidation_candidates",
            "Liquidation candidates produced by the latest scan"
        );
        auto& errors = metrics.AddCounter("risk_engine_errors_total", "Total failed risk iterations");
        dlp::ethereum::RpcClient rpc{EnvironmentOrDefault("DLP_RPC_URL", "http://127.0.0.1:8545")};
        dlp::risk::PostgresRiskRepository repository{databaseUrl};
        dlp::risk::RiskEngine engine{repository, rpc.GetChainId()};
        dlp::risk::PostgresRiskEventStore store{databaseUrl};
        dlp::risk::RiskWorker worker{
            engine,
            store,
            static_cast<std::size_t>(UnsignedEnvironment("DLP_LIQUIDATION_SCAN_LIMIT", "1000"))
        };
        dlp::messaging::JetStreamConsumer events{
            EnvironmentOrDefault("DLP_NATS_URL", "nats://127.0.0.1:4222"),
            "chain.>",
            "risk-engine"
        };

        const auto rescanInterval = std::chrono::seconds{
            UnsignedEnvironment("DLP_RISK_RESCAN_SECONDS", "30")
        };
        auto nextRescan = std::chrono::steady_clock::now() + rescanInterval;
        metrics.SetReady(true);
        do
        {
            try
            {
                auto message = events.Fetch(std::chrono::milliseconds{500});
                const auto now = static_cast<std::uint64_t>(std::time(nullptr));
                if(message.has_value())
                {
                    const auto result = worker.ProcessWithStats(message->Event().eventId, now);
                    scanned.Increment(static_cast<double>(result.positionsScanned));
                    candidates.Increment(static_cast<double>(result.liquidationCandidates));
                    currentCandidates.Set(static_cast<double>(result.liquidationCandidates));
                    if(result.committed)
                    {
                        std::cout << "processed risk event " << message->Event().eventId << '\n';
                    }
                    message->Ack();
                }
                if(std::chrono::steady_clock::now() >= nextRescan)
                {
                    const auto result = worker.RescanWithStats(now);
                    scanned.Increment(static_cast<double>(result.positionsScanned));
                    candidates.Increment(static_cast<double>(result.liquidationCandidates));
                    currentCandidates.Set(static_cast<double>(result.liquidationCandidates));
                    if(result.committed)
                    {
                        std::cout << "completed periodic risk rescan\n";
                    }
                    nextRescan = std::chrono::steady_clock::now() + rescanInterval;
                }
            }
            catch(const std::exception& exception)
            {
                errors.Increment();
                if(runOnce)
                {
                    throw;
                }
                std::cerr << "risk worker iteration failed: " << exception.what() << '\n';
                std::this_thread::sleep_for(std::chrono::seconds{1});
            }
        }
        while(running.load() && !runOnce);
    }
    catch(const std::exception& exception)
    {
        std::cerr << "risk worker stopped: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
