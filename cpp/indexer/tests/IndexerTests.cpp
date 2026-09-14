#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dlp/indexer/Indexer.hpp"
#include "TestData.hpp"

namespace dlp::indexer
{

namespace
{

ethereum::BlockHeader Block(
    std::uint64_t number,
    const ethereum::Hash256& hash,
    const ethereum::Hash256& parentHash
)
{
    return ethereum::BlockHeader{ethereum::Uint256{number}, hash, parentHash};
}

class FakeChain final : public ChainClient
{
public:
    [[nodiscard]] ethereum::Uint256 GetChainId() const override
    {
        return ethereum::Uint256{31337};
    }

    [[nodiscard]] std::uint64_t GetBlockNumber() const override
    {
        return blocks.rbegin()->first;
    }

    [[nodiscard]] std::optional<ethereum::BlockHeader> GetBlockByNumber(
        std::uint64_t number
    ) const override
    {
        const auto iterator = blocks.find(number);
        if(iterator == blocks.end())
        {
            return std::nullopt;
        }
        return iterator->second;
    }

    [[nodiscard]] std::vector<ethereum::RpcLog> GetLogs(std::uint64_t blockNumber) const override
    {
        const auto iterator = logs.find(blockNumber);
        return iterator == logs.end() ? std::vector<ethereum::RpcLog>{} : iterator->second;
    }

    std::map<std::uint64_t, ethereum::BlockHeader> blocks;
    std::map<std::uint64_t, std::vector<ethereum::RpcLog>> logs;
};

ethereum::RpcLog ToRpcLog(const RawLog& log)
{
    return ethereum::RpcLog{
        log.contractAddress,
        log.topics,
        log.data,
        ethereum::Uint256{log.blockNumber},
        log.blockHash,
        log.transactionHash,
        ethereum::Uint256{log.transactionIndex},
        ethereum::Uint256{log.logIndex},
        false
    };
}

class MemoryStore final : public IndexerStore
{
public:
    explicit MemoryStore(const ChainContracts& contracts)
        : state(StateProjector{contracts}.CreateInitialState())
    {
    }

    [[nodiscard]] std::optional<SyncCursor> LoadCursor(
        const ethereum::Uint256& chainId
    ) const override
    {
        if(cursor.has_value() && cursor->chainId == chainId)
        {
            return cursor;
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<IndexedBlock> LoadCanonicalBlock(
        const ethereum::Uint256& chainId,
        std::uint64_t blockNumber
    ) const override
    {
        const auto iterator = std::find_if(
            blocks.rbegin(),
            blocks.rend(),
            [&chainId, blockNumber](const IndexedBlock& block) {
                return block.chainId == chainId && block.number == blockNumber && block.canonical;
            }
        );
        if(iterator == blocks.rend())
        {
            return std::nullopt;
        }
        return *iterator;
    }

    [[nodiscard]] std::vector<RawLog> LoadCanonicalLogsThrough(
        const ethereum::Uint256& chainId,
        std::uint64_t blockNumber
    ) const override
    {
        std::vector<RawLog> result;
        for(const auto& log : logs)
        {
            if(log.chainId == chainId && log.canonical && log.blockNumber <= blockNumber)
            {
                result.push_back(log);
            }
        }
        return result;
    }

    [[nodiscard]] DerivedState LoadDerivedState(
        const ethereum::Uint256&,
        const ChainContracts&
    ) const override
    {
        return state;
    }

    void CommitBlock(
        const IndexedBlock& block,
        const std::vector<RawLog>& blockLogs,
        const DerivedState& nextState
    ) override
    {
        blocks.push_back(block);
        logs.insert(logs.end(), blockLogs.begin(), blockLogs.end());
        state = nextState;
        cursor = SyncCursor{block.chainId, block.number, block.hash};
    }

    void RewindTo(
        const ethereum::Uint256& chainId,
        const std::optional<IndexedBlock>& ancestor,
        const DerivedState& nextState
    ) override
    {
        const auto isOrphan = [&chainId, &ancestor](const auto& value) {
            return value.chainId == chainId
                && value.canonical
                && (!ancestor.has_value() || value.blockNumber > ancestor->number);
        };
        for(auto& block : blocks)
        {
            if(block.chainId == chainId
                && block.canonical
                && (!ancestor.has_value() || block.number > ancestor->number))
            {
                block.canonical = false;
            }
        }
        for(auto& log : logs)
        {
            if(isOrphan(log))
            {
                log.canonical = false;
            }
        }
        state = nextState;
        if(ancestor.has_value())
        {
            cursor = SyncCursor{chainId, ancestor->number, ancestor->hash};
        }
        else
        {
            cursor = std::nullopt;
        }
    }

    std::optional<SyncCursor> cursor;
    std::vector<IndexedBlock> blocks;
    std::vector<RawLog> logs;
    DerivedState state;
};

}

TEST(IndexerTests, ResumesFromStoredCanonicalCursor)
{
    const auto contracts = test::Contracts();
    const auto hash0 = test::Hash(1, 0);
    const auto hash1 = test::Hash(1, 1);
    FakeChain chain;
    chain.blocks.emplace(0, Block(0, hash0, ethereum::Hash256{}));
    chain.blocks.emplace(1, Block(1, hash1, hash0));
    MemoryStore store{contracts};
    Indexer indexer{chain, store, contracts, 0};

    EXPECT_EQ(indexer.SyncToHead(), 2U);
    ASSERT_TRUE(store.cursor.has_value());
    EXPECT_EQ(store.cursor->blockNumber, 1U);
    EXPECT_EQ(indexer.SyncToHead(), 0U);
    EXPECT_EQ(store.blocks.size(), 2U);
}

TEST(IndexerTests, MarksOrphansAndRebuildsStateFromCommonAncestor)
{
    const auto contracts = test::Contracts();
    const auto hash0 = test::Hash(1, 0);
    const auto hash1 = test::Hash(1, 1);
    const auto oldHash2 = test::Hash(1, 2);
    const auto newHash2 = test::Hash(2, 2);
    const auto newHash3 = test::Hash(2, 3);
    FakeChain chain;
    chain.blocks.emplace(0, Block(0, hash0, ethereum::Hash256{}));
    chain.blocks.emplace(1, Block(1, hash1, hash0));
    chain.blocks.emplace(2, Block(2, oldHash2, hash1));
    chain.logs[2] = {
        ToRpcLog(test::EventLog(
            ethereum::ProtocolEventKind::PriceUpdated,
            contracts.oracle,
            {contracts.weth, ethereum::Uint256{100}, ethereum::Uint256{2}},
            2,
            0,
            oldHash2
        ))
    };

    MemoryStore store{contracts};
    Indexer indexer{chain, store, contracts, 0};
    EXPECT_EQ(indexer.SyncToHead(), 3U);
    EXPECT_EQ(store.state.market.wethPrice, ethereum::Uint256{100});

    chain.blocks[2] = Block(2, newHash2, hash1);
    chain.blocks.emplace(3, Block(3, newHash3, newHash2));
    chain.logs[2] = {
        ToRpcLog(test::EventLog(
            ethereum::ProtocolEventKind::PriceUpdated,
            contracts.oracle,
            {contracts.weth, ethereum::Uint256{200}, ethereum::Uint256{3}},
            2,
            0,
            newHash2
        ))
    };

    EXPECT_EQ(indexer.SyncToHead(), 2U);
    ASSERT_TRUE(store.cursor.has_value());
    EXPECT_EQ(store.cursor->blockNumber, 3U);
    EXPECT_EQ(store.cursor->blockHash, newHash3);
    EXPECT_EQ(store.state.market.wethPrice, ethereum::Uint256{200});

    const auto oldBlock = std::find_if(
        store.blocks.begin(),
        store.blocks.end(),
        [&oldHash2](const IndexedBlock& block) { return block.hash == oldHash2; }
    );
    ASSERT_NE(oldBlock, store.blocks.end());
    EXPECT_FALSE(oldBlock->canonical);

    const auto oldLog = std::find_if(
        store.logs.begin(),
        store.logs.end(),
        [&oldHash2](const RawLog& log) { return log.blockHash == oldHash2; }
    );
    ASSERT_NE(oldLog, store.logs.end());
    EXPECT_FALSE(oldLog->canonical);
}

}
