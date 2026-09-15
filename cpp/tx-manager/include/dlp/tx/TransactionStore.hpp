#ifndef DLP_TX_TRANSACTION_STORE_HPP
#define DLP_TX_TRANSACTION_STORE_HPP

#include <optional>
#include <vector>

#include "dlp/tx/TxTypes.hpp"

namespace dlp::tx
{

class TransactionStore
{
public:
    virtual ~TransactionStore() = default;

    [[nodiscard]] virtual bool Insert(const TxJob& job) = 0;
    [[nodiscard]] virtual std::vector<TxJob> LoadActive(
        const ethereum::Uint256& chainId,
        const ethereum::Address& wallet
    ) const = 0;
    [[nodiscard]] virtual std::optional<ethereum::Uint256> LoadHighestNonce(
        const ethereum::Uint256& chainId,
        const ethereum::Address& wallet
    ) const = 0;
    virtual void Save(const TxJob& job) = 0;
};

}

#endif
