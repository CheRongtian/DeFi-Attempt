#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "dlp/api/ApiChain.hpp"
#include "dlp/api/ApiService.hpp"
#include "dlp/api/ApiStore.hpp"
#include "dlp/api/HttpServer.hpp"
#include "dlp/risk/PostgresRiskRepository.hpp"
#include "dlp/risk/RiskEngine.hpp"

namespace
{

[[nodiscard]] std::string EnvironmentOrDefault(const char* name, const char* fallback)
{
    const auto* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? std::string{fallback} : std::string{value};
}

}

int main()
{
    try
    {
        const auto databaseUrl = EnvironmentOrDefault(
            "DLP_DATABASE_URL",
            "postgresql://dlp:dlp@127.0.0.1:5432/dlp"
        );
        const auto rpcUrl = EnvironmentOrDefault("DLP_RPC_URL", "http://127.0.0.1:8545");
        const auto apiAddress = EnvironmentOrDefault("DLP_API_ADDRESS", "127.0.0.1");
        const auto apiPort = EnvironmentOrDefault("DLP_API_PORT", "8080");
        dlp::api::RpcApiChain chain{rpcUrl};
        const auto chainId = dlp::ethereum::RpcClient{rpcUrl}.GetChainId();
        dlp::risk::PostgresRiskRepository riskRepository{databaseUrl};
        dlp::risk::RiskEngine riskEngine{riskRepository, chainId};
        dlp::api::PostgresApiStore apiStore{databaseUrl};
        dlp::api::ApiService service{riskEngine, apiStore, chain, chainId};
        dlp::api::HttpServer server{
            service,
            apiAddress,
            static_cast<std::uint16_t>(std::stoul(apiPort))
        };
        std::cout << "API listening on http://" << apiAddress << ':' << apiPort << '\n';
        server.Run();
    }
    catch(const std::exception& exception)
    {
        std::cerr << "API server stopped: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
