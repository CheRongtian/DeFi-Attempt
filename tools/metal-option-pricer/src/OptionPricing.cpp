#include "dlp/options/OptionPricing.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dlp::options
{

namespace
{

double NormalCdf(double value)
{
    return 0.5 * std::erfc(-value / std::sqrt(2.0));
}

void ValidateContract(const PricingRequest& request)
{
    if(request.spot <= 0.0 || request.strike <= 0.0)
    {
        throw std::invalid_argument{"spot and strike must be greater than zero"};
    }
    if(request.expiryYears <= 0.0)
    {
        throw std::invalid_argument{"expiryYears must be greater than zero"};
    }
    if(request.volatility <= 0.0)
    {
        throw std::invalid_argument{"volatility must be greater than zero"};
    }
}

}

OptionType ParseOptionType(std::string_view value)
{
    if(value == "call")
    {
        return OptionType::Call;
    }
    if(value == "put")
    {
        return OptionType::Put;
    }
    throw std::invalid_argument{"optionType must be call or put"};
}

std::string_view ToString(OptionType optionType) noexcept
{
    return optionType == OptionType::Call ? "call" : "put";
}

double Payoff(OptionType optionType, double strike, double spot)
{
    return optionType == OptionType::Call
        ? std::max(spot - strike, 0.0)
        : std::max(strike - spot, 0.0);
}

double BlackScholesPrice(const PricingRequest& request)
{
    ValidateContract(request);

    const double rootExpiry = std::sqrt(request.expiryYears);
    const double d1 = (
        std::log(request.spot / request.strike) +
        (request.riskFreeRate + 0.5 * request.volatility * request.volatility) * request.expiryYears
    ) / (request.volatility * rootExpiry);
    const double d2 = d1 - request.volatility * rootExpiry;
    const double discountedStrike = request.strike *
        std::exp(-request.riskFreeRate * request.expiryYears);

    if(request.optionType == OptionType::Call)
    {
        return request.spot * NormalCdf(d1) - discountedStrike * NormalCdf(d2);
    }
    return discountedStrike * NormalCdf(-d2) - request.spot * NormalCdf(-d1);
}

}
