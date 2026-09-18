#include <optional>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "dlp/api/ApiService.hpp"
#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::api
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
        ethereum::Uint256{300'000'000'000},
        ethereum::Uint256{1},
        ethereum::Uint256{4'000'000'000},
        ethereum::Uint256{100'000'000},
        ethereum::Uint256{1},
        ethereum::Uint256{4'000'000'000},
        1,
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

class MemoryApiStore final : public ApiStore
{
public:
    [[nodiscard]] std::vector<LiquidationRecord> LoadLiquidations(
        const ethereum::Uint256&,
        std::size_t
    ) const override
    {
        return {};
    }
    [[nodiscard]] ProtocolStats LoadProtocolStats(const ethereum::Uint256&) const override
    {
        return ProtocolStats{1, 0, ethereum::Uint256{1}, ethereum::Uint256{2}, ethereum::Uint256{3}, {}, {}};
    }
    [[nodiscard]] ethereum::Uint256 LoadUsdcSupply(
        const ethereum::Uint256&,
        const ethereum::Address&
    ) const override
    {
        return ethereum::Uint256{7'000'000};
    }
};

class MemoryChain final : public ApiChain
{
public:
    [[nodiscard]] std::uint64_t GetBlockNumber() const override { return 12; }
};

class FailingChain final : public ApiChain
{
public:
    [[nodiscard]] std::uint64_t GetBlockNumber() const override
    {
        throw std::runtime_error("internal RPC failure detail");
    }
};

}

TEST(ApiServiceTests, ServesEveryConfiguredGetRoute)
{
    MemoryRiskRepository repository;
    risk::RiskEngine riskEngine{repository, ethereum::Uint256{31337}};
    MemoryApiStore store;
    MemoryChain chain;
    const ApiService service{riskEngine, store, chain, ethereum::Uint256{31337}};

    const std::vector<std::string> targets{
        "/markets",
        "/positions/" + Address(5).ToHex(),
        "/positions/" + Address(5).ToHex() + "/health",
        "/liquidations",
        "/protocol/stats"
    };
    for(const auto& target : targets)
    {
        const auto response = service.Handle(ApiRequest{"GET", target, {}});
        EXPECT_EQ(response.status, 200U) << target;
        EXPECT_TRUE(nlohmann::json::parse(response.body).contains("data")) << target;
    }
}

TEST(ApiServiceTests, SimulatesRiskWithDeterministicIntegerInputs)
{
    MemoryRiskRepository repository;
    risk::RiskEngine riskEngine{repository, ethereum::Uint256{31337}};
    MemoryApiStore store;
    MemoryChain chain;
    const ApiService service{riskEngine, store, chain, ethereum::Uint256{31337}};
    const nlohmann::json body{
        {"address", Address(5).ToHex()},
        {"wethCollateral", "10000000000000000000"},
        {"usdcDebt", "10000000000"},
        {"wethPrice", "100000000000"},
        {"usdcPrice", "100000000"}
    };

    const auto response = service.Handle(ApiRequest{"POST", "/risk/simulate", body.dump()});
    ASSERT_EQ(response.status, 200U);
    const auto result = nlohmann::json::parse(response.body);
    EXPECT_EQ(result.at("data").at("healthFactorWad"), "800000000000000000");
}

TEST(ApiServiceTests, IncludesMarketConfigurationAndUsdcSupplyPosition)
{
    MemoryRiskRepository repository;
    risk::RiskEngine riskEngine{repository, ethereum::Uint256{31337}};
    MemoryApiStore store;
    MemoryChain chain;
    const ApiService service{riskEngine, store, chain, ethereum::Uint256{31337}};

    const auto marketResponse = service.Handle(ApiRequest{"GET", "/markets", {}});
    ASSERT_EQ(marketResponse.status, 200U);
    const auto market = nlohmann::json::parse(marketResponse.body).at("data").front();
    EXPECT_EQ(market.at("ltvBps"), "7500");
    EXPECT_EQ(market.at("liquidationThresholdBps"), "8000");

    const auto positionResponse = service.Handle(ApiRequest{
        "GET",
        "/positions/" + Address(5).ToHex(),
        {}
    });
    ASSERT_EQ(positionResponse.status, 200U);
    const auto position = nlohmann::json::parse(positionResponse.body).at("data");
    EXPECT_EQ(position.at("usdcSupply"), "7000000");
}

TEST(ApiServiceTests, DoesNotExposeInternalFailureDetails)
{
    MemoryRiskRepository repository;
    risk::RiskEngine riskEngine{repository, ethereum::Uint256{31337}};
    MemoryApiStore store;
    FailingChain chain;
    const ApiService service{riskEngine, store, chain, ethereum::Uint256{31337}};

    const auto response = service.Handle(ApiRequest{"GET", "/markets", {}});

    ASSERT_EQ(response.status, 500U);
    EXPECT_EQ(nlohmann::json::parse(response.body).at("error"), "internal server error");
    EXPECT_EQ(response.body.find("internal RPC failure detail"), std::string::npos);
}

}
