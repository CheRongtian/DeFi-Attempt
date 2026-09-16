#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/RpcClient.hpp"
#include "dlp/liquidator/LiquidationChain.hpp"
#include "dlp/liquidator/LiquidationJobs.hpp"
#include "dlp/liquidator/Liquidator.hpp"
#include "dlp/messaging/JetStream.hpp"

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
    return std::stoull(EnvironmentOrDefault(name, fallback));
}

}

int main(int argc, char* argv[])
{
    const bool runOnce = argc == 2 && std::string{argv[1]} == "--once";
    if(argc > 2 || (argc == 2 && !runOnce))
    {
        std::cerr << "usage: dlp_liquidator [--once]\n";
        return 2;
    }

    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);

    try
    {
        const auto rpcUrl = EnvironmentOrDefault("DLP_RPC_URL", "http://127.0.0.1:8545");
        const auto databaseUrl = EnvironmentOrDefault(
            "DLP_DATABASE_URL",
            "postgresql://dlp:dlp@127.0.0.1:5432/dlp"
        );
        const auto workerId = EnvironmentOrDefault("DLP_LIQUIDATOR_WORKER_ID", "liquidator-local");
        const auto leaseDuration = std::chrono::seconds{
            UnsignedEnvironment("DLP_LIQUIDATION_LEASE_SECONDS", "30")
        };
        const auto operatorAddress = dlp::ethereum::Address::FromHex(
            RequiredEnvironment("DLP_OPERATOR_ADDRESS")
        );
        const dlp::liquidator::LiquidationContracts contracts{
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_POOL_ADDRESS")),
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_LIQUIDATION_MANAGER_ADDRESS")),
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_ORACLE_ADDRESS")),
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_WETH_ADDRESS")),
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_USDC_ADDRESS"))
        };

        const auto chainId = dlp::ethereum::RpcClient{rpcUrl}.GetChainId();
        dlp::liquidator::RpcLiquidationChain chain{rpcUrl, contracts};
        dlp::liquidator::Liquidator liquidator{
            chain,
            dlp::liquidator::LiquidatorConfig{
                operatorAddress,
                contracts.liquidationManager,
                dlp::ethereum::Uint256::FromDecimal(
                    EnvironmentOrDefault("DLP_MIN_LIQUIDATION_PROFIT_USD_WAD", "0")
                ),
                UnsignedEnvironment("DLP_MIN_COLLATERAL_BPS", "9900")
            }
        };
        dlp::liquidator::PostgresLiquidationJobStore jobs{databaseUrl};
        dlp::messaging::JetStreamConsumer events{
            EnvironmentOrDefault("DLP_NATS_URL", "nats://127.0.0.1:4222"),
            std::string{dlp::messaging::RISK_LIQUIDATION_DETECTED},
            "liquidator"
        };

        do
        {
            try
            {
                auto message = events.Fetch(std::chrono::milliseconds{500});
                if(message.has_value())
                {
                    const auto queued = jobs.Enqueue(message->Event());
                    message->Ack();
                    if(queued)
                    {
                        std::cout << "queued liquidation job " << message->Event().eventId << '\n';
                    }
                }

                auto invalidated = jobs.InvalidateNonCanonical(chainId);
                auto job = jobs.Claim(chainId, workerId, leaseDuration);
                if(job.has_value())
                {
                    auto submission = liquidator.Prepare(job->candidate);
                    if(submission.has_value())
                    {
                        if(jobs.Submit(*job, *submission, operatorAddress))
                        {
                            std::cout << "submitted liquidation job " << job->jobId << '\n';
                        }
                    }
                    else if(jobs.Invalidate(*job, "position is no longer profitable to liquidate"))
                    {
                        ++invalidated;
                    }
                }
                const auto reconciled = jobs.ReconcileSubmitted();
                if(invalidated != 0U || reconciled != 0U)
                {
                    std::cout << "updated " << invalidated << " stale and " << reconciled
                              << " submitted liquidation job(s)\n";
                }
            }
            catch(const std::exception& exception)
            {
                if(runOnce)
                {
                    throw;
                }
                std::cerr << "liquidator iteration failed: " << exception.what() << '\n';
                std::this_thread::sleep_for(std::chrono::seconds{1});
            }
        }
        while(running.load() && !runOnce);
    }
    catch(const std::exception& exception)
    {
        std::cerr << "liquidator stopped: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
