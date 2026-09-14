#include "dlp/indexer/StateProjector.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <variant>

#include "dlp/ethereum/Uint256Math.hpp"

namespace dlp::indexer
{

namespace
{

const ethereum::Uint256& Ray()
{
    static const auto value = ethereum::Uint256::FromDecimal("1000000000000000000000000000");
    return value;
}

template<typename Value>
const Value& EventValue(const DecodedProtocolEvent& event, std::size_t index)
{
    return std::get<Value>(event.values.at(index));
}

PositionState& GetPosition(DerivedState& state, const ethereum::Address& user)
{
    const auto iterator = state.positions.try_emplace(
        user,
        PositionState{user, ethereum::Uint256{}, ethereum::Uint256{}, ethereum::Uint256{}}
    ).first;
    return iterator->second;
}

void RemoveEmptyPosition(DerivedState& state, const ethereum::Address& user)
{
    const auto iterator = state.positions.find(user);
    if(iterator != state.positions.end()
        && iterator->second.wethCollateral.IsZero()
        && iterator->second.scaledUsdcSupply.IsZero()
        && iterator->second.scaledUsdcDebt.IsZero())
    {
        state.positions.erase(iterator);
    }
}

ethereum::Uint256 SupplyAtIndex(
    const ethereum::Uint256& scaledAmount,
    const ethereum::Uint256& index
)
{
    return ethereum::Uint256Math::MulDivDown(scaledAmount, index, Ray());
}

ethereum::Uint256 DebtAtIndex(
    const ethereum::Uint256& scaledAmount,
    const ethereum::Uint256& index
)
{
    return ethereum::Uint256Math::MulDivUp(scaledAmount, index, Ray());
}

}

StateProjector::StateProjector(ChainContracts contracts)
    : contracts_(std::move(contracts)), decoder_(contracts_)
{
}

DerivedState StateProjector::CreateInitialState() const
{
    DerivedState state;
    state.market.contracts = contracts_;
    state.market.borrowIndex = Ray();
    state.market.liquidityIndex = Ray();
    return state;
}

DerivedState StateProjector::Rebuild(const std::vector<RawLog>& logs) const
{
    auto state = CreateInitialState();
    std::vector<const RawLog*> orderedLogs;
    orderedLogs.reserve(logs.size());
    for(const auto& log : logs)
    {
        if(log.canonical)
        {
            orderedLogs.push_back(&log);
        }
    }

    std::sort(
        orderedLogs.begin(),
        orderedLogs.end(),
        [](const RawLog* left, const RawLog* right) {
            if(left->blockNumber != right->blockNumber)
            {
                return left->blockNumber < right->blockNumber;
            }
            if(left->transactionIndex != right->transactionIndex)
            {
                return left->transactionIndex < right->transactionIndex;
            }
            return left->logIndex < right->logIndex;
        }
    );

    for(const auto* log : orderedLogs)
    {
        Apply(state, *log);
    }
    return state;
}

void StateProjector::Apply(DerivedState& state, const RawLog& log) const
{
    const auto event = decoder_.Decode(log);
    if(event.has_value())
    {
        ApplyEvent(state, *event);
    }
}

void StateProjector::ApplyEvent(DerivedState& state, const DecodedProtocolEvent& event) const
{
    using ethereum::Address;
    using ethereum::ProtocolEventKind;
    using ethereum::Uint256;
    using ethereum::Uint256Math;

    auto& market = state.market;

    switch(event.kind)
    {
        case ProtocolEventKind::Supplied:
        {
            const auto& user = EventValue<Address>(event, 0);
            const auto& asset = EventValue<Address>(event, 1);
            const auto& amount = EventValue<Uint256>(event, 2);
            auto& position = GetPosition(state, user);

            if(asset == contracts_.weth)
            {
                position.wethCollateral += amount;
                market.totalWethCollateral += amount;
            }
            else if(asset == contracts_.usdc)
            {
                const auto supplyBefore = SupplyAtIndex(
                    market.totalScaledUsdcSupply,
                    market.liquidityIndex
                );
                const auto scaledMint = Uint256Math::MulDivDown(
                    amount,
                    Ray(),
                    market.liquidityIndex
                );
                position.scaledUsdcSupply += scaledMint;
                market.totalScaledUsdcSupply += scaledMint;
                const auto supplyAfter = SupplyAtIndex(
                    market.totalScaledUsdcSupply,
                    market.liquidityIndex
                );
                market.protocolReserve += amount - (supplyAfter - supplyBefore);
                market.availableUsdcLiquidity += amount;
            }
            break;
        }
        case ProtocolEventKind::Withdrawn:
        {
            const auto& user = EventValue<Address>(event, 0);
            const auto& asset = EventValue<Address>(event, 1);
            const auto& amount = EventValue<Uint256>(event, 2);
            auto& position = GetPosition(state, user);

            if(asset == contracts_.weth)
            {
                position.wethCollateral -= amount;
                market.totalWethCollateral -= amount;
            }
            else if(asset == contracts_.usdc)
            {
                const auto supplied = SupplyAtIndex(
                    position.scaledUsdcSupply,
                    market.liquidityIndex
                );
                const auto scaledBurn = amount == supplied
                    ? position.scaledUsdcSupply
                    : Uint256Math::MulDivUp(amount, Ray(), market.liquidityIndex);
                const auto supplyBefore = SupplyAtIndex(
                    market.totalScaledUsdcSupply,
                    market.liquidityIndex
                );
                position.scaledUsdcSupply -= scaledBurn;
                market.totalScaledUsdcSupply -= scaledBurn;
                const auto supplyAfter = SupplyAtIndex(
                    market.totalScaledUsdcSupply,
                    market.liquidityIndex
                );
                market.protocolReserve += (supplyBefore - supplyAfter) - amount;
                market.availableUsdcLiquidity -= amount;
            }
            RemoveEmptyPosition(state, user);
            break;
        }
        case ProtocolEventKind::Borrowed:
        {
            const auto& user = EventValue<Address>(event, 0);
            const auto& asset = EventValue<Address>(event, 1);
            const auto& amount = EventValue<Uint256>(event, 2);
            if(asset != contracts_.usdc)
            {
                break;
            }

            auto& position = GetPosition(state, user);
            const auto debtBefore = DebtAtIndex(position.scaledUsdcDebt, market.borrowIndex);
            const auto scaledMint = Uint256Math::MulDivUp(amount, Ray(), market.borrowIndex);
            position.scaledUsdcDebt += scaledMint;
            market.totalScaledUsdcDebt += scaledMint;
            const auto debtAfter = DebtAtIndex(position.scaledUsdcDebt, market.borrowIndex);
            market.protocolReserve += (debtAfter - debtBefore) - amount;
            market.availableUsdcLiquidity -= amount;
            break;
        }
        case ProtocolEventKind::Repaid:
        {
            const auto& user = EventValue<Address>(event, 0);
            const auto& asset = EventValue<Address>(event, 1);
            const auto& repaidAmount = EventValue<Uint256>(event, 2);
            const auto& remainingDebt = EventValue<Uint256>(event, 3);
            if(asset != contracts_.usdc)
            {
                break;
            }

            auto& position = GetPosition(state, user);
            const auto currentDebt = DebtAtIndex(position.scaledUsdcDebt, market.borrowIndex);
            const auto scaledBurn = remainingDebt.IsZero()
                ? position.scaledUsdcDebt
                : Uint256Math::MulDivDown(repaidAmount, Ray(), market.borrowIndex);
            position.scaledUsdcDebt -= scaledBurn;
            market.totalScaledUsdcDebt -= scaledBurn;
            const auto projectedRemaining = DebtAtIndex(position.scaledUsdcDebt, market.borrowIndex);
            if(projectedRemaining != remainingDebt)
            {
                throw std::runtime_error("Repaid event does not match reconstructed debt");
            }
            market.protocolReserve += repaidAmount - (currentDebt - remainingDebt);
            market.availableUsdcLiquidity += repaidAmount;
            RemoveEmptyPosition(state, user);
            break;
        }
        case ProtocolEventKind::InterestAccrued:
        {
            market.lastInterestTimestamp = EventValue<Uint256>(event, 0);
            market.borrowIndex = EventValue<Uint256>(event, 1);
            market.liquidityIndex = EventValue<Uint256>(event, 2);
            market.protocolReserve += EventValue<Uint256>(event, 3);
            break;
        }
        case ProtocolEventKind::Liquidated:
        {
            const auto& liquidator = EventValue<Address>(event, 0);
            const auto& borrower = EventValue<Address>(event, 1);
            const auto& debtAsset = EventValue<Address>(event, 2);
            const auto& collateralAsset = EventValue<Address>(event, 3);
            const auto& repaidAmount = EventValue<Uint256>(event, 4);
            const auto& collateralSeized = EventValue<Uint256>(event, 5);
            auto& position = GetPosition(state, borrower);

            const auto currentDebt = DebtAtIndex(position.scaledUsdcDebt, market.borrowIndex);
            const auto scaledBurn = repaidAmount >= currentDebt
                ? position.scaledUsdcDebt
                : Uint256Math::MulDivDown(repaidAmount, Ray(), market.borrowIndex);
            position.scaledUsdcDebt -= scaledBurn;
            market.totalScaledUsdcDebt -= scaledBurn;
            const auto remainingDebt = DebtAtIndex(position.scaledUsdcDebt, market.borrowIndex);
            market.protocolReserve += repaidAmount - (currentDebt - remainingDebt);
            market.availableUsdcLiquidity += repaidAmount;
            position.wethCollateral -= collateralSeized;
            market.totalWethCollateral -= collateralSeized;

            state.liquidations.push_back(LiquidationState{
                event.source.chainId,
                event.source.blockNumber,
                event.source.blockHash,
                event.source.transactionHash,
                event.source.logIndex,
                liquidator,
                borrower,
                debtAsset,
                collateralAsset,
                repaidAmount,
                collateralSeized
            });
            RemoveEmptyPosition(state, borrower);
            break;
        }
        case ProtocolEventKind::BadDebtRecognized:
        {
            const auto& borrower = EventValue<Address>(event, 0);
            const auto& amount = EventValue<Uint256>(event, 1);
            auto& position = GetPosition(state, borrower);
            market.totalScaledUsdcDebt -= position.scaledUsdcDebt;
            position.scaledUsdcDebt = Uint256{};
            market.badDebt += amount;
            RemoveEmptyPosition(state, borrower);
            break;
        }
        case ProtocolEventKind::PriceUpdated:
        {
            const auto& asset = EventValue<Address>(event, 0);
            const auto& price = EventValue<Uint256>(event, 1);
            const auto& updatedAt = EventValue<Uint256>(event, 2);
            if(asset == contracts_.weth)
            {
                market.wethPrice = price;
                market.wethPriceUpdatedAt = updatedAt;
            }
            else if(asset == contracts_.usdc)
            {
                market.usdcPrice = price;
                market.usdcPriceUpdatedAt = updatedAt;
            }
            break;
        }
        case ProtocolEventKind::AssetRegistered:
        {
            const auto& asset = EventValue<Address>(event, 0);
            const auto& maxPriceAge = EventValue<Uint256>(event, 1);
            if(asset == contracts_.weth)
            {
                market.wethMaxPriceAge = maxPriceAge;
            }
            else if(asset == contracts_.usdc)
            {
                market.usdcMaxPriceAge = maxPriceAge;
            }
            break;
        }
    }
}

}
