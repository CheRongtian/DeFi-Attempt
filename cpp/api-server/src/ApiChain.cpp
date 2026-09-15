#include "dlp/api/ApiChain.hpp"

#include <utility>

namespace dlp::api
{

class RpcApiChain::Impl final
{
public:
    explicit Impl(std::string endpoint)
        : client(std::move(endpoint))
    {
    }

    ethereum::RpcClient client;
};

RpcApiChain::RpcApiChain(std::string endpoint)
    : implementation_(std::make_unique<Impl>(std::move(endpoint)))
{
}

RpcApiChain::~RpcApiChain() = default;
RpcApiChain::RpcApiChain(RpcApiChain&& other) noexcept = default;
RpcApiChain& RpcApiChain::operator=(RpcApiChain&& other) noexcept = default;

std::uint64_t RpcApiChain::GetBlockNumber() const
{
    return implementation_->client.GetBlockNumber().ToUint64();
}

}
