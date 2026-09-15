#ifndef DLP_RISK_RISK_ENGINE_HPP
#define DLP_RISK_RISK_ENGINE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "dlp/risk/RiskCalculator.hpp"
#include "dlp/risk/RiskRepository.hpp"

namespace dlp::risk
{

class RiskEngine final
{
public:
    RiskEngine(RiskRepository& repository, ethereum::Uint256 chainId);

    [[nodiscard]] MarketSnapshot LoadMarket() const;
    [[nodiscard]] std::optional<PositionRisk> Evaluate(
        const ethereum::Address& user,
        std::uint64_t evaluatedAt
    ) const;
    [[nodiscard]] std::vector<LiquidationCandidate> Scan(
        std::size_t limit,
        std::uint64_t evaluatedAt
    ) const;

private:
    RiskRepository& repository_;
    ethereum::Uint256 chainId_;
};

}

#endif
