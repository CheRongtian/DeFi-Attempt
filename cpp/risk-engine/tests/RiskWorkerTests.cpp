#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "RiskTestData.hpp"
#include "dlp/risk/RiskWorker.hpp"

namespace dlp::risk
{

namespace
{

class MemoryRiskRepository final : public RiskRepository
{
public:
    MarketSnapshot market = test::Market();
    std::vector<IndexedPosition> positions{
        IndexedPosition{
            test::Address(5),
            ethereum::Uint256::FromDecimal("10000000000000000000"),
            ethereum::Uint256::FromDecimal("25000000000")
        }
    };

    [[nodiscard]] MarketSnapshot LoadMarket(const ethereum::Uint256&) const override
    {
        return market;
    }

    [[nodiscard]] std::optional<IndexedPosition> LoadPosition(
        const ethereum::Uint256&,
        const ethereum::Address&
    ) const override
    {
        return positions.front();
    }

    [[nodiscard]] std::vector<IndexedPosition> LoadPositions(
        const ethereum::Uint256&,
        std::size_t
    ) const override
    {
        return positions;
    }
};

class MemoryEventStore final : public RiskEventStore
{
public:
    std::string sourceEventId;
    std::vector<LiquidationCandidate> candidates;

    bool CommitScan(
        std::string_view eventId,
        const std::vector<LiquidationCandidate>& values
    ) override
    {
        sourceEventId = eventId;
        candidates = values;
        return true;
    }
};

}

TEST(RiskWorkerTests, TurnsAnIndexedBlockIntoLiquidationEvents)
{
    MemoryRiskRepository repository;
    RiskEngine engine{repository, ethereum::Uint256{31337}};
    MemoryEventStore store;
    RiskWorker worker{engine, store, 100};

    EXPECT_TRUE(worker.Process("chain.block.processed:31337:block", 1'700'000'000));
    EXPECT_EQ(store.sourceEventId, "chain.block.processed:31337:block");
    ASSERT_EQ(store.candidates.size(), 1U);
    EXPECT_EQ(store.candidates.front().canonicalVersion, repository.market.canonicalVersion);
}

TEST(RiskWorkerTests, GivesEachPeriodicRescanItsOwnSourceEvent)
{
    MemoryRiskRepository repository;
    RiskEngine engine{repository, ethereum::Uint256{31337}};
    MemoryEventStore store;
    RiskWorker worker{engine, store, 100};

    EXPECT_TRUE(worker.Rescan(1'700'000'000));
    EXPECT_EQ(store.sourceEventId, "risk.rescan:31337:1:1700000000");
    EXPECT_TRUE(worker.Rescan(1'700'000'030));
    EXPECT_EQ(store.sourceEventId, "risk.rescan:31337:1:1700000030");
}

}
