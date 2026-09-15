#ifndef DLP_API_API_CHAIN_HPP
#define DLP_API_API_CHAIN_HPP

#include <memory>
#include <string>

#include "dlp/ethereum/RpcClient.hpp"

namespace dlp::api
{

class ApiChain
{
public:
    virtual ~ApiChain() = default;
    [[nodiscard]] virtual std::uint64_t GetBlockNumber() const = 0;
};

class RpcApiChain final : public ApiChain
{
public:
    explicit RpcApiChain(std::string endpoint);
    ~RpcApiChain() override;

    RpcApiChain(RpcApiChain&& other) noexcept;
    RpcApiChain& operator=(RpcApiChain&& other) noexcept;

    RpcApiChain(const RpcApiChain&) = delete;
    RpcApiChain& operator=(const RpcApiChain&) = delete;

    [[nodiscard]] std::uint64_t GetBlockNumber() const override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
