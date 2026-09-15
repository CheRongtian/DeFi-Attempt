#include "dlp/risk/RiskCalculator.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

#include "dlp/ethereum/Uint256Math.hpp"

namespace dlp::risk
{

namespace
{

using ethereum::Uint256;
using ethereum::Uint256Math;

const Uint256& Bps()
{
    static const Uint256 value{10'000};
    return value;
}

const Uint256& OracleScale()
{
    static const Uint256 value{100'000'000};
    return value;
}

const Uint256& UsdcValueMultiplier()
{
    static const Uint256 value{10'000};
    return value;
}

const Uint256& LtvBps()
{
    static const Uint256 value{7'500};
    return value;
}

const Uint256& LiquidationThresholdBps()
{
    static const Uint256 value{8'000};
    return value;
}

const Uint256& CloseFactorBps()
{
    static const Uint256 value{5'000};
    return value;
}

const Uint256& LiquidationBonusBps()
{
    static const Uint256 value{500};
    return value;
}

const Uint256& MinimumUsdcBorrow()
{
    static const Uint256 value{1'000'000};
    return value;
}

const Uint256& OptimalUtilization()
{
    static const auto value = Uint256::FromDecimal("800000000000000000000000000");
    return value;
}

const Uint256& BaseRate()
{
    static const auto value = Uint256::FromDecimal("20000000000000000000000000");
    return value;
}

const Uint256& SlopeOne()
{
    static const auto value = Uint256::FromDecimal("80000000000000000000000000");
    return value;
}

const Uint256& SlopeTwo()
{
    static const auto value = Uint256::FromDecimal("1000000000000000000000000000");
    return value;
}

const Uint256& SecondsPerYear()
{
    static const Uint256 value{31'536'000};
    return value;
}

[[nodiscard]] Uint256 Minimum(const Uint256& left, const Uint256& right)
{
    return left < right ? left : right;
}

}

const Uint256& RiskCalculator::Wad()
{
    static const auto value = Uint256::FromDecimal("1000000000000000000");
    return value;
}

const Uint256& RiskCalculator::Ray()
{
    static const auto value = Uint256::FromDecimal("1000000000000000000000000000");
    return value;
}

const Uint256& RiskCalculator::MaximumUint256()
{
    static const auto value = Uint256::FromDecimal(
        "115792089237316195423570985008687907853269984665640564039457584007913129639935"
    );
    return value;
}

Uint256 RiskCalculator::DebtAtIndex(const Uint256& scaledDebt, const Uint256& borrowIndex)
{
    return Uint256Math::MulDivUp(scaledDebt, borrowIndex, Ray());
}

Uint256 RiskCalculator::ProjectedBorrowIndex(const MarketSnapshot& market, std::uint64_t evaluatedAt)
{
    if(market.totalScaledUsdcDebt.IsZero())
    {
        return market.borrowIndex;
    }

    const auto lastUpdate = market.lastInterestTimestamp.ToUint64();
    if(evaluatedAt <= lastUpdate)
    {
        return market.borrowIndex;
    }

    const auto totalDebt = DebtAtIndex(market.totalScaledUsdcDebt, market.borrowIndex);
    const auto utilization = Uint256Math::MulDivDown(
        totalDebt,
        Ray(),
        market.availableUsdcLiquidity + totalDebt
    );
    const auto borrowRate = utilization <= OptimalUtilization()
        ? BaseRate() + Uint256Math::MulDivDown(utilization, SlopeOne(), OptimalUtilization())
        : BaseRate() + SlopeOne() + Uint256Math::MulDivDown(
            utilization - OptimalUtilization(),
            SlopeTwo(),
            Ray() - OptimalUtilization()
        );
    const auto growth = Ray() + Uint256Math::MulDivDown(
        borrowRate,
        Uint256{evaluatedAt - lastUpdate},
        SecondsPerYear()
    );
    return Uint256Math::MulDivDown(market.borrowIndex, growth, Ray());
}

Uint256 RiskCalculator::CollateralValue(const Uint256& wethAmount, const Uint256& wethPrice)
{
    return Uint256Math::MulDivDown(wethAmount, wethPrice, OracleScale());
}

Uint256 RiskCalculator::DebtValue(const Uint256& usdcAmount, const Uint256& usdcPrice)
{
    return usdcAmount * usdcPrice * UsdcValueMultiplier();
}

void RiskCalculator::RequireFreshPrice(
    const Uint256& price,
    const Uint256& updatedAt,
    const Uint256& maxAge,
    std::uint64_t evaluatedAt,
    const char* asset
)
{
    if(price.IsZero() || updatedAt.IsZero() || maxAge.IsZero())
    {
        throw std::runtime_error(std::string{asset} + " price is unavailable");
    }
    const auto updated = updatedAt.ToUint64();
    const auto maximumAge = maxAge.ToUint64();
    if(evaluatedAt < updated || evaluatedAt - updated > maximumAge)
    {
        throw std::runtime_error(std::string{asset} + " price is stale");
    }
}

PositionRisk RiskCalculator::Evaluate(
    const Position& position,
    const MarketSnapshot& market,
    std::uint64_t evaluatedAt
)
{
    if(position.usdcDebt.IsZero())
    {
        return PositionRisk{
            position,
            market.wethPrice.IsZero() ? Uint256{} : CollateralValue(position.wethCollateral, market.wethPrice),
            Uint256{},
            market.wethPrice.IsZero()
                ? Uint256{}
                : Uint256Math::MulDivDown(
                    CollateralValue(position.wethCollateral, market.wethPrice), LtvBps(), Bps()
                ),
            MaximumUint256(),
            false,
            market.indexedBlock,
            market.indexedBlockHash
        };
    }

    RequireFreshPrice(
        market.wethPrice,
        market.wethPriceUpdatedAt,
        market.wethMaxPriceAge,
        evaluatedAt,
        "WETH"
    );
    RequireFreshPrice(
        market.usdcPrice,
        market.usdcPriceUpdatedAt,
        market.usdcMaxPriceAge,
        evaluatedAt,
        "USDC"
    );

    const auto collateralValue = CollateralValue(position.wethCollateral, market.wethPrice);
    const auto debtValue = DebtValue(position.usdcDebt, market.usdcPrice);
    const auto maxBorrow = Uint256Math::MulDivDown(collateralValue, LtvBps(), Bps());
    const auto adjustedCollateral = Uint256Math::MulDivDown(
        collateralValue,
        LiquidationThresholdBps(),
        Bps()
    );
    const auto healthFactor = Uint256Math::MulDivDown(adjustedCollateral, Wad(), debtValue);

    return PositionRisk{
        position,
        collateralValue,
        debtValue,
        maxBorrow,
        healthFactor,
        healthFactor < Wad(),
        market.indexedBlock,
        market.indexedBlockHash
    };
}

PositionRisk RiskCalculator::EvaluateIndexed(
    const IndexedPosition& position,
    const MarketSnapshot& market,
    std::uint64_t evaluatedAt
)
{
    return Evaluate(
        Position{
            position.user,
            position.wethCollateral,
            DebtAtIndex(position.scaledUsdcDebt, ProjectedBorrowIndex(market, evaluatedAt))
        },
        market,
        evaluatedAt
    );
}

std::optional<LiquidationCandidate> RiskCalculator::BuildCandidate(
    const PositionRisk& risk,
    const MarketSnapshot& market,
    std::optional<Uint256> requestedRepay
)
{
    if(!risk.liquidatable || risk.position.usdcDebt.IsZero() || risk.position.wethCollateral.IsZero())
    {
        return std::nullopt;
    }

    const auto debt = risk.position.usdcDebt;
    const auto collateral = risk.position.wethCollateral;
    const auto requested = requestedRepay.value_or(debt);
    if(requested.IsZero())
    {
        return std::nullopt;
    }
    auto closeFactorLimit = Uint256Math::MulDivDown(debt, CloseFactorBps(), Bps());
    if(debt - closeFactorLimit < MinimumUsdcBorrow())
    {
        closeFactorLimit = debt;
    }

    const auto collateralLimitedValue = Uint256Math::MulDivDown(
        risk.collateralValue,
        Bps(),
        Bps() + LiquidationBonusBps()
    );
    const auto valueAtTokenPrecision = Uint256Math::MulDivDown(
        collateralLimitedValue,
        Uint256{1},
        market.usdcPrice
    );
    const auto collateralLimitedRepay = Uint256Math::MulDivDown(
        valueAtTokenPrecision,
        Uint256{1},
        Bps()
    );
    auto repay = Minimum(requested, Minimum(closeFactorLimit, collateralLimitedRepay));
    const bool collateralLimited = collateralLimitedRepay <= requested
        && collateralLimitedRepay <= closeFactorLimit;

    const auto remainingDebt = debt - repay;
    if(!collateralLimited && !remainingDebt.IsZero() && remainingDebt < MinimumUsdcBorrow())
    {
        repay = debt - MinimumUsdcBorrow();
    }
    if(repay.IsZero())
    {
        return std::nullopt;
    }

    const auto collateralValueWithBonus = Uint256Math::MulDivDown(
        DebtValue(repay, market.usdcPrice),
        Bps() + LiquidationBonusBps(),
        Bps()
    );
    auto collateralSeized = Uint256Math::MulDivDown(
        collateralValueWithBonus,
        OracleScale(),
        market.wethPrice
    );
    if(collateralLimited)
    {
        collateralSeized = collateral;
    }
    if(collateralSeized.IsZero())
    {
        return std::nullopt;
    }

    const auto seizedValue = CollateralValue(collateralSeized, market.wethPrice);
    const auto repaidValue = DebtValue(repay, market.usdcPrice);
    const auto bonus = seizedValue > repaidValue ? seizedValue - repaidValue : Uint256{};
    const auto badDebt = collateralLimited && debt > repay ? debt - repay : Uint256{};

    return LiquidationCandidate{
        risk.position.user,
        risk.healthFactor,
        market.usdc,
        market.weth,
        repay,
        bonus,
        collateralSeized,
        badDebt,
        risk.blockNumber,
        risk.blockHash
    };
}

}
