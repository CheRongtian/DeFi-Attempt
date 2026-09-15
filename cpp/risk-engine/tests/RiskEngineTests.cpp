#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "RiskTestData.hpp"
#include "dlp/risk/RiskEngine.hpp"

namespace dlp::risk
{

namespace
{

class MemoryRiskRepository final : public RiskRepository
{
public:
    MarketSnapshot market = test::Market();
    std::vector<IndexedPosition> positions;

    [[nodiscard]] MarketSnapshot LoadMarket(const ethereum::Uint256&) const override
    {
        return market;
    }

    [[nodiscard]] std::optional<IndexedPosition> LoadPosition(
        const ethereum::Uint256&,
        const ethereum::Address& user
    ) const override
    {
        for(const auto& position : positions)
        {
            if(position.user == user)
            {
                return position;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::vector<IndexedPosition> LoadPositions(
        const ethereum::Uint256&,
        std::size_t limit
    ) const override
    {
        const auto count = std::min(limit, positions.size());
        return {positions.begin(), positions.begin() + static_cast<std::ptrdiff_t>(count)};
    }
};

}

TEST(RiskEngineTests, ReadsOnePositionAndScansLiquidationCandidates)
{
    MemoryRiskRepository repository;
    repository.market.wethPrice = ethereum::Uint256{100'000'000'000};
    repository.positions.push_back(IndexedPosition{
        test::Address(5),
        ethereum::Uint256::FromDecimal("10000000000000000000"),
        ethereum::Uint256::FromDecimal("10000000000")
    });
    const RiskEngine engine{repository, ethereum::Uint256{31337}};

    const auto position = engine.Evaluate(test::Address(5), 1'700'000'000);
    ASSERT_TRUE(position.has_value());
    EXPECT_TRUE(position->liquidatable);

    const auto candidates = engine.Scan(10, 1'700'000'000);
    ASSERT_EQ(candidates.size(), 1U);
    EXPECT_EQ(candidates.front().borrower, test::Address(5));
}

}
