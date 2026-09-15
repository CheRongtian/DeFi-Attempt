#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dlp/tx/TxManager.hpp"

namespace dlp::tx
{

namespace
{

ethereum::Address Address(std::uint8_t suffix)
{
    ethereum::Address::Storage bytes{};
    bytes.back() = suffix;
    return ethereum::Address{bytes};
}

class MemoryStore final : public TransactionStore
{
public:
    std::map<std::string, TxJob> jobs;
    std::vector<TxStatus> savedStatuses;

    bool Insert(const TxJob& job) override
    {
        return jobs.emplace(job.jobId, job).second;
    }

    [[nodiscard]] std::vector<TxJob> LoadActive(
        const ethereum::Uint256&,
        const ethereum::Address&
    ) const override
    {
        std::vector<TxJob> result;
        for(const auto& entry : jobs)
        {
            const auto& job = entry.second;
            if(job.status == TxStatus::Pending || job.status == TxStatus::Submitted
                || job.status == TxStatus::Included)
            {
                result.push_back(job);
            }
        }
        return result;
    }

    [[nodiscard]] std::optional<ethereum::Uint256> LoadHighestNonce(
        const ethereum::Uint256&,
        const ethereum::Address&
    ) const override
    {
        std::optional<ethereum::Uint256> result;
        for(const auto& entry : jobs)
        {
            const auto& job = entry.second;
            if(job.nonce.has_value() && (!result.has_value() || *job.nonce > *result))
            {
                result = job.nonce;
            }
        }
        return result;
    }

    void Save(const TxJob& job) override
    {
        jobs[job.jobId] = job;
        savedStatuses.push_back(job.status);
    }
};

class MemoryRpc final : public TransactionRpc
{
public:
    std::uint64_t head{10};
    mutable std::size_t sendCount{0};
    mutable std::size_t failedSendsRemaining{0};
    std::optional<ethereum::TransactionReceipt> receipt;
    ethereum::Hash256 canonicalHash{};

    [[nodiscard]] ethereum::Uint256 GetChainId() const override { return ethereum::Uint256{31337}; }
    [[nodiscard]] std::uint64_t GetBlockNumber() const override { return head; }
    [[nodiscard]] std::optional<ethereum::BlockHeader> GetBlock(std::uint64_t number) const override
    {
        return ethereum::BlockHeader{ethereum::Uint256{number}, canonicalHash, {}, {}, std::nullopt};
    }
    [[nodiscard]] ethereum::Uint256 GetPendingNonce(const ethereum::Address&) const override
    {
        return ethereum::Uint256{7};
    }
    [[nodiscard]] ethereum::Uint256 EstimateGas(const ethereum::TransactionCall&) const override
    {
        return ethereum::Uint256{100'000};
    }
    [[nodiscard]] FeeQuote GetFeeQuote() const override
    {
        return FeeQuote{ethereum::Uint256{1}, ethereum::Uint256{2}};
    }
    [[nodiscard]] ethereum::Hash256 SendRawTransaction(const ethereum::Bytes& raw) const override
    {
        ++sendCount;
        if(failedSendsRemaining != 0U)
        {
            --failedSendsRemaining;
            throw ethereum::RpcException(ethereum::RpcErrorKind::Remote, "transaction rejected");
        }
        return ethereum::Keccak::Hash(raw);
    }
    [[nodiscard]] std::optional<ethereum::TransactionReceipt> GetReceipt(
        const ethereum::Hash256&
    ) const override
    {
        return receipt;
    }
};

}

TEST(TxManagerTests, QueuesSubmitsIncludesAndFinalizesATransaction)
{
    MemoryStore store;
    MemoryRpc rpc;
    ethereum::Secp256k1Signer signer{
        "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
    };
    TxManager manager{store, rpc, signer};

    ASSERT_TRUE(manager.Queue("liquidation:1", Address(2), {1, 2, 3}));
    manager.RunOnce();
    ASSERT_EQ(store.jobs.at("liquidation:1").status, TxStatus::Submitted);
    EXPECT_EQ(store.jobs.at("liquidation:1").nonce, ethereum::Uint256{7});

    ethereum::Hash256 blockHash{};
    blockHash.back() = 9;
    rpc.canonicalHash = blockHash;
    rpc.receipt = ethereum::TransactionReceipt{
        *store.jobs.at("liquidation:1").transactionHash,
        ethereum::Uint256{10},
        blockHash,
        true,
        ethereum::Uint256{90'000},
        ethereum::Uint256{2}
    };
    manager.RunOnce();
    EXPECT_EQ(store.jobs.at("liquidation:1").status, TxStatus::Included);
    manager.RunOnce();
    EXPECT_EQ(store.jobs.at("liquidation:1").status, TxStatus::Finalized);
}

TEST(TxManagerTests, MarksAnOrphanedInclusionAsReorged)
{
    MemoryStore store;
    MemoryRpc rpc;
    ethereum::Secp256k1Signer signer{
        "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
    };
    TxManager manager{store, rpc, signer, TxManagerConfig{2, 3, 3}};
    ASSERT_TRUE(manager.Queue("liquidation:2", Address(2), {1}));
    manager.RunOnce();

    ethereum::Hash256 includedHash{};
    includedHash.back() = 4;
    rpc.receipt = ethereum::TransactionReceipt{
        *store.jobs.at("liquidation:2").transactionHash,
        ethereum::Uint256{10},
        includedHash,
        true,
        ethereum::Uint256{90'000},
        ethereum::Uint256{2}
    };
    manager.RunOnce();
    manager.RunOnce();
    EXPECT_EQ(store.jobs.at("liquidation:2").status, TxStatus::Reorged);
}

TEST(TxManagerTests, ReplacesAStaleSubmissionWithTheSameNonce)
{
    MemoryStore store;
    MemoryRpc rpc;
    ethereum::Secp256k1Signer signer{
        "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
    };
    TxManager manager{store, rpc, signer, TxManagerConfig{1, 1, 2}};
    ASSERT_TRUE(manager.Queue("liquidation:3", Address(2), {1}));
    manager.RunOnce();
    const auto originalNonce = store.jobs.at("liquidation:3").nonce;

    rpc.head = 11;
    manager.RunOnce();
    EXPECT_EQ(store.jobs.at("liquidation:3").status, TxStatus::Submitted);
    EXPECT_EQ(store.jobs.at("liquidation:3").nonce, originalNonce);
    EXPECT_EQ(store.jobs.at("liquidation:3").retryCount, 1U);
    EXPECT_EQ(rpc.sendCount, 2U);
    EXPECT_NE(
        std::find(store.savedStatuses.begin(), store.savedStatuses.end(), TxStatus::Replaced),
        store.savedStatuses.end()
    );
}

TEST(TxManagerTests, AllocatesAfterTheHighestRecoveredNonce)
{
    MemoryStore store;
    MemoryRpc rpc;
    ethereum::Secp256k1Signer signer{
        "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
    };
    TxJob recovered;
    recovered.jobId = "existing";
    recovered.chainId = ethereum::Uint256{31337};
    recovered.wallet = signer.GetAddress();
    recovered.to = Address(2);
    recovered.data = {1};
    recovered.status = TxStatus::Included;
    recovered.nonce = ethereum::Uint256{8};
    recovered.transactionHash = ethereum::Hash256{};
    recovered.includedBlockNumber = 10;
    recovered.includedBlockHash = ethereum::Hash256{};
    store.jobs.emplace(recovered.jobId, recovered);

    TxManager manager{store, rpc, signer};
    ASSERT_TRUE(manager.Queue("next", Address(2), {2}));
    manager.RunOnce();
    EXPECT_EQ(store.jobs.at("next").nonce, ethereum::Uint256{9});
}

TEST(TxManagerTests, CountsEachSubmissionRetryOnce)
{
    MemoryStore store;
    MemoryRpc rpc;
    rpc.failedSendsRemaining = 3;
    ethereum::Secp256k1Signer signer{
        "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
    };
    TxManager manager{store, rpc, signer, TxManagerConfig{1, 3, 2}};

    ASSERT_TRUE(manager.Queue("liquidation:retry", Address(2), {1}));
    manager.RunOnce();
    EXPECT_EQ(store.jobs.at("liquidation:retry").status, TxStatus::Pending);
    EXPECT_EQ(store.jobs.at("liquidation:retry").retryCount, 0U);

    manager.RunOnce();
    EXPECT_EQ(store.jobs.at("liquidation:retry").status, TxStatus::Pending);
    EXPECT_EQ(store.jobs.at("liquidation:retry").retryCount, 1U);

    manager.RunOnce();
    EXPECT_EQ(store.jobs.at("liquidation:retry").status, TxStatus::Failed);
    EXPECT_EQ(store.jobs.at("liquidation:retry").retryCount, 2U);
    EXPECT_EQ(rpc.sendCount, 3U);
}

}
