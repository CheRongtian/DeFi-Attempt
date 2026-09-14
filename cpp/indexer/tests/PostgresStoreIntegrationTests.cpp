#include <cstdlib>

#include <gtest/gtest.h>

#include "dlp/indexer/PostgresStore.hpp"
#include "TestData.hpp"

namespace dlp::indexer
{

TEST(PostgresStoreIntegrationTests, ReadsMigratedTables)
{
    const auto* connectionString = std::getenv("DLP_TEST_DATABASE_URL");
    if(connectionString == nullptr || *connectionString == '\0')
    {
        GTEST_SKIP() << "set DLP_TEST_DATABASE_URL to run the PostgreSQL integration test";
    }

    const auto chainId = ethereum::Uint256::FromDecimal(
        "115792089237316195423570985008687907853269984665640564039457584007913129639935"
    );
    const PostgresStore store{connectionString};

    EXPECT_FALSE(store.LoadCursor(chainId).has_value());
    const auto state = store.LoadDerivedState(chainId, test::Contracts());
    EXPECT_TRUE(state.positions.empty());
    EXPECT_TRUE(state.liquidations.empty());
}

}
