#include "dlp/indexer/RpcChainClient.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dlp::indexer
{

class RpcChainClient::Impl final
{
public:
    Impl(
        std::vector<std::string> endpoints,
        ChainContracts contracts,
        std::chrono::milliseconds timeout
    )
        : contracts_(std::move(contracts))
    {
        if(endpoints.empty())
        {
            throw std::invalid_argument("indexer requires at least one RPC endpoint");
        }
        clients_.reserve(endpoints.size());
        for(auto& endpoint : endpoints)
        {
            clients_.push_back(std::make_unique<ethereum::RpcClient>(std::move(endpoint), timeout));
        }
    }

    template<typename Operation>
    [[nodiscard]] auto Execute(Operation&& operation) const
        -> std::invoke_result_t<Operation, ethereum::RpcClient&>
    {
        std::exception_ptr lastFailure;
        const auto start = activeIndex_;
        for(std::size_t offset = 0; offset < clients_.size(); ++offset)
        {
            const auto index = (start + offset) % clients_.size();
            try
            {
                ValidateCandidate(index);
                auto result = operation(*clients_[index]);
                if(index != start)
                {
                    ethereum::ObserveRpcFailover();
                }
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

    void UpdateCheckpoint(const std::optional<ethereum::BlockHeader>& block) const
    {
        if(block.has_value())
        {
            checkpoint_ = *block;
        }
    }

    [[nodiscard]] const ChainContracts& Contracts() const noexcept
    {
        return contracts_;
    }

private:
    void ValidateCandidate(std::size_t index) const
    {
        if(index == activeIndex_ && chainId_.has_value())
        {
            return;
        }

        auto& candidate = *clients_[index];
        const auto candidateChainId = candidate.GetChainId();
        if(!chainId_.has_value())
        {
            chainId_ = candidateChainId;
        }
        else if(candidateChainId != *chainId_)
        {
            throw std::runtime_error("RPC failover candidate belongs to a different chain");
        }

        if(index != activeIndex_ && checkpoint_.has_value())
        {
            const auto candidateBlock = candidate.GetBlockByNumber(checkpoint_->number);
            if(!candidateBlock.has_value() || candidateBlock->hash != checkpoint_->hash)
            {
                throw std::runtime_error("RPC failover candidate has a different canonical block");
            }
        }
    }

    std::vector<std::unique_ptr<ethereum::RpcClient>> clients_;
    ChainContracts contracts_;
    mutable std::size_t activeIndex_{0};
    mutable std::optional<ethereum::Uint256> chainId_;
    mutable std::optional<ethereum::BlockHeader> checkpoint_;
};

RpcChainClient::RpcChainClient(
    std::string endpoint,
    ChainContracts contracts,
    std::chrono::milliseconds timeout
)
    : RpcChainClient(
          std::vector<std::string>{std::move(endpoint)},
          std::move(contracts),
          timeout
      )
{
}

RpcChainClient::RpcChainClient(
    std::vector<std::string> endpoints,
    ChainContracts contracts,
    std::chrono::milliseconds timeout
)
    : implementation_(std::make_unique<Impl>(
          std::move(endpoints),
          std::move(contracts),
          timeout
      ))
{
}

RpcChainClient::~RpcChainClient() = default;
RpcChainClient::RpcChainClient(RpcChainClient&& other) noexcept = default;
RpcChainClient& RpcChainClient::operator=(RpcChainClient&& other) noexcept = default;

ethereum::Uint256 RpcChainClient::GetChainId() const
{
    return implementation_->Execute(
        [](ethereum::RpcClient& client) { return client.GetChainId(); }
    );
}

std::uint64_t RpcChainClient::GetBlockNumber() const
{
    return implementation_->Execute(
        [](ethereum::RpcClient& client) { return client.GetBlockNumber().ToUint64(); }
    );
}

std::optional<ethereum::BlockHeader> RpcChainClient::GetBlockByNumber(std::uint64_t number) const
{
    auto block = implementation_->Execute([number](ethereum::RpcClient& client) {
        return client.GetBlockByNumber(ethereum::Uint256{number});
    });
    implementation_->UpdateCheckpoint(block);
    return block;
}

std::vector<ethereum::RpcLog> RpcChainClient::GetLogs(std::uint64_t blockNumber) const
{
    const ethereum::Uint256 number{blockNumber};
    const auto getLogs = [&number](ethereum::RpcClient& client, const ethereum::Address& address) {
        ethereum::LogFilter filter;
        filter.fromBlock = number;
        filter.toBlock = number;
        filter.address = address;
        return client.GetLogs(filter);
    };

    auto result = implementation_->Execute([&](ethereum::RpcClient& client) {
        auto logs = getLogs(client, implementation_->Contracts().pool);
        auto oracleLogs = getLogs(client, implementation_->Contracts().oracle);
        logs.insert(logs.end(), oracleLogs.begin(), oracleLogs.end());
        return logs;
    });

    std::sort(
        result.begin(),
        result.end(),
        [](const ethereum::RpcLog& left, const ethereum::RpcLog& right) {
            return left.logIndex < right.logIndex;
        }
    );
    return result;
}

}
