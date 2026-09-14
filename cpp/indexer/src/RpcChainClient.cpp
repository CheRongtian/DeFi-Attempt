#include "dlp/indexer/RpcChainClient.hpp"

#include <algorithm>
#include <utility>

namespace dlp::indexer
{

RpcChainClient::RpcChainClient(
    std::string endpoint,
    ChainContracts contracts,
    std::chrono::milliseconds timeout
)
    : client_(std::move(endpoint), timeout), contracts_(std::move(contracts))
{
}

ethereum::Uint256 RpcChainClient::GetChainId() const
{
    return client_.GetChainId();
}

std::uint64_t RpcChainClient::GetBlockNumber() const
{
    return client_.GetBlockNumber().ToUint64();
}

std::optional<ethereum::BlockHeader> RpcChainClient::GetBlockByNumber(std::uint64_t number) const
{
    return client_.GetBlockByNumber(ethereum::Uint256{number});
}

std::vector<ethereum::RpcLog> RpcChainClient::GetLogs(std::uint64_t blockNumber) const
{
    const ethereum::Uint256 number{blockNumber};
    const auto getLogs = [this, &number](const ethereum::Address& address) {
        ethereum::LogFilter filter;
        filter.fromBlock = number;
        filter.toBlock = number;
        filter.address = address;
        return client_.GetLogs(filter);
    };

    auto result = getLogs(contracts_.pool);
    auto oracleLogs = getLogs(contracts_.oracle);
    result.insert(result.end(), oracleLogs.begin(), oracleLogs.end());

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
