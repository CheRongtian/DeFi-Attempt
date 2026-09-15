#include "dlp/liquidator/Liquidator.hpp"

#include <stdexcept>
#include <utility>

#include "dlp/ethereum/Abi.hpp"
#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Uint256Math.hpp"
#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::liquidator
{

Liquidator::Liquidator(
    risk::RiskEngine& riskEngine,
    LiquidationChain& chain,
    tx::TransactionQueue& transactionQueue,
    LiquidatorConfig config
)
    : riskEngine_(riskEngine),
      chain_(chain),
      transactionQueue_(transactionQueue),
      config_(std::move(config))
{
    if(config_.minimumCollateralBps > 10'000U)
    {
        throw std::invalid_argument("minimum collateral BPS cannot exceed 10000");
    }
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

std::string Liquidator::JobId(const risk::LiquidationCandidate& candidate) const
{
    return "liquidation:" + candidate.borrower.ToHex(false) + ":"
        + std::to_string(candidate.blockNumber) + ":" + ethereum::Hex::Encode(candidate.blockHash, false);
}

bool Liquidator::Process(const risk::LiquidationCandidate& indexedCandidate)
{
    const auto market = chain_.LoadMarket();
    const auto position = chain_.LoadPosition(indexedCandidate.borrower);
    const auto currentRisk = risk::RiskCalculator::Evaluate(position, market, market.observedAt);
    const auto currentCandidate = risk::RiskCalculator::BuildCandidate(currentRisk, market);
    if(!currentCandidate.has_value())
    {
        return false;
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
        return false;
    }

    return transactionQueue_.Queue(
        JobId(*currentCandidate),
        config_.liquidationManager,
        data
    );
}

std::size_t Liquidator::RunOnce(std::size_t scanLimit, std::uint64_t evaluatedAt)
{
    const auto candidates = riskEngine_.Scan(scanLimit, evaluatedAt);
    std::size_t queued = 0;
    for(const auto& candidate : candidates)
    {
        if(Process(candidate))
        {
            ++queued;
        }
    }
    return queued;
}

}
