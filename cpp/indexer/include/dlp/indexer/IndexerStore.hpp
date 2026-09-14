#ifndef DLP_INDEXER_INDEXER_STORE_HPP
#define DLP_INDEXER_INDEXER_STORE_HPP

#include <cstdint>
#include <optional>
#include <vector>

#include "dlp/indexer/IndexerTypes.hpp"

namespace dlp::indexer
{

class IndexerStore
{
public:
    virtual ~IndexerStore() = default;

    [[nodiscard]] virtual std::optional<SyncCursor> LoadCursor(
        const ethereum::Uint256& chainId
    ) const = 0;

    [[nodiscard]] virtual std::optional<IndexedBlock> LoadCanonicalBlock(
        const ethereum::Uint256& chainId,
        std::uint64_t blockNumber
    ) const = 0;

    [[nodiscard]] virtual std::vector<RawLog> LoadCanonicalLogsThrough(
        const ethereum::Uint256& chainId,
        std::uint64_t blockNumber
    ) const = 0;

    [[nodiscard]] virtual DerivedState LoadDerivedState(
        const ethereum::Uint256& chainId,
        const ChainContracts& contracts
    ) const = 0;

    virtual void CommitBlock(
        const IndexedBlock& block,
        const std::vector<RawLog>& logs,
        const DerivedState& state
    ) = 0;

    virtual void RewindTo(
        const ethereum::Uint256& chainId,
        const std::optional<IndexedBlock>& ancestor,
        const DerivedState& state
    ) = 0;
};

}

#endif
