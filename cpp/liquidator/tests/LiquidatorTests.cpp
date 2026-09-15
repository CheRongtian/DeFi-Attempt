#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "dlp/liquidator/Liquidator.hpp"

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
        ethereum::Uint256{}
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

class MemoryQueue final : public tx::TransactionQueue
{
public:
    std::size_t count{0};

    bool Queue(
        std::string,
        const ethereum::Address&,
        ethereum::Bytes,
        ethereum::Uint256
    ) override
    {
        ++count;
        return true;
    }
};

}

TEST(LiquidatorTests, RevalidatesAndQueuesAProfitableCandidate)
{
    MemoryRiskRepository repository;
    risk::RiskEngine riskEngine{repository, ethereum::Uint256{31337}};
    MemoryChain chain;
    MemoryQueue queue;
    Liquidator liquidator{
        riskEngine,
        chain,
        queue,
        LiquidatorConfig{Address(9), Address(8), ethereum::Uint256{}, 9'900}
    };

    EXPECT_EQ(liquidator.RunOnce(10, 1'700'000'000), 1U);
    EXPECT_EQ(queue.count, 1U);
}

TEST(LiquidatorTests, DropsACandidateThatIsHealthyOnTheLatestChain)
{
    MemoryRiskRepository repository;
    risk::RiskEngine riskEngine{repository, ethereum::Uint256{31337}};
    MemoryChain chain;
    chain.market.wethPrice = ethereum::Uint256{300'000'000'000};
    MemoryQueue queue;
    Liquidator liquidator{
        riskEngine,
        chain,
        queue,
        LiquidatorConfig{Address(9), Address(8), ethereum::Uint256{}, 9'900}
    };

    EXPECT_EQ(liquidator.RunOnce(10, 1'700'000'000), 0U);
    EXPECT_EQ(queue.count, 0U);
}

}
