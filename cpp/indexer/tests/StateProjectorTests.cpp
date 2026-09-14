#include <gtest/gtest.h>

#include "dlp/indexer/StateProjector.hpp"
#include "TestData.hpp"

namespace dlp::indexer
{

TEST(StateProjectorTests, ReconstructsSupplyBorrowInterestRepayAndPrices)
{
    const auto contracts = test::Contracts();
    const auto lender = test::Address(10);
    const auto borrower = test::Address(11);
    const auto borrowIndex = ethereum::Uint256::FromDecimal("1100000000000000000000000000");
    const auto liquidityIndex = ethereum::Uint256::FromDecimal("1020000000000000000000000000");

    const std::vector<RawLog> logs{
        test::EventLog(
            ethereum::ProtocolEventKind::Supplied,
            contracts.pool,
            {lender, contracts.usdc, ethereum::Uint256{100}},
            1,
            0
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::Supplied,
            contracts.pool,
            {borrower, contracts.weth, ethereum::Uint256{10}},
            1,
            1
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::Borrowed,
            contracts.pool,
            {borrower, contracts.usdc, ethereum::Uint256{40}},
            1,
            2
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::InterestAccrued,
            contracts.pool,
            {ethereum::Uint256{20}, borrowIndex, liquidityIndex, ethereum::Uint256{2}},
            2,
            0,
            test::Hash(0, 2)
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::Repaid,
            contracts.pool,
            {borrower, contracts.usdc, ethereum::Uint256{22}, ethereum::Uint256{22}},
            2,
            1,
            test::Hash(0, 2)
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::AssetRegistered,
            contracts.oracle,
            {contracts.weth, ethereum::Uint256{3600}},
            2,
            2,
            test::Hash(0, 2)
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::PriceUpdated,
            contracts.oracle,
            {contracts.weth, ethereum::Uint256{300000000000ULL}, ethereum::Uint256{21}},
            2,
            3,
            test::Hash(0, 2)
        )
    };

    const auto state = StateProjector{contracts}.Rebuild(logs);

    EXPECT_EQ(state.positions.at(lender).scaledUsdcSupply, ethereum::Uint256{100});
    EXPECT_EQ(state.positions.at(borrower).wethCollateral, ethereum::Uint256{10});
    EXPECT_EQ(state.positions.at(borrower).scaledUsdcDebt, ethereum::Uint256{20});
    EXPECT_EQ(state.market.totalScaledUsdcSupply, ethereum::Uint256{100});
    EXPECT_EQ(state.market.totalScaledUsdcDebt, ethereum::Uint256{20});
    EXPECT_EQ(state.market.availableUsdcLiquidity, ethereum::Uint256{82});
    EXPECT_EQ(state.market.protocolReserve, ethereum::Uint256{2});
    EXPECT_EQ(state.market.borrowIndex, borrowIndex);
    EXPECT_EQ(state.market.liquidityIndex, liquidityIndex);
    EXPECT_EQ(state.market.wethPrice, ethereum::Uint256{300000000000ULL});
    EXPECT_EQ(state.market.wethPriceUpdatedAt, ethereum::Uint256{21});
    EXPECT_EQ(state.market.wethMaxPriceAge, ethereum::Uint256{3600});
}

TEST(StateProjectorTests, ReconstructsWithdrawalLiquidationAndBadDebt)
{
    const auto contracts = test::Contracts();
    const auto lender = test::Address(10);
    const auto borrower = test::Address(11);
    const auto liquidator = test::Address(12);
    const auto blockHash = test::Hash(0, 3);

    const std::vector<RawLog> logs{
        test::EventLog(
            ethereum::ProtocolEventKind::Supplied,
            contracts.pool,
            {lender, contracts.usdc, ethereum::Uint256{100}},
            1,
            0
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::Supplied,
            contracts.pool,
            {borrower, contracts.weth, ethereum::Uint256{10}},
            1,
            1
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::Borrowed,
            contracts.pool,
            {borrower, contracts.usdc, ethereum::Uint256{40}},
            2,
            0,
            test::Hash(0, 2)
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::Withdrawn,
            contracts.pool,
            {lender, contracts.usdc, ethereum::Uint256{25}},
            2,
            1,
            test::Hash(0, 2)
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::Liquidated,
            contracts.pool,
            {
                liquidator,
                borrower,
                contracts.usdc,
                contracts.weth,
                ethereum::Uint256{20},
                ethereum::Uint256{10}
            },
            3,
            0,
            blockHash
        ),
        test::EventLog(
            ethereum::ProtocolEventKind::BadDebtRecognized,
            contracts.pool,
            {borrower, ethereum::Uint256{20}},
            3,
            1,
            blockHash
        )
    };

    const auto state = StateProjector{contracts}.Rebuild(logs);

    EXPECT_EQ(state.positions.at(lender).scaledUsdcSupply, ethereum::Uint256{75});
    EXPECT_EQ(state.positions.count(borrower), 0U);
    EXPECT_EQ(state.market.totalWethCollateral, ethereum::Uint256{});
    EXPECT_EQ(state.market.totalScaledUsdcDebt, ethereum::Uint256{});
    EXPECT_EQ(state.market.availableUsdcLiquidity, ethereum::Uint256{55});
    EXPECT_EQ(state.market.badDebt, ethereum::Uint256{20});
    ASSERT_EQ(state.liquidations.size(), 1U);
    EXPECT_EQ(state.liquidations.front().liquidator, liquidator);
    EXPECT_EQ(state.liquidations.front().borrower, borrower);
    EXPECT_EQ(state.liquidations.front().repaidAmount, ethereum::Uint256{20});
    EXPECT_EQ(state.liquidations.front().collateralSeized, ethereum::Uint256{10});
}

TEST(StateProjectorTests, IgnoresNonCanonicalLogsDuringRebuild)
{
    const auto contracts = test::Contracts();
    auto orphan = test::EventLog(
        ethereum::ProtocolEventKind::PriceUpdated,
        contracts.oracle,
        {contracts.weth, ethereum::Uint256{100}, ethereum::Uint256{1}}
    );
    orphan.canonical = false;

    const auto state = StateProjector{contracts}.Rebuild({orphan});

    EXPECT_EQ(state.market.wethPrice, ethereum::Uint256{});
}

}
