#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <string>
#include <utility>

#include <gtest/gtest.h>
#include <pqxx/pqxx>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/liquidator/LiquidationJobs.hpp"

namespace dlp::liquidator
{

namespace
{

ethereum::Address JobAddress(std::uint8_t suffix)
{
    ethereum::Address::Storage bytes{};
    bytes.back() = suffix;
    return ethereum::Address{bytes};
}

messaging::EventEnvelope LiquidationEvent(
    std::string eventId,
    std::uint64_t chainId = 91'337
)
{
    ethereum::Hash256 blockHash{};
    blockHash.back() = 9;
    return messaging::EventEnvelope{
        std::move(eventId),
        std::string{messaging::RISK_LIQUIDATION_DETECTED},
        "position:05",
        ethereum::Uint256{chainId},
        42,
        blockHash,
        std::nullopt,
        std::nullopt,
        7,
        1'700'000'000,
        {
            {"borrower", JobAddress(5).ToHex()},
            {"healthFactor", "900000000000000000"},
            {"debtAsset", JobAddress(4).ToHex()},
            {"collateralAsset", JobAddress(3).ToHex()},
            {"maxRepay", "5000000000"},
            {"expectedBonus", "250000000000000000000"},
            {"expectedCollateral", "1750000000000000000"},
            {"expectedBadDebt", "0"}
        }
    };
}

class TestRows final
{
public:
    TestRows(std::string connectionString, const messaging::EventEnvelope& event)
        : connectionString_(std::move(connectionString)),
          eventId_(event.eventId),
          chainId_(event.chainId.ToDecimal()),
          blockNumber_(event.blockNumber),
          blockHash_(ethereum::Hex::Encode(event.blockHash))
    {
        Remove();
        pqxx::connection connection{connectionString_};
        pqxx::work transaction{connection};
        transaction.exec(
            R"SQL(
                INSERT INTO blocks
                    (chain_id, block_number, block_hash, parent_hash, canonical)
                VALUES ($1, $2, $3, $4, TRUE)
            )SQL",
            pqxx::params{
                chainId_,
                blockNumber_,
                blockHash_,
                ethereum::Hex::Encode(ethereum::Hash256{})
            }
        );
        transaction.commit();
    }

    ~TestRows() noexcept(false)
    {
        Remove();
    }

    void ExpireLease() const
    {
        pqxx::connection connection{connectionString_};
        pqxx::work transaction{connection};
        transaction.exec(
            "UPDATE liquidation_jobs SET lease_until = NOW() - INTERVAL '1 second' WHERE job_id = $1",
            pqxx::params{eventId_}
        );
        transaction.commit();
    }

private:
    void Remove() const
    {
        pqxx::connection connection{connectionString_};
        pqxx::work transaction{connection};
        transaction.exec("DELETE FROM liquidation_jobs WHERE job_id = $1", pqxx::params{eventId_});
        transaction.exec("DELETE FROM tx_jobs WHERE job_id = $1", pqxx::params{"liquidation-tx:" + eventId_});
        transaction.exec(
            "DELETE FROM processed_events WHERE consumer_name = 'liquidator' AND event_id = $1",
            pqxx::params{eventId_}
        );
        transaction.exec(
            "DELETE FROM blocks WHERE chain_id = $1 AND block_hash = $2",
            pqxx::params{chainId_, blockHash_}
        );
        transaction.commit();
    }

    std::string connectionString_;
    std::string eventId_;
    std::string chainId_;
    std::uint64_t blockNumber_;
    std::string blockHash_;
};

}

TEST(LiquidationJobStoreTests, RejectsAWorkerAfterItsLeaseIsTakenOver)
{
    const auto* connectionString = std::getenv("DLP_TEST_DATABASE_URL");
    if(connectionString == nullptr || *connectionString == '\0')
    {
        GTEST_SKIP() << "set DLP_TEST_DATABASE_URL to run the PostgreSQL integration test";
    }

    const std::string eventId = "risk.liquidation.detected:lease-test";
    const auto event = LiquidationEvent(eventId);
    TestRows rows{connectionString, event};
    PostgresLiquidationJobStore store{connectionString};

    EXPECT_TRUE(store.Enqueue(event));
    EXPECT_FALSE(store.Enqueue(event));
    auto first = store.Claim(event.chainId, "worker-a", std::chrono::seconds{60});
    ASSERT_TRUE(first.has_value());

    rows.ExpireLease();
    auto second = store.Claim(event.chainId, "worker-b", std::chrono::seconds{60});
    ASSERT_TRUE(second.has_value());
    EXPECT_GT(second->fencingToken, first->fencingToken);

    const LiquidationSubmission submission{
        JobAddress(8),
        ethereum::Bytes{0x12, 0x34},
        ethereum::Uint256{}
    };
    EXPECT_FALSE(store.Submit(*first, submission, JobAddress(9)));
    EXPECT_TRUE(store.Submit(*second, submission, JobAddress(9)));
}

TEST(LiquidationJobStoreTests, AllowsOnlyOneConcurrentClaim)
{
    const auto* connectionString = std::getenv("DLP_TEST_DATABASE_URL");
    if(connectionString == nullptr || *connectionString == '\0')
    {
        GTEST_SKIP() << "set DLP_TEST_DATABASE_URL to run the PostgreSQL integration test";
    }

    const auto event = LiquidationEvent(
        "risk.liquidation.detected:concurrent-claim-test",
        91'338
    );
    TestRows rows{connectionString, event};
    PostgresLiquidationJobStore store{connectionString};
    ASSERT_TRUE(store.Enqueue(event));

    const std::string databaseUrl{connectionString};
    std::barrier start{3};
    const auto claim = [&](std::string workerId)
    {
        PostgresLiquidationJobStore workerStore{databaseUrl};
        start.arrive_and_wait();
        return workerStore.Claim(event.chainId, workerId, std::chrono::seconds{60});
    };
    auto first = std::async(std::launch::async, claim, "worker-a");
    auto second = std::async(std::launch::async, claim, "worker-b");
    start.arrive_and_wait();

    const auto firstClaim = first.get();
    const auto secondClaim = second.get();
    EXPECT_NE(firstClaim.has_value(), secondClaim.has_value());
}

}
