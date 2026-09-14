#include <cstdlib>

#include <gtest/gtest.h>

#include "dlp/ethereum/Address.hpp"
#include "dlp/indexer/RpcChainClient.hpp"

namespace dlp::indexer
{

TEST(RpcChainClientIntegrationTests, ReadsConfiguredProtocolLogsFromAnvil)
{
    const auto* endpoint = std::getenv("DLP_RPC_URL");
    const auto* pool = std::getenv("DLP_POOL_ADDRESS");
    const auto* oracle = std::getenv("DLP_ORACLE_ADDRESS");
    const auto* weth = std::getenv("DLP_WETH_ADDRESS");
    const auto* usdc = std::getenv("DLP_USDC_ADDRESS");
    const auto missing = [](const char* value) { return value == nullptr || *value == '\0'; };
    if(missing(endpoint) || missing(pool) || missing(oracle) || missing(weth) || missing(usdc))
    {
        GTEST_SKIP() << "set the RPC URL and four protocol addresses to run the Anvil integration test";
    }

    const ChainContracts contracts{
        ethereum::Address::FromHex(pool),
        ethereum::Address::FromHex(oracle),
        ethereum::Address::FromHex(weth),
        ethereum::Address::FromHex(usdc)
    };
    const RpcChainClient client{endpoint, contracts};
    const auto head = client.GetBlockNumber();
    const auto block = client.GetBlockByNumber(head);

    ASSERT_TRUE(block.has_value());
    EXPECT_EQ(block->number, ethereum::Uint256{head});
    for(const auto& log : client.GetLogs(head))
    {
        EXPECT_EQ(log.blockNumber, ethereum::Uint256{head});
        EXPECT_TRUE(log.address == contracts.pool || log.address == contracts.oracle);
    }
}

}
