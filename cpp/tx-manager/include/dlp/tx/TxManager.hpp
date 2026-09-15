#ifndef DLP_TX_TX_MANAGER_HPP
#define DLP_TX_TX_MANAGER_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include "dlp/ethereum/Transaction.hpp"
#include "dlp/tx/TransactionRpc.hpp"
#include "dlp/tx/TransactionStore.hpp"

namespace dlp::tx
{

struct TxManagerConfig
{
    std::uint64_t confirmationDepth{1};
    std::uint64_t replacementAfterBlocks{3};
    std::uint32_t maximumRetries{3};
};

class TransactionQueue
{
public:
    virtual ~TransactionQueue() = default;

    [[nodiscard]] virtual bool Queue(
        std::string jobId,
        const ethereum::Address& to,
        ethereum::Bytes data,
        ethereum::Uint256 value = ethereum::Uint256{}
    ) = 0;
};

class TxManager final : public TransactionQueue
{
public:
    TxManager(
        TransactionStore& store,
        TransactionRpc& rpc,
        ethereum::Secp256k1Signer& signer,
        TxManagerConfig config = {}
    );

    [[nodiscard]] bool Queue(
        std::string jobId,
        const ethereum::Address& to,
        ethereum::Bytes data,
        ethereum::Uint256 value = ethereum::Uint256{}
    ) override;
    void RunOnce();

private:
    void Submit(TxJob& job, bool retry);
    void Poll(TxJob& job);
    [[nodiscard]] ethereum::Uint256 AllocateNonce() const;

    TransactionStore& store_;
    TransactionRpc& rpc_;
    ethereum::Secp256k1Signer& signer_;
    TxManagerConfig config_;
    ethereum::Uint256 chainId_;
};

}

#endif
