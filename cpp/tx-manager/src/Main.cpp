#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "dlp/ethereum/Transaction.hpp"
#include "dlp/ethereum/RpcEndpoints.hpp"
#include "dlp/observability/Metrics.hpp"
#include "dlp/observability/RpcMetrics.hpp"
#include "dlp/tx/PostgresTransactionStore.hpp"
#include "dlp/tx/TransactionRpc.hpp"
#include "dlp/tx/TxManager.hpp"

namespace
{

std::atomic_bool running{true};

void Stop(int)
{
    running.store(false);
}

[[nodiscard]] std::string EnvironmentOrDefault(const char* name, std::string fallback)
{
    const auto* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? std::move(fallback) : std::string{value};
}

[[nodiscard]] std::string RequiredEnvironment(const char* name)
{
    const auto* value = std::getenv(name);
    if(value == nullptr || *value == '\0')
    {
        throw std::runtime_error(std::string{"missing environment variable: "} + name);
    }
    return value;
}

[[nodiscard]] std::uint64_t UnsignedEnvironment(const char* name, const char* fallback)
{
    const auto value = EnvironmentOrDefault(name, fallback);
    std::size_t parsed = 0;
    const auto result = std::stoull(value, &parsed);
    if(parsed != value.size())
    {
        throw std::invalid_argument(std::string{name} + " must be an unsigned integer");
    }
    return result;
}

}

int main(int argc, char* argv[])
{
    const bool runOnce = argc == 2 && std::string{argv[1]} == "--once";
    if(argc > 2 || (argc == 2 && !runOnce))
    {
        std::cerr << "usage: dlp_tx_manager [--once]\n";
        return 2;
    }

    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);

    try
    {
        dlp::tx::PostgresTransactionStore store{
            EnvironmentOrDefault("DLP_DATABASE_URL", "postgresql://dlp:dlp@127.0.0.1:5432/dlp")
        };
        const auto defaultRpc = EnvironmentOrDefault(
            "DLP_RPC_URL",
            "http://127.0.0.1:8545"
        );
        const auto primaryRpc = EnvironmentOrDefault("DLP_RPC_PRIMARY_URL", defaultRpc);
        auto broadcastRpcs = dlp::ethereum::ParseRpcEndpoints(
            EnvironmentOrDefault("DLP_RPC_BROADCAST_URLS", "")
        );
        auto transactionRpcs = dlp::ethereum::MergeRpcEndpoints(
            primaryRpc,
            std::move(broadcastRpcs)
        );
        auto additionalRpcs = std::vector<std::string>(
            std::next(transactionRpcs.begin()),
            transactionRpcs.end()
        );
        dlp::observability::ServiceMetrics metrics{
            "tx-manager",
            EnvironmentOrDefault("DLP_METRICS_ADDRESS", "0.0.0.0"),
            static_cast<std::uint16_t>(UnsignedEnvironment("DLP_METRICS_PORT", "9104"))
        };
        dlp::observability::RpcMetrics rpcMetrics{metrics};
        auto& pending = metrics.AddGauge("tx_pending_total", "Current pending or submitted transactions");
        auto& included = metrics.AddCounter("tx_included_total", "Total transactions included on chain");
        auto& finalized = metrics.AddCounter("tx_finalized_total", "Total finalized transactions");
        auto& reorged = metrics.AddCounter("tx_reorged_total", "Total transactions invalidated by reorgs");
        auto& failed = metrics.AddCounter("tx_failed_total", "Total failed transactions");
        auto& confirmationBlocks = metrics.AddHistogram(
            "tx_confirmation_latency_blocks",
            "Blocks from transaction submission through inclusion",
            {1.0, 2.0, 3.0, 5.0, 8.0, 13.0, 21.0}
        );
        dlp::tx::RpcTransactionClient rpc{transactionRpcs.front(), std::move(additionalRpcs)};
        dlp::ethereum::Secp256k1Signer signer{RequiredEnvironment("DLP_OPERATOR_PRIVATE_KEY")};
        dlp::tx::TxManager manager{
            store,
            rpc,
            signer,
            dlp::tx::TxManagerConfig{
                UnsignedEnvironment("DLP_TX_CONFIRMATIONS", "1"),
                UnsignedEnvironment("DLP_TX_REPLACEMENT_BLOCKS", "3"),
                static_cast<std::uint32_t>(UnsignedEnvironment("DLP_TX_MAX_RETRIES", "3"))
            }
        };
        metrics.SetReady(true);

        do
        {
            try
            {
                const auto result = manager.RunOnce();
                pending.Set(static_cast<double>(result.active));
                included.Increment(static_cast<double>(result.included));
                finalized.Increment(static_cast<double>(result.finalized));
                reorged.Increment(static_cast<double>(result.reorged));
                failed.Increment(static_cast<double>(result.failed));
                for(const auto blocks : result.confirmationBlocks)
                {
                    confirmationBlocks.Observe(static_cast<double>(blocks));
                }
            }
            catch(const std::exception& exception)
            {
                if(runOnce)
                {
                    throw;
                }
                std::cerr << "transaction manager iteration failed: " << exception.what() << '\n';
            }
            if(!runOnce)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        while(running.load() && !runOnce);
    }
    catch(const std::exception& exception)
    {
        std::cerr << "transaction manager stopped: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
