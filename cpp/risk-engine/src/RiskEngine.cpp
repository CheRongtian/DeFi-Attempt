#include "dlp/risk/RiskEngine.hpp"

#include <utility>

namespace dlp::risk
{

RiskEngine::RiskEngine(RiskRepository& repository, ethereum::Uint256 chainId)
    : repository_(repository), chainId_(std::move(chainId))
{
}

MarketSnapshot RiskEngine::LoadMarket() const
{
    return repository_.LoadMarket(chainId_);
}

std::optional<PositionRisk> RiskEngine::Evaluate(
    const ethereum::Address& user,
    std::uint64_t evaluatedAt
) const
{
    const auto position = repository_.LoadPosition(chainId_, user);
    if(!position.has_value())
    {
        return std::nullopt;
    }
    return RiskCalculator::EvaluateIndexed(*position, repository_.LoadMarket(chainId_), evaluatedAt);
}

std::vector<LiquidationCandidate> RiskEngine::Scan(
    std::size_t limit,
    std::uint64_t evaluatedAt
) const
{
    const auto market = repository_.LoadMarket(chainId_);
    const auto positions = repository_.LoadPositions(chainId_, limit);
    std::vector<LiquidationCandidate> candidates;

    for(const auto& position : positions)
    {
        const auto risk = RiskCalculator::EvaluateIndexed(position, market, evaluatedAt);
        auto candidate = RiskCalculator::BuildCandidate(risk, market);
        if(candidate.has_value())
        {
            candidates.push_back(std::move(*candidate));
        }
    }
    return candidates;
}

}
