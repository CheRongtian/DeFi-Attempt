#include <atomic>
#include <chrono>
#include <cstdint>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/RpcEndpoints.hpp"
#include "dlp/indexer/Indexer.hpp"
#include "dlp/indexer/PostgresStore.hpp"
#include "dlp/indexer/RpcChainClient.hpp"
#include "dlp/observability/Metrics.hpp"
#include "dlp/observability/RpcMetrics.hpp"

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

[[nodiscard]] std::uint64_t StartBlock()
{
    const auto value = EnvironmentOrDefault("DLP_START_BLOCK", "0");
    std::size_t parsedLength = 0;
    const auto result = std::stoull(value, &parsedLength);
    if(parsedLength != value.size())
    {
        throw std::invalid_argument("DLP_START_BLOCK must be an unsigned integer");
    }
    return static_cast<std::uint64_t>(result);
}

}

int main(int argc, char* argv[])
{
    const bool runOnce = argc == 2 && std::string{argv[1]} == "--once";
    if(argc > 2 || (argc == 2 && !runOnce))
    {
        std::cerr << "usage: dlp_indexer [--once]\n";
        return 2;
    }

    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);

    try
    {
        const dlp::indexer::ChainContracts contracts{
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_POOL_ADDRESS")),
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_ORACLE_ADDRESS")),
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_WETH_ADDRESS")),
            dlp::ethereum::Address::FromHex(RequiredEnvironment("DLP_USDC_ADDRESS"))
        };
        dlp::indexer::RpcChainClient chain{
            dlp::ethereum::MergeRpcEndpoints(
                EnvironmentOrDefault("DLP_RPC_URL", "http://127.0.0.1:8545"),
                dlp::ethereum::ParseRpcEndpoints(
                    EnvironmentOrDefault("DLP_RPC_FAILOVER_URLS", "")
                )
            ),
            contracts
        };
        dlp::indexer::PostgresStore store{
            EnvironmentOrDefault(
                "DLP_DATABASE_URL",
                "postgresql://dlp:dlp@127.0.0.1:5432/dlp"
            )
        };
        dlp::indexer::Indexer indexer{chain, store, contracts, StartBlock()};
        dlp::observability::ServiceMetrics metrics{
            "indexer",
            EnvironmentOrDefault("DLP_METRICS_ADDRESS", "0.0.0.0"),
            static_cast<std::uint16_t>(std::stoul(EnvironmentOrDefault("DLP_METRICS_PORT", "9101")))
        };
        dlp::observability::RpcMetrics rpcMetrics{metrics};
        auto& indexedBlocks = metrics.AddCounter(
            "indexed_blocks_total",
            "Total canonical blocks committed by the indexer"
        );
        auto& reorgs = metrics.AddCounter("indexer_reorgs_total", "Total indexer chain rewinds");
        auto& errors = metrics.AddCounter("indexer_errors_total", "Total failed indexer iterations");
        auto& indexedHeight = metrics.AddGauge(
            "indexed_block_height",
            "Latest canonical block committed by the indexer"
        );
        auto& chainHeight = metrics.AddGauge("chain_head_height", "Latest chain head seen by the indexer");
        auto& lag = metrics.AddGauge("indexer_lag_blocks", "Blocks between chain head and index cursor");
        std::uint64_t observedReorgs = 0;
        metrics.SetReady(true);

        do
        {
            try
            {
                const auto indexed = indexer.SyncToHead();
                if(indexed != 0U)
                {
                    std::cout << "indexed " << indexed << " block(s)\n";
                    indexedBlocks.Increment(static_cast<double>(indexed));
                }
                const auto& status = indexer.Status();
                indexedHeight.Set(static_cast<double>(status.indexedBlock));
                chainHeight.Set(static_cast<double>(status.chainHead));
                lag.Set(static_cast<double>(status.chainHead - status.indexedBlock));
                if(status.reorgs > observedReorgs)
                {
                    reorgs.Increment(static_cast<double>(status.reorgs - observedReorgs));
                    observedReorgs = status.reorgs;
                }
            }
            catch(const std::exception& exception)
            {
                errors.Increment();
                if(runOnce)
                {
                    throw;
                }
                std::cerr << "indexing failed: " << exception.what() << '\n';
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
        std::cerr << "indexer stopped: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}
