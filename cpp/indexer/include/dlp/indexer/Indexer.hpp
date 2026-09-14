#ifndef DLP_INDEXER_INDEXER_HPP
#define DLP_INDEXER_INDEXER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>

#include "dlp/indexer/ChainClient.hpp"
#include "dlp/indexer/IndexerStore.hpp"
#include "dlp/indexer/StateProjector.hpp"

namespace dlp::indexer
{

class Indexer final
{
public:
    Indexer(
        ChainClient& chain,
        IndexerStore& store,
        ChainContracts contracts,
        std::uint64_t startBlock
    );

    [[nodiscard]] std::size_t SyncToHead();

private:
    [[nodiscard]] std::optional<SyncCursor> ReconcileCursor(
        const ethereum::Uint256& chainId,
        std::uint64_t chainHead
    );
    [[nodiscard]] std::optional<IndexedBlock> FindCommonAncestor(
        const ethereum::Uint256& chainId,
        std::uint64_t upperBound
    ) const;
    void Rewind(
        const ethereum::Uint256& chainId,
        const std::optional<IndexedBlock>& ancestor
    );
    void IndexBlock(
        const ethereum::Uint256& chainId,
        const ethereum::BlockHeader& header
    );

    ChainClient& chain_;
    IndexerStore& store_;
    ChainContracts contracts_;
    std::uint64_t startBlock_;
    StateProjector projector_;
};

}

#endif
