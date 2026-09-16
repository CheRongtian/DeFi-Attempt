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

        do
        {
            try
            {
                const auto indexed = indexer.SyncToHead();
                if(indexed != 0U)
                {
                    std::cout << "indexed " << indexed << " block(s)\n";
                }
            }
            catch(const std::exception& exception)
            {
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
