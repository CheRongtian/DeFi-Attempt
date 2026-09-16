#include <gtest/gtest.h>

#include "dlp/ethereum/RpcEndpoints.hpp"

namespace dlp::ethereum
{

TEST(RpcEndpointsTests, ParsesCommaSeparatedEndpoints)
{
    const auto endpoints = ParseRpcEndpoints(
        "http://rpc-a:8545, http://rpc-b:8545,http://rpc-c:8545"
    );

    ASSERT_EQ(endpoints.size(), 3U);
    EXPECT_EQ(endpoints[0], "http://rpc-a:8545");
    EXPECT_EQ(endpoints[1], "http://rpc-b:8545");
    EXPECT_EQ(endpoints[2], "http://rpc-c:8545");
}

TEST(RpcEndpointsTests, KeepsPrimaryFirstAndRemovesDuplicates)
{
    const auto endpoints = MergeRpcEndpoints(
        "http://rpc-a:8545",
        {"http://rpc-b:8545", "http://rpc-a:8545"}
    );

    ASSERT_EQ(endpoints.size(), 2U);
    EXPECT_EQ(endpoints[0], "http://rpc-a:8545");
    EXPECT_EQ(endpoints[1], "http://rpc-b:8545");
}

}
