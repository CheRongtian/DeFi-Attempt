#include "dlp/tx/TransactionRpc.hpp"

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace dlp::tx
{

class RpcTransactionClient::Impl final
{
public:
    Impl(std::string primaryEndpoint, std::vector<std::string> additionalEndpoints)
    {
        endpoints_.reserve(additionalEndpoints.size() + 1U);
        endpoints_.push_back(std::make_unique<ethereum::RpcClient>(std::move(primaryEndpoint)));
        for(auto& endpoint : additionalEndpoints)
        {
            endpoints_.push_back(std::make_unique<ethereum::RpcClient>(std::move(endpoint)));
        }
        validated_.resize(endpoints_.size(), false);
    }

    template<typename Operation>
    [[nodiscard]] auto Execute(Operation&& operation) const
        -> std::invoke_result_t<Operation, ethereum::RpcClient&>
    {
        return ExecuteFrom(activeIndex_, std::forward<Operation>(operation));
    }

    template<typename Operation>
    [[nodiscard]] auto ExecutePrimary(Operation&& operation) const
        -> std::invoke_result_t<Operation, ethereum::RpcClient&>
    {
        return ExecuteFrom(0, std::forward<Operation>(operation));
    }

    [[nodiscard]] ethereum::Hash256 Broadcast(const ethereum::Bytes& rawTransaction) const
    {
        std::optional<ethereum::Hash256> transactionHash;
        std::exception_ptr lastFailure;
        for(std::size_t index = 0; index < endpoints_.size(); ++index)
        {
            try
            {
                Validate(index);
                const auto submittedHash = endpoints_[index]->SendRawTransaction(rawTransaction);
                if(!transactionHash.has_value())
                {
                    transactionHash = submittedHash;
                }
            }
            catch(...)
            {
                lastFailure = std::current_exception();
            }
        }
        if(transactionHash.has_value())
        {
            return *transactionHash;
        }
        std::rethrow_exception(lastFailure);
    }

private:
    template<typename Operation>
    [[nodiscard]] auto ExecuteFrom(std::size_t start, Operation&& operation) const
        -> std::invoke_result_t<Operation, ethereum::RpcClient&>
    {
        std::exception_ptr lastFailure;
        for(std::size_t offset = 0; offset < endpoints_.size(); ++offset)
        {
            const auto index = (start + offset) % endpoints_.size();
            try
            {
                Validate(index);
                auto result = operation(*endpoints_[index]);
                activeIndex_ = index;
                return result;
            }
            catch(...)
            {
                lastFailure = std::current_exception();
            }
        }
        std::rethrow_exception(lastFailure);
    }

    void Validate(std::size_t index) const
    {
        if(validated_[index])
        {
            return;
        }
        const auto chainId = endpoints_[index]->GetChainId();
        if(!chainId_.has_value())
        {
            chainId_ = chainId;
        }
        else if(chainId != *chainId_)
        {
            throw std::runtime_error("transaction RPC endpoint belongs to a different chain");
        }
        validated_[index] = true;
    }

    std::vector<std::unique_ptr<ethereum::RpcClient>> endpoints_;
    mutable std::vector<bool> validated_;
    mutable std::optional<ethereum::Uint256> chainId_;
    mutable std::size_t activeIndex_{0};
};

RpcTransactionClient::RpcTransactionClient(std::string endpoint)
    : RpcTransactionClient(std::move(endpoint), {})
{
}

RpcTransactionClient::RpcTransactionClient(
    std::string primaryEndpoint,
    std::vector<std::string> additionalEndpoints
)
    : implementation_(std::make_unique<Impl>(
          std::move(primaryEndpoint),
          std::move(additionalEndpoints)
      ))
{
}

RpcTransactionClient::~RpcTransactionClient() = default;
RpcTransactionClient::RpcTransactionClient(RpcTransactionClient&& other) noexcept = default;
RpcTransactionClient& RpcTransactionClient::operator=(RpcTransactionClient&& other) noexcept = default;

ethereum::Uint256 RpcTransactionClient::GetChainId() const
{
    return implementation_->Execute(
        [](ethereum::RpcClient& client) { return client.GetChainId(); }
    );
}

std::uint64_t RpcTransactionClient::GetBlockNumber() const
{
    return implementation_->Execute(
        [](ethereum::RpcClient& client) { return client.GetBlockNumber().ToUint64(); }
    );
}

std::optional<ethereum::BlockHeader> RpcTransactionClient::GetBlock(std::uint64_t number) const
{
    return implementation_->Execute([number](ethereum::RpcClient& client) {
        return client.GetBlockByNumber(ethereum::Uint256{number});
    });
}

ethereum::Uint256 RpcTransactionClient::GetPendingNonce(const ethereum::Address& wallet) const
{
    return implementation_->ExecutePrimary([&wallet](ethereum::RpcClient& client) {
        return client.GetTransactionCount(wallet, "pending");
    });
}

ethereum::Uint256 RpcTransactionClient::EstimateGas(const ethereum::TransactionCall& call) const
{
    return implementation_->Execute(
        [&call](ethereum::RpcClient& client) { return client.EstimateGas(call); }
    );
}

FeeQuote RpcTransactionClient::GetFeeQuote() const
{
    return implementation_->Execute([](ethereum::RpcClient& client) {
        const auto head = client.GetBlockNumber();
        const auto block = client.GetBlockByNumber(head);
        if(!block.has_value() || !block->baseFeePerGas.has_value())
        {
            throw std::runtime_error("latest block does not contain an EIP-1559 base fee");
        }
        const auto priorityFee = client.GetMaxPriorityFeePerGas();
        return FeeQuote{
            priorityFee,
            *block->baseFeePerGas * ethereum::Uint256{2} + priorityFee
        };
    });
}

ethereum::Hash256 RpcTransactionClient::SendRawTransaction(
    const ethereum::Bytes& rawTransaction
) const
{
    return implementation_->Broadcast(rawTransaction);
}

std::optional<ethereum::TransactionReceipt> RpcTransactionClient::GetReceipt(
    const ethereum::Hash256& transactionHash
) const
{
    return implementation_->Execute([&transactionHash](ethereum::RpcClient& client) {
        return client.GetTransactionReceipt(transactionHash);
    });
}

}
