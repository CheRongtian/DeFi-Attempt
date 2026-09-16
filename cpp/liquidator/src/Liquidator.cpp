#include "dlp/liquidator/Liquidator.hpp"

#include <utility>

#include "dlp/ethereum/Abi.hpp"
#include "dlp/ethereum/Uint256Math.hpp"
#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::liquidator
{

Liquidator::Liquidator(
    LiquidationChain& chain,
    LiquidatorConfig config
)
    : chain_(chain), config_(std::move(config))
{
}

ethereum::Bytes Liquidator::EncodeLiquidation(const risk::LiquidationCandidate& candidate) const
{
    const auto minimumCollateral = ethereum::Uint256Math::MulDivDown(
        candidate.expectedCollateral,
        ethereum::Uint256{config_.minimumCollateralBps},
        ethereum::Uint256{10'000}
    );
    return ethereum::Abi::EncodeFunction(
        "liquidate(address,address,address,uint256,uint256)",
        {
            candidate.borrower,
            candidate.debtAsset,
            candidate.collateralAsset,
            candidate.maxRepay,
            minimumCollateral
        }
    );
}

std::optional<LiquidationSubmission> Liquidator::Prepare(
    const risk::LiquidationCandidate& indexedCandidate
)
{
    const auto market = chain_.LoadMarket();
    const auto position = chain_.LoadPosition(indexedCandidate.borrower);
    const auto currentRisk = risk::RiskCalculator::Evaluate(position, market, market.observedAt);
    const auto currentCandidate = risk::RiskCalculator::BuildCandidate(currentRisk, market);
    if(!currentCandidate.has_value())
    {
        return std::nullopt;
    }

    const auto data = EncodeLiquidation(*currentCandidate);
    const auto gasLimit = chain_.EstimateGas(
        config_.operatorAddress,
        config_.liquidationManager,
        data
    );
    const auto fees = chain_.GetFeeQuote();
    const auto gasCostUsd = risk::RiskCalculator::CollateralValue(
        gasLimit * fees.maxFeePerGas,
        market.wethPrice
    );
    if(currentCandidate->expectedBonus <= gasCostUsd
        || currentCandidate->expectedBonus - gasCostUsd < config_.minimumProfitUsdWad)
    {
        return std::nullopt;
    }

    return LiquidationSubmission{
        config_.liquidationManager,
        std::move(data),
        ethereum::Uint256{}
    };
}

}
