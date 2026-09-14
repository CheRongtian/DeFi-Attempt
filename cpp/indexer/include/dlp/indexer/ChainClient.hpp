#ifndef DLP_INDEXER_CHAIN_CLIENT_HPP
#define DLP_INDEXER_CHAIN_CLIENT_HPP

#include <cstdint>
#include <optional>
#include <vector>

#include "dlp/ethereum/RpcClient.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::indexer
{

class ChainClient
{
public:
    virtual ~ChainClient() = default;

    [[nodiscard]] virtual ethereum::Uint256 GetChainId() const = 0;
    [[nodiscard]] virtual std::uint64_t GetBlockNumber() const = 0;
    [[nodiscard]] virtual std::optional<ethereum::BlockHeader> GetBlockByNumber(
        std::uint64_t number
    ) const = 0;
    [[nodiscard]] virtual std::vector<ethereum::RpcLog> GetLogs(std::uint64_t blockNumber) const = 0;
};

}

#endif
