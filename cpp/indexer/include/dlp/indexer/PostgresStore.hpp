#ifndef DLP_INDEXER_POSTGRES_STORE_HPP
#define DLP_INDEXER_POSTGRES_STORE_HPP

#include <memory>
#include <string>

#include "dlp/indexer/IndexerStore.hpp"

namespace dlp::indexer
{

class PostgresStore final : public IndexerStore
{
public:
    explicit PostgresStore(std::string connectionString);
    ~PostgresStore() override;

    PostgresStore(PostgresStore&& other) noexcept;
    PostgresStore& operator=(PostgresStore&& other) noexcept;

    PostgresStore(const PostgresStore&) = delete;
    PostgresStore& operator=(const PostgresStore&) = delete;

    [[nodiscard]] std::optional<SyncCursor> LoadCursor(
        const ethereum::Uint256& chainId
    ) const override;

    [[nodiscard]] std::optional<IndexedBlock> LoadCanonicalBlock(
        const ethereum::Uint256& chainId,
        std::uint64_t blockNumber
    ) const override;

    [[nodiscard]] std::vector<RawLog> LoadCanonicalLogsThrough(
        const ethereum::Uint256& chainId,
        std::uint64_t blockNumber
    ) const override;

    [[nodiscard]] DerivedState LoadDerivedState(
        const ethereum::Uint256& chainId,
        const ChainContracts& contracts
    ) const override;

    void CommitBlock(
        const IndexedBlock& block,
        const std::vector<RawLog>& logs,
        const DerivedState& state
    ) override;

    void RewindTo(
        const ethereum::Uint256& chainId,
        const std::optional<IndexedBlock>& ancestor,
        const DerivedState& state
    ) override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
