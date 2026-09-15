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

class ScaleRepository final : public RiskRepository
{
public:
    explicit ScaleRepository(std::size_t size)
        : market_(test::Market())
    {
        market_.wethPrice = ethereum::Uint256{100'000'000'000};
        positions_.reserve(size);
        for(std::size_t index = 0; index < size; ++index)
        {
            ethereum::Address::Storage bytes{};
            bytes[12] = static_cast<std::uint8_t>((index >> 56U) & 0xffU);
            bytes[13] = static_cast<std::uint8_t>((index >> 48U) & 0xffU);
            bytes[14] = static_cast<std::uint8_t>((index >> 40U) & 0xffU);
            bytes[15] = static_cast<std::uint8_t>((index >> 32U) & 0xffU);
            bytes[16] = static_cast<std::uint8_t>((index >> 24U) & 0xffU);
            bytes[17] = static_cast<std::uint8_t>((index >> 16U) & 0xffU);
            bytes[18] = static_cast<std::uint8_t>((index >> 8U) & 0xffU);
            bytes[19] = static_cast<std::uint8_t>(index & 0xffU);
            positions_.push_back(IndexedPosition{
                ethereum::Address{bytes},
                ethereum::Uint256::FromDecimal("10000000000000000000"),
                ethereum::Uint256::FromDecimal("10000000000")
            });
        }
    }

    [[nodiscard]] MarketSnapshot LoadMarket(const ethereum::Uint256&) const override
    {
        return market_;
    }

    [[nodiscard]] std::optional<IndexedPosition> LoadPosition(
        const ethereum::Uint256&,
        const ethereum::Address&
    ) const override
    {
        return std::nullopt;
    }

    [[nodiscard]] std::vector<IndexedPosition> LoadPositions(
        const ethereum::Uint256&,
        std::size_t limit
    ) const override
    {
        const auto count = std::min(limit, positions_.size());
        return {positions_.begin(), positions_.begin() + static_cast<std::ptrdiff_t>(count)};
    }

private:
    MarketSnapshot market_;
    std::vector<IndexedPosition> positions_;
};

}

class RiskScaleTests : public testing::TestWithParam<std::size_t>
{
};

TEST_P(RiskScaleTests, ScansTheConfiguredPositionCount)
{
    ScaleRepository repository{GetParam()};
    const RiskEngine engine{repository, ethereum::Uint256{31337}};
    EXPECT_EQ(engine.Scan(GetParam(), 1'700'000'000).size(), GetParam());
}

INSTANTIATE_TEST_SUITE_P(PositionCounts, RiskScaleTests, testing::Values(10U, 1'000U, 10'000U, 100'000U));

}
