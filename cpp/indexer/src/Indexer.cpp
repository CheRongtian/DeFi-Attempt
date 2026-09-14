#include "dlp/indexer/Indexer.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace dlp::indexer
{

Indexer::Indexer(
    ChainClient& chain,
    IndexerStore& store,
    ChainContracts contracts,
    std::uint64_t startBlock
)
    : chain_(chain),
      store_(store),
      contracts_(std::move(contracts)),
      startBlock_(startBlock),
      projector_(contracts_)
{
}

std::size_t Indexer::SyncToHead()
{
    const auto chainId = chain_.GetChainId();
    std::size_t indexedCount = 0;

    while(true)
    {
        const auto chainHead = chain_.GetBlockNumber();
        auto cursor = ReconcileCursor(chainId, chainHead);

        if(cursor.has_value() && cursor->blockNumber == std::numeric_limits<std::uint64_t>::max())
        {
            return indexedCount;
        }

        const auto nextBlock = cursor.has_value() ? cursor->blockNumber + 1U : startBlock_;
        if(nextBlock > chainHead)
        {
            return indexedCount;
        }

        const auto header = chain_.GetBlockByNumber(nextBlock);
        if(!header.has_value())
        {
            throw std::runtime_error("chain did not return the requested block");
        }
        if(header->number.ToUint64() != nextBlock)
        {
            throw std::runtime_error("RPC block number does not match the request");
        }

        if(cursor.has_value() && header->parentHash != cursor->blockHash)
        {
            const auto ancestor = FindCommonAncestor(chainId, std::min(cursor->blockNumber, chainHead));
            Rewind(chainId, ancestor);
            continue;
        }

        IndexBlock(chainId, *header);
        ++indexedCount;
    }
}

std::optional<SyncCursor> Indexer::ReconcileCursor(
    const ethereum::Uint256& chainId,
    std::uint64_t chainHead
)
{
    const auto cursor = store_.LoadCursor(chainId);
    if(!cursor.has_value())
    {
        return std::nullopt;
    }

    if(cursor->blockNumber <= chainHead)
    {
        const auto chainBlock = chain_.GetBlockByNumber(cursor->blockNumber);
        if(chainBlock.has_value() && chainBlock->hash == cursor->blockHash)
        {
            return cursor;
        }
    }

    const auto ancestor = FindCommonAncestor(chainId, std::min(cursor->blockNumber, chainHead));
    Rewind(chainId, ancestor);
    if(!ancestor.has_value())
    {
        return std::nullopt;
    }
    return SyncCursor{chainId, ancestor->number, ancestor->hash};
}

std::optional<IndexedBlock> Indexer::FindCommonAncestor(
    const ethereum::Uint256& chainId,
    std::uint64_t upperBound
) const
{
    auto blockNumber = upperBound;
    while(blockNumber >= startBlock_)
    {
        const auto storedBlock = store_.LoadCanonicalBlock(chainId, blockNumber);
        const auto chainBlock = chain_.GetBlockByNumber(blockNumber);
        if(storedBlock.has_value() && chainBlock.has_value() && storedBlock->hash == chainBlock->hash)
        {
            return storedBlock;
        }

        if(blockNumber == startBlock_)
        {
            break;
        }
        --blockNumber;
    }
    return std::nullopt;
}

void Indexer::Rewind(
    const ethereum::Uint256& chainId,
    const std::optional<IndexedBlock>& ancestor
)
{
    const auto state = ancestor.has_value()
        ? projector_.Rebuild(store_.LoadCanonicalLogsThrough(chainId, ancestor->number))
        : projector_.CreateInitialState();
    store_.RewindTo(chainId, ancestor, state);
}

void Indexer::IndexBlock(
    const ethereum::Uint256& chainId,
    const ethereum::BlockHeader& header
)
{
    const auto blockNumber = header.number.ToUint64();
    auto rpcLogs = chain_.GetLogs(blockNumber);
    std::sort(
        rpcLogs.begin(),
        rpcLogs.end(),
        [](const ethereum::RpcLog& left, const ethereum::RpcLog& right) {
            return left.logIndex < right.logIndex;
        }
    );

    std::vector<RawLog> logs;
    logs.reserve(rpcLogs.size());
    for(auto& rpcLog : rpcLogs)
    {
        if(rpcLog.removed)
        {
            continue;
        }
        if(rpcLog.blockNumber.ToUint64() != blockNumber || rpcLog.blockHash != header.hash)
        {
            throw std::runtime_error("block changed while its logs were being fetched");
        }

        logs.push_back(RawLog{
            chainId,
            blockNumber,
            rpcLog.blockHash,
            rpcLog.transactionHash,
            rpcLog.transactionIndex.ToUint64(),
            rpcLog.logIndex.ToUint64(),
            std::move(rpcLog.address),
            std::move(rpcLog.topics),
            std::move(rpcLog.data),
            true
        });
    }

    auto state = store_.LoadDerivedState(chainId, contracts_);
    for(const auto& log : logs)
    {
        projector_.Apply(state, log);
    }

    store_.CommitBlock(
        IndexedBlock{chainId, blockNumber, header.hash, header.parentHash, true},
        logs,
        state
    );
}

}
