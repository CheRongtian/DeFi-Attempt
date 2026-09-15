#ifndef DLP_TX_TRANSACTION_RPC_HPP
#define DLP_TX_TRANSACTION_RPC_HPP

#include <memory>
#include <optional>
#include <string>

#include "dlp/ethereum/RpcClient.hpp"
#include "dlp/tx/TxTypes.hpp"

namespace dlp::tx
{

class TransactionRpc
{
public:
    virtual ~TransactionRpc() = default;

    [[nodiscard]] virtual ethereum::Uint256 GetChainId() const = 0;
    [[nodiscard]] virtual std::uint64_t GetBlockNumber() const = 0;
    [[nodiscard]] virtual std::optional<ethereum::BlockHeader> GetBlock(
        std::uint64_t number
    ) const = 0;
    [[nodiscard]] virtual ethereum::Uint256 GetPendingNonce(
        const ethereum::Address& wallet
    ) const = 0;
    [[nodiscard]] virtual ethereum::Uint256 EstimateGas(
        const ethereum::TransactionCall& call
    ) const = 0;
    [[nodiscard]] virtual FeeQuote GetFeeQuote() const = 0;
    [[nodiscard]] virtual ethereum::Hash256 SendRawTransaction(
        const ethereum::Bytes& rawTransaction
    ) const = 0;
    [[nodiscard]] virtual std::optional<ethereum::TransactionReceipt> GetReceipt(
        const ethereum::Hash256& transactionHash
    ) const = 0;
};

class RpcTransactionClient final : public TransactionRpc
{
public:
    explicit RpcTransactionClient(std::string endpoint);
    ~RpcTransactionClient() override;

    RpcTransactionClient(RpcTransactionClient&& other) noexcept;
    RpcTransactionClient& operator=(RpcTransactionClient&& other) noexcept;

    RpcTransactionClient(const RpcTransactionClient&) = delete;
    RpcTransactionClient& operator=(const RpcTransactionClient&) = delete;

    [[nodiscard]] ethereum::Uint256 GetChainId() const override;
    [[nodiscard]] std::uint64_t GetBlockNumber() const override;
    [[nodiscard]] std::optional<ethereum::BlockHeader> GetBlock(std::uint64_t number) const override;
    [[nodiscard]] ethereum::Uint256 GetPendingNonce(const ethereum::Address& wallet) const override;
    [[nodiscard]] ethereum::Uint256 EstimateGas(const ethereum::TransactionCall& call) const override;
    [[nodiscard]] FeeQuote GetFeeQuote() const override;
    [[nodiscard]] ethereum::Hash256 SendRawTransaction(
        const ethereum::Bytes& rawTransaction
    ) const override;
    [[nodiscard]] std::optional<ethereum::TransactionReceipt> GetReceipt(
        const ethereum::Hash256& transactionHash
    ) const override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
