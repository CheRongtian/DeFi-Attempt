#ifndef DLP_TX_POSTGRES_TRANSACTION_STORE_HPP
#define DLP_TX_POSTGRES_TRANSACTION_STORE_HPP

#include <memory>
#include <string>

#include "dlp/tx/TransactionStore.hpp"

namespace dlp::tx
{

class PostgresTransactionStore final : public TransactionStore
{
public:
    explicit PostgresTransactionStore(std::string connectionString);
    ~PostgresTransactionStore() override;

    PostgresTransactionStore(PostgresTransactionStore&& other) noexcept;
    PostgresTransactionStore& operator=(PostgresTransactionStore&& other) noexcept;

    PostgresTransactionStore(const PostgresTransactionStore&) = delete;
    PostgresTransactionStore& operator=(const PostgresTransactionStore&) = delete;

    [[nodiscard]] bool Insert(const TxJob& job) override;
    [[nodiscard]] std::vector<TxJob> LoadActive(
        const ethereum::Uint256& chainId,
        const ethereum::Address& wallet
    ) const override;
    [[nodiscard]] std::optional<ethereum::Uint256> LoadHighestNonce(
        const ethereum::Uint256& chainId,
        const ethereum::Address& wallet
    ) const override;
    void Save(const TxJob& job) override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
