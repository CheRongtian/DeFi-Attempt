#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "dlp/liquidator/Liquidator.hpp"
#include "dlp/risk/RiskEngine.hpp"

namespace dlp::liquidator
{

namespace
{

ethereum::Address Address(std::uint8_t suffix)
{
    ethereum::Address::Storage bytes{};
    bytes.back() = suffix;
    return ethereum::Address{bytes};
}

risk::MarketSnapshot Market()
{
    ethereum::Hash256 hash{};
    hash.back() = 1;
    return risk::MarketSnapshot{
        ethereum::Uint256{31337},
        10,
        hash,
        Address(1),
        Address(2),
        Address(3),
        Address(4),
        risk::RiskCalculator::Ray(),
        ethereum::Uint256{100'000'000'000},
        ethereum::Uint256{1'700'000'000},
        ethereum::Uint256{86'400},
        ethereum::Uint256{100'000'000},
        ethereum::Uint256{1'700'000'000},
        ethereum::Uint256{86'400},
        1'700'000'000,
        ethereum::Uint256{},
        ethereum::Uint256{},
        ethereum::Uint256{},
        1
    };
}

class MemoryRiskRepository final : public risk::RiskRepository
{
public:
    risk::MarketSnapshot market = Market();
    risk::IndexedPosition position{
        Address(5),
        ethereum::Uint256::FromDecimal("10000000000000000000"),
        ethereum::Uint256::FromDecimal("10000000000")
    };

    [[nodiscard]] risk::MarketSnapshot LoadMarket(const ethereum::Uint256&) const override { return market; }
    [[nodiscard]] std::optional<risk::IndexedPosition> LoadPosition(
        const ethereum::Uint256&,
        const ethereum::Address& user
    ) const override
    {
        return user == position.user ? std::optional<risk::IndexedPosition>{position} : std::nullopt;
    }
    [[nodiscard]] std::vector<risk::IndexedPosition> LoadPositions(
        const ethereum::Uint256&,
        std::size_t
    ) const override
    {
        return {position};
    }
};

class MemoryChain final : public LiquidationChain
{
public:
    risk::MarketSnapshot market = Market();
    risk::Position position{
        Address(5),
        ethereum::Uint256::FromDecimal("10000000000000000000"),
        ethereum::Uint256::FromDecimal("10000000000")
    };
    ethereum::Uint256 gasLimit{100'000};

    [[nodiscard]] risk::MarketSnapshot LoadMarket() const override { return market; }
    [[nodiscard]] risk::Position LoadPosition(const ethereum::Address&) const override { return position; }
    [[nodiscard]] ethereum::Uint256 EstimateGas(
        const ethereum::Address&,
        const ethereum::Address&,
        const ethereum::Bytes&
    ) const override
    {
        return gasLimit;
    }
    [[nodiscard]] tx::FeeQuote GetFeeQuote() const override
    {
        return tx::FeeQuote{ethereum::Uint256{1}, ethereum::Uint256{1}};
    }
};

}

TEST(LiquidatorTests, RevalidatesAndPreparesAProfitableCandidate)
{
    MemoryRiskRepository repository;
    risk::RiskEngine riskEngine{repository, ethereum::Uint256{31337}};
    MemoryChain chain;
    Liquidator liquidator{chain, LiquidatorConfig{Address(9), Address(8), ethereum::Uint256{}, 9'900}};
    const auto candidates = riskEngine.Scan(10, 1'700'000'000);

    ASSERT_EQ(candidates.size(), 1U);
    EXPECT_TRUE(liquidator.Prepare(candidates.front()).has_value());
}

TEST(LiquidatorTests, DropsACandidateThatIsHealthyOnTheLatestChain)
{
    MemoryRiskRepository repository;
    risk::RiskEngine riskEngine{repository, ethereum::Uint256{31337}};
    MemoryChain chain;
    chain.market.wethPrice = ethereum::Uint256{300'000'000'000};
    Liquidator liquidator{chain, LiquidatorConfig{Address(9), Address(8), ethereum::Uint256{}, 9'900}};
    const auto candidates = riskEngine.Scan(10, 1'700'000'000);

    ASSERT_EQ(candidates.size(), 1U);
    EXPECT_FALSE(liquidator.Prepare(candidates.front()).has_value());
}

}
