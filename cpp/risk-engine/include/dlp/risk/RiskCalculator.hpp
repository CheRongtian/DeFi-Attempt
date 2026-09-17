#ifndef DLP_RISK_RISK_CALCULATOR_HPP
#define DLP_RISK_RISK_CALCULATOR_HPP

#include <cstdint>
#include <optional>

#include "dlp/risk/RiskTypes.hpp"

namespace dlp::risk
{

class RiskCalculator final
{
public:
    [[nodiscard]] static const ethereum::Uint256& Wad();
    [[nodiscard]] static const ethereum::Uint256& Ray();
    [[nodiscard]] static const ethereum::Uint256& MaximumUint256();
    [[nodiscard]] static const ethereum::Uint256& LtvBasisPoints();
    [[nodiscard]] static const ethereum::Uint256& LiquidationThresholdBasisPoints();

    [[nodiscard]] static ethereum::Uint256 DebtAtIndex(
        const ethereum::Uint256& scaledDebt,
        const ethereum::Uint256& borrowIndex
    );
    [[nodiscard]] static ethereum::Uint256 ProjectedBorrowIndex(
        const MarketSnapshot& market,
        std::uint64_t evaluatedAt
    );
    [[nodiscard]] static ethereum::Uint256 CollateralValue(
        const ethereum::Uint256& wethAmount,
        const ethereum::Uint256& wethPrice
    );
    [[nodiscard]] static ethereum::Uint256 DebtValue(
        const ethereum::Uint256& usdcAmount,
        const ethereum::Uint256& usdcPrice
    );
    [[nodiscard]] static PositionRisk Evaluate(
        const Position& position,
        const MarketSnapshot& market,
        std::uint64_t evaluatedAt
    );
    [[nodiscard]] static PositionRisk EvaluateIndexed(
        const IndexedPosition& position,
        const MarketSnapshot& market,
        std::uint64_t evaluatedAt
    );
    [[nodiscard]] static std::optional<LiquidationCandidate> BuildCandidate(
        const PositionRisk& risk,
        const MarketSnapshot& market,
        std::optional<ethereum::Uint256> requestedRepay = std::nullopt
    );

private:
    static void RequireFreshPrice(
        const ethereum::Uint256& price,
        const ethereum::Uint256& updatedAt,
        const ethereum::Uint256& maxAge,
        std::uint64_t evaluatedAt,
        const char* asset
    );
};

}

#endif
