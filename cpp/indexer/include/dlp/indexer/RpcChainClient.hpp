#ifndef DLP_INDEXER_RPC_CHAIN_CLIENT_HPP
#define DLP_INDEXER_RPC_CHAIN_CLIENT_HPP

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "dlp/ethereum/RpcClient.hpp"
#include "dlp/indexer/ChainClient.hpp"
#include "dlp/indexer/IndexerTypes.hpp"

namespace dlp::indexer
{

class RpcChainClient final : public ChainClient
{
public:
    RpcChainClient(
        std::string endpoint,
        ChainContracts contracts,
        std::chrono::milliseconds timeout = std::chrono::seconds(5)
    );
    RpcChainClient(
        std::vector<std::string> endpoints,
        ChainContracts contracts,
        std::chrono::milliseconds timeout = std::chrono::seconds(5)
    );
    ~RpcChainClient() override;

    RpcChainClient(RpcChainClient&& other) noexcept;
    RpcChainClient& operator=(RpcChainClient&& other) noexcept;

    RpcChainClient(const RpcChainClient&) = delete;
    RpcChainClient& operator=(const RpcChainClient&) = delete;

    [[nodiscard]] ethereum::Uint256 GetChainId() const override;
    [[nodiscard]] std::uint64_t GetBlockNumber() const override;
    [[nodiscard]] std::optional<ethereum::BlockHeader> GetBlockByNumber(
        std::uint64_t number
    ) const override;
    [[nodiscard]] std::vector<ethereum::RpcLog> GetLogs(std::uint64_t blockNumber) const override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
