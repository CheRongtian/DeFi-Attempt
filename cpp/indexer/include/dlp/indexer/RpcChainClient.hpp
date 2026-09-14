#ifndef DLP_INDEXER_RPC_CHAIN_CLIENT_HPP
#define DLP_INDEXER_RPC_CHAIN_CLIENT_HPP

#include <chrono>
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

    [[nodiscard]] ethereum::Uint256 GetChainId() const override;
    [[nodiscard]] std::uint64_t GetBlockNumber() const override;
    [[nodiscard]] std::optional<ethereum::BlockHeader> GetBlockByNumber(
        std::uint64_t number
    ) const override;
    [[nodiscard]] std::vector<ethereum::RpcLog> GetLogs(std::uint64_t blockNumber) const override;

private:
    ethereum::RpcClient client_;
    ChainContracts contracts_;
};

}

#endif
