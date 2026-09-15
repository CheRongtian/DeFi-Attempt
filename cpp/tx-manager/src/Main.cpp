#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#include "dlp/ethereum/Transaction.hpp"
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
        dlp::tx::RpcTransactionClient rpc{
            EnvironmentOrDefault("DLP_RPC_URL", "http://127.0.0.1:8545")
        };
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

        do
        {
            try
            {
                manager.RunOnce();
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
