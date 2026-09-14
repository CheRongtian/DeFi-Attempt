#include <chrono>
#include <cstdlib>
#include <stdexcept>

#include <gtest/gtest.h>

#include "dlp/ethereum/RpcClient.hpp"

namespace dlp::ethereum
{

TEST(RpcClientTests, RejectsUnsupportedEndpointAndInvalidTimeout)
{
    EXPECT_THROW(RpcClient("https://127.0.0.1:8545"), RpcException);
    EXPECT_THROW(RpcClient("http://127.0.0.1:8545", std::chrono::milliseconds::zero()), RpcException);
}

TEST(RpcClientIntegrationTests, ReadsChainBlockAndLogsFromAnvil)
{
    const auto* endpoint = std::getenv("DLP_RPC_URL");
    if(endpoint == nullptr || *endpoint == '\0')
    {
        GTEST_SKIP() << "set DLP_RPC_URL to run the Anvil RPC integration test";
    }

    const RpcClient client{endpoint};
    const auto chainId = client.GetChainId();
    const auto blockNumber = client.GetBlockNumber();
    EXPECT_GT(chainId, Uint256{});

    const auto block = client.GetBlockByNumber(blockNumber);
    ASSERT_TRUE(block.has_value());
    EXPECT_EQ(block->number, blockNumber);

    LogFilter filter;
    filter.fromBlock = blockNumber;
    filter.toBlock = blockNumber;
    const auto logs = client.GetLogs(filter);
    for(const auto& log : logs)
    {
        EXPECT_EQ(log.blockNumber, blockNumber);
    }
}

}
