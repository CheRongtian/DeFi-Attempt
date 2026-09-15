#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "RiskTestData.hpp"
#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::risk
{

TEST(RiskGoldenTests, MatchesAllSolidityPositionVectors)
{
    std::ifstream input{DLP_GOLDEN_FILE};
    ASSERT_TRUE(input.good());
    const auto vectors = nlohmann::json::parse(input);

    for(const auto& [name, vector] : vectors.at("risk_position").items())
    {
        auto market = test::Market();
        market.wethPrice = ethereum::Uint256::FromDecimal(vector.at("weth_price").get<std::string>());
        market.usdcPrice = ethereum::Uint256::FromDecimal(vector.at("usdc_price").get<std::string>());
        const Position position{
            test::Address(5),
            ethereum::Uint256::FromDecimal(vector.at("collateral_amount").get<std::string>()),
            ethereum::Uint256::FromDecimal(vector.at("debt_amount").get<std::string>())
        };
        const auto actual = RiskCalculator::Evaluate(position, market, 1'700'000'000);

        EXPECT_EQ(actual.collateralValue.ToDecimal(), vector.at("expected_collateral_value").get<std::string>()) << name;
        EXPECT_EQ(actual.debtValue.ToDecimal(), vector.at("expected_debt_value").get<std::string>()) << name;
        EXPECT_EQ(actual.maxBorrow.ToDecimal(), vector.at("expected_max_borrow").get<std::string>()) << name;
        EXPECT_EQ(actual.healthFactor.ToDecimal(), vector.at("expected_health_factor").get<std::string>()) << name;
    }
}

TEST(RiskGoldenTests, MatchesSolidityProjectedBorrowIndices)
{
    std::ifstream input{DLP_GOLDEN_FILE};
    ASSERT_TRUE(input.good());
    const auto vectors = nlohmann::json::parse(input);

    for(const auto& [name, vector] : vectors.at("interest_index").items())
    {
        auto market = test::Market(1'000'000);
        market.availableUsdcLiquidity = ethereum::Uint256::FromDecimal("20000000000");
        market.totalScaledUsdcDebt = ethereum::Uint256::FromDecimal("80000000000");
        const auto elapsed = ethereum::Uint256::FromDecimal(
            vector.at("elapsed_seconds").get<std::string>()
        ).ToUint64();
        EXPECT_EQ(
            RiskCalculator::ProjectedBorrowIndex(market, 1'000'000 + elapsed).ToDecimal(),
            vector.at("expected_borrow_index").get<std::string>()
        ) << name;
    }
}

TEST(RiskGoldenTests, MatchesAllSolidityLiquidationVectors)
{
    std::ifstream input{DLP_GOLDEN_FILE};
    ASSERT_TRUE(input.good());
    const auto vectors = nlohmann::json::parse(input);

    for(const auto& [name, vector] : vectors.at("liquidation").items())
    {
        auto market = test::Market();
        market.wethPrice = ethereum::Uint256::FromDecimal(
            vector.at("liquidation_weth_price").get<std::string>()
        );
        const Position position{
            test::Address(5),
            ethereum::Uint256::FromDecimal(vector.at("collateral_amount").get<std::string>()),
            ethereum::Uint256::FromDecimal(vector.at("debt_amount").get<std::string>())
        };
        const auto risk = RiskCalculator::Evaluate(position, market, 1'700'000'000);
        const auto candidate = RiskCalculator::BuildCandidate(
            risk,
            market,
            ethereum::Uint256::FromDecimal(vector.at("requested_repay").get<std::string>())
        );

        ASSERT_TRUE(candidate.has_value()) << name;
        EXPECT_EQ(candidate->maxRepay.ToDecimal(), vector.at("expected_repaid").get<std::string>()) << name;
        EXPECT_EQ(
            candidate->expectedCollateral.ToDecimal(),
            vector.at("expected_collateral_seized").get<std::string>()
        ) << name;
        EXPECT_EQ(candidate->expectedBadDebt.ToDecimal(), vector.at("expected_bad_debt").get<std::string>()) << name;
    }
}

}
