#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "dlp/api/ApiChain.hpp"
#include "dlp/api/ApiService.hpp"
#include "dlp/api/ApiStore.hpp"
#include "dlp/api/HttpServer.hpp"
#include "dlp/ethereum/RpcEndpoints.hpp"
#include "dlp/observability/Metrics.hpp"
#include "dlp/observability/RpcMetrics.hpp"
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
        auto readEndpoints = dlp::ethereum::ParseRpcEndpoints(
            EnvironmentOrDefault("DLP_READ_RPC_URLS", "")
        );
        if(readEndpoints.empty())
        {
            readEndpoints.push_back(rpcUrl);
        }
        const auto apiAddress = EnvironmentOrDefault("DLP_API_ADDRESS", "127.0.0.1");
        const auto apiPort = EnvironmentOrDefault("DLP_API_PORT", "8080");
        dlp::observability::ServiceMetrics metrics{
            "api-server",
            EnvironmentOrDefault("DLP_METRICS_ADDRESS", "0.0.0.0"),
            static_cast<std::uint16_t>(std::stoul(EnvironmentOrDefault("DLP_METRICS_PORT", "9106")))
        };
        dlp::observability::RpcMetrics rpcMetrics{metrics};
        dlp::api::RpcApiChain chain{std::move(readEndpoints)};
        const auto chainId = chain.GetChainId();
        dlp::risk::PostgresRiskRepository riskRepository{databaseUrl};
        dlp::risk::RiskEngine riskEngine{riskRepository, chainId};
        dlp::api::PostgresApiStore apiStore{databaseUrl};
        auto& positionCount = metrics.AddGauge(
            "protocol_position_count",
            "Current indexed protocol position count"
        );
        auto& liquidationCount = metrics.AddGauge(
            "protocol_liquidation_count",
            "Current indexed liquidation count"
        );
        auto& stateCollectionErrors = metrics.AddCounter(
            "api_state_collection_errors_total",
            "Total failed protocol state metric collections"
        );
        std::jthread stateMetrics([&](std::stop_token stopToken) {
            while(!stopToken.stop_requested())
            {
                try
                {
                    const auto stats = apiStore.LoadProtocolStats(chainId);
                    positionCount.Set(static_cast<double>(stats.positionCount));
                    liquidationCount.Set(static_cast<double>(stats.liquidationCount));
                }
                catch(const std::exception&)
                {
                    stateCollectionErrors.Increment();
                }
                std::this_thread::sleep_for(std::chrono::seconds{5});
            }
        });
        dlp::api::ApiService service{riskEngine, apiStore, chain, chainId};
        dlp::api::HttpServer server{
            service,
            apiAddress,
            static_cast<std::uint16_t>(std::stoul(apiPort))
        };
        metrics.SetReady(true);
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
