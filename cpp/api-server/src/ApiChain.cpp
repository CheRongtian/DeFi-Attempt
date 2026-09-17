#include "dlp/api/ApiChain.hpp"

#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace dlp::api
{

class RpcApiChain::Impl final
{
public:
    explicit Impl(std::vector<std::string> endpoints)
    {
        if(endpoints.empty())
        {
            throw std::invalid_argument("API requires at least one RPC endpoint");
        }
        clients_.reserve(endpoints.size());
        validated_.resize(endpoints.size(), false);
        for(auto& endpoint : endpoints)
        {
            clients_.push_back(std::make_unique<ethereum::RpcClient>(std::move(endpoint)));
        }
    }

    template<typename Operation>
    [[nodiscard]] auto Execute(Operation&& operation) const
        -> std::invoke_result_t<Operation, ethereum::RpcClient&>
    {
        std::exception_ptr lastFailure;
        const auto start = ActiveIndex();
        for(std::size_t offset = 0; offset < clients_.size(); ++offset)
        {
            const auto index = (start + offset) % clients_.size();
            try
            {
                Validate(index);
                auto result = operation(*clients_[index]);
                if(index != start)
                {
                    ethereum::ObserveRpcFailover();
                }
                SetActiveIndex(index);
                return result;
            }
            catch(...)
            {
                lastFailure = std::current_exception();
            }
        }
        std::rethrow_exception(lastFailure);
    }

    [[nodiscard]] ethereum::Uint256 GetChainId() const
    {
        return Execute([](ethereum::RpcClient& client) { return client.GetChainId(); });
    }

private:
    [[nodiscard]] std::size_t ActiveIndex() const
    {
        const std::scoped_lock lock{mutex_};
        return activeIndex_;
    }

    void SetActiveIndex(std::size_t index) const
    {
        const std::scoped_lock lock{mutex_};
        activeIndex_ = index;
    }

    void Validate(std::size_t index) const
    {
        {
            const std::scoped_lock lock{mutex_};
            if(validated_[index])
            {
                return;
            }
        }

        const auto chainId = clients_[index]->GetChainId();
        const std::scoped_lock lock{mutex_};
        if(!chainId_.has_value())
        {
            chainId_ = chainId;
        }
        else if(chainId != *chainId_)
        {
            throw std::runtime_error("read-only RPC endpoint belongs to a different chain");
        }
        validated_[index] = true;
    }

    std::vector<std::unique_ptr<ethereum::RpcClient>> clients_;
    mutable std::vector<bool> validated_;
    mutable std::optional<ethereum::Uint256> chainId_;
    mutable std::size_t activeIndex_{0};
    mutable std::mutex mutex_;
};

RpcApiChain::RpcApiChain(std::string endpoint)
    : RpcApiChain(std::vector<std::string>{std::move(endpoint)})
{
}

RpcApiChain::RpcApiChain(std::vector<std::string> endpoints)
    : implementation_(std::make_unique<Impl>(std::move(endpoints)))
{
}

RpcApiChain::~RpcApiChain() = default;
RpcApiChain::RpcApiChain(RpcApiChain&& other) noexcept = default;
RpcApiChain& RpcApiChain::operator=(RpcApiChain&& other) noexcept = default;

std::uint64_t RpcApiChain::GetBlockNumber() const
{
    return implementation_->Execute(
        [](ethereum::RpcClient& client) { return client.GetBlockNumber().ToUint64(); }
    );
}

ethereum::Uint256 RpcApiChain::GetChainId() const
{
    return implementation_->GetChainId();
}

}
