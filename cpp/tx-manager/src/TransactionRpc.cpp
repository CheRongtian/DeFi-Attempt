#include "dlp/tx/TransactionRpc.hpp"

#include <stdexcept>
#include <utility>

namespace dlp::tx
{

class RpcTransactionClient::Impl final
{
public:
    explicit Impl(std::string endpoint)
        : client(std::move(endpoint))
    {
    }

    ethereum::RpcClient client;
};

RpcTransactionClient::RpcTransactionClient(std::string endpoint)
    : implementation_(std::make_unique<Impl>(std::move(endpoint)))
{
}

RpcTransactionClient::~RpcTransactionClient() = default;
RpcTransactionClient::RpcTransactionClient(RpcTransactionClient&& other) noexcept = default;
RpcTransactionClient& RpcTransactionClient::operator=(RpcTransactionClient&& other) noexcept = default;

ethereum::Uint256 RpcTransactionClient::GetChainId() const
{
    return implementation_->client.GetChainId();
}

std::uint64_t RpcTransactionClient::GetBlockNumber() const
{
    return implementation_->client.GetBlockNumber().ToUint64();
}

std::optional<ethereum::BlockHeader> RpcTransactionClient::GetBlock(std::uint64_t number) const
{
    return implementation_->client.GetBlockByNumber(ethereum::Uint256{number});
}

ethereum::Uint256 RpcTransactionClient::GetPendingNonce(const ethereum::Address& wallet) const
{
    return implementation_->client.GetTransactionCount(wallet, "pending");
}

ethereum::Uint256 RpcTransactionClient::EstimateGas(const ethereum::TransactionCall& call) const
{
    return implementation_->client.EstimateGas(call);
}

FeeQuote RpcTransactionClient::GetFeeQuote() const
{
    const auto head = implementation_->client.GetBlockNumber();
    const auto block = implementation_->client.GetBlockByNumber(head);
    if(!block.has_value() || !block->baseFeePerGas.has_value())
    {
        throw std::runtime_error("latest block does not contain an EIP-1559 base fee");
    }
    const auto priorityFee = implementation_->client.GetMaxPriorityFeePerGas();
    return FeeQuote{
        priorityFee,
        *block->baseFeePerGas * ethereum::Uint256{2} + priorityFee
    };
}

ethereum::Hash256 RpcTransactionClient::SendRawTransaction(
    const ethereum::Bytes& rawTransaction
) const
{
    return implementation_->client.SendRawTransaction(rawTransaction);
}

std::optional<ethereum::TransactionReceipt> RpcTransactionClient::GetReceipt(
    const ethereum::Hash256& transactionHash
) const
{
    return implementation_->client.GetTransactionReceipt(transactionHash);
}

}
