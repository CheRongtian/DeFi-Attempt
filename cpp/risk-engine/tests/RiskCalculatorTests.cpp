#include <gtest/gtest.h>

#include "RiskTestData.hpp"
#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::risk
{

TEST(RiskCalculatorTests, MatchesTheStandardSolidityRiskPosition)
{
    const auto market = test::Market();
    const Position position{
        test::Address(5),
        ethereum::Uint256::FromDecimal("10000000000000000000"),
        ethereum::Uint256::FromDecimal("10000000000")
    };
    const auto result = RiskCalculator::Evaluate(position, market, 1'700'000'000);

    EXPECT_EQ(result.collateralValue.ToDecimal(), "30000000000000000000000");
    EXPECT_EQ(result.debtValue.ToDecimal(), "10000000000000000000000");
    EXPECT_EQ(result.maxBorrow.ToDecimal(), "22500000000000000000000");
    EXPECT_EQ(result.healthFactor.ToDecimal(), "2400000000000000000");
    EXPECT_FALSE(result.liquidatable);
}

TEST(RiskCalculatorTests, GivesZeroDebtTheMaximumHealthFactor)
{
    auto market = test::Market();
    market.wethPrice = ethereum::Uint256{};
    const Position position{test::Address(5), ethereum::Uint256{}, ethereum::Uint256{}};
    const auto result = RiskCalculator::Evaluate(position, market, 1'700'000'000);
    EXPECT_EQ(result.healthFactor, RiskCalculator::MaximumUint256());
    EXPECT_FALSE(result.liquidatable);
}

TEST(RiskCalculatorTests, RejectsStalePricesForDebtPositions)
{
    const auto market = test::Market(1'600'000'000);
    const Position position{test::Address(5), ethereum::Uint256{1}, ethereum::Uint256{1}};
    EXPECT_THROW(
        [&] { return RiskCalculator::Evaluate(position, market, 1'700'000'000); }(),
        std::runtime_error
    );
}

TEST(RiskCalculatorTests, BuildsTheSolidityCloseFactorCandidate)
{
    auto market = test::Market();
    market.wethPrice = ethereum::Uint256{100'000'000'000};
    const Position position{
        test::Address(5),
        ethereum::Uint256::FromDecimal("10000000000000000000"),
        ethereum::Uint256::FromDecimal("10000000000")
    };
    const auto risk = RiskCalculator::Evaluate(position, market, 1'700'000'000);
    const auto candidate = RiskCalculator::BuildCandidate(risk, market);
    ASSERT_TRUE(candidate.has_value());
    EXPECT_EQ(candidate->maxRepay.ToDecimal(), "5000000000");
    EXPECT_EQ(candidate->healthFactor.ToDecimal(), "800000000000000000");
}

}
