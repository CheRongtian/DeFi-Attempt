#ifndef DLP_API_API_SERVICE_HPP
#define DLP_API_API_SERVICE_HPP

#include <string_view>

#include "dlp/api/ApiChain.hpp"
#include "dlp/api/ApiStore.hpp"
#include "dlp/risk/RiskEngine.hpp"

namespace dlp::api
{

class ApiService final
{
public:
    ApiService(
        risk::RiskEngine& riskEngine,
        ApiStore& store,
        ApiChain& chain,
        ethereum::Uint256 chainId
    );

    [[nodiscard]] ApiResponse Handle(const ApiRequest& request) const;

private:
    [[nodiscard]] ApiResponse Markets() const;
    [[nodiscard]] ApiResponse Position(std::string_view address, bool healthOnly) const;
    [[nodiscard]] ApiResponse Liquidations() const;
    [[nodiscard]] ApiResponse Stats() const;
    [[nodiscard]] ApiResponse Simulate(std::string_view body) const;

    risk::RiskEngine& riskEngine_;
    ApiStore& store_;
    ApiChain& chain_;
    ethereum::Uint256 chainId_;
};

}

#endif
