#include "dlp/liquidator/LiquidationJobs.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <pqxx/pqxx>

#include "dlp/ethereum/Hex.hpp"

namespace dlp::liquidator
{

namespace
{

[[nodiscard]] ethereum::Hash256 ParseHash(std::string_view value)
{
    const auto bytes = ethereum::Hex::Decode(value);
    if(bytes.size() != ethereum::Hash256{}.size())
    {
        throw std::invalid_argument("liquidation job block hash must contain 32 bytes");
    }
    ethereum::Hash256 hash{};
    std::copy(bytes.begin(), bytes.end(), hash.begin());
    return hash;
}

[[nodiscard]] risk::LiquidationCandidate CandidateFromEvent(
    const messaging::EventEnvelope& event
)
{
    return risk::LiquidationCandidate{
        ethereum::Address::FromHex(event.payload.at("borrower").get<std::string>()),
        ethereum::Uint256::FromDecimal(event.payload.at("healthFactor").get<std::string>()),
        ethereum::Address::FromHex(event.payload.at("debtAsset").get<std::string>()),
        ethereum::Address::FromHex(event.payload.at("collateralAsset").get<std::string>()),
        ethereum::Uint256::FromDecimal(event.payload.at("maxRepay").get<std::string>()),
        ethereum::Uint256::FromDecimal(event.payload.at("expectedBonus").get<std::string>()),
        ethereum::Uint256::FromDecimal(event.payload.at("expectedCollateral").get<std::string>()),
        ethereum::Uint256::FromDecimal(event.payload.at("expectedBadDebt").get<std::string>()),
        event.blockNumber,
        event.blockHash,
        event.chainId,
        event.canonicalVersion
    };
}

[[nodiscard]] LiquidationJob ParseJob(const pqxx::row_ref& row)
{
    return LiquidationJob{
        row["job_id"].as<std::string>(),
        risk::LiquidationCandidate{
            ethereum::Address::FromHex(row["borrower_address"].as<std::string>()),
            ethereum::Uint256::FromDecimal(row["health_factor"].as<std::string>()),
            ethereum::Address::FromHex(row["debt_asset"].as<std::string>()),
            ethereum::Address::FromHex(row["collateral_asset"].as<std::string>()),
            ethereum::Uint256::FromDecimal(row["max_repay"].as<std::string>()),
            ethereum::Uint256::FromDecimal(row["expected_bonus"].as<std::string>()),
            ethereum::Uint256::FromDecimal(row["expected_collateral"].as<std::string>()),
            ethereum::Uint256::FromDecimal(row["expected_bad_debt"].as<std::string>()),
            row["block_number"].as<std::uint64_t>(),
            ParseHash(row["block_hash"].as<std::string>()),
            ethereum::Uint256::FromDecimal(row["chain_id"].as<std::string>()),
            row["canonical_version"].as<std::uint64_t>()
        },
        row["worker_id"].as<std::string>(),
        row["fencing_token"].as<std::uint64_t>()
    };
}

}

class PostgresLiquidationJobStore::Impl final
{
public:
    explicit Impl(std::string connectionString)
        : connectionString_(std::move(connectionString))
    {
    }

    [[nodiscard]] pqxx::connection Connect() const
    {
        return pqxx::connection{connectionString_};
    }

private:
    std::string connectionString_;
};

PostgresLiquidationJobStore::PostgresLiquidationJobStore(std::string connectionString)
    : implementation_(std::make_unique<Impl>(std::move(connectionString)))
{
}

PostgresLiquidationJobStore::~PostgresLiquidationJobStore() = default;
PostgresLiquidationJobStore::PostgresLiquidationJobStore(PostgresLiquidationJobStore&& other) noexcept = default;
PostgresLiquidationJobStore& PostgresLiquidationJobStore::operator=(
    PostgresLiquidationJobStore&& other
) noexcept = default;

bool PostgresLiquidationJobStore::Enqueue(const messaging::EventEnvelope& event)
{
    const auto candidate = CandidateFromEvent(event);
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    const auto processed = transaction.exec(
        R"SQL(
            INSERT INTO processed_events (consumer_name, event_id)
            VALUES ('liquidator', $1)
            ON CONFLICT (consumer_name, event_id) DO NOTHING
        )SQL",
        pqxx::params{event.eventId}
    );
    if(processed.affected_rows() == 0)
    {
        transaction.commit();
        return false;
    }

    transaction.exec(
        R"SQL(
            INSERT INTO liquidation_jobs
            (
                job_id,
                source_event_id,
                chain_id,
                borrower_address,
                debt_asset,
                collateral_asset,
                health_factor,
                max_repay,
                expected_bonus,
                expected_collateral,
                expected_bad_debt,
                block_number,
                block_hash,
                canonical_version,
                status
            )
            VALUES
            (
                $1, $2, $3, $4, $5, $6, $7, $8,
                $9, $10, $11, $12, $13, $14, 'Available'
            )
            ON CONFLICT (source_event_id) DO NOTHING
        )SQL",
        pqxx::params{
            event.eventId,
            event.eventId,
            candidate.chainId.ToDecimal(),
            candidate.borrower.ToHex(),
            candidate.debtAsset.ToHex(),
            candidate.collateralAsset.ToHex(),
            candidate.healthFactor.ToDecimal(),
            candidate.maxRepay.ToDecimal(),
            candidate.expectedBonus.ToDecimal(),
            candidate.expectedCollateral.ToDecimal(),
            candidate.expectedBadDebt.ToDecimal(),
            candidate.blockNumber,
            ethereum::Hex::Encode(candidate.blockHash),
            candidate.canonicalVersion
        }
    );
    transaction.commit();
    return true;
}

std::optional<LiquidationJob> PostgresLiquidationJobStore::Claim(
    const ethereum::Uint256& chainId,
    std::string_view workerId,
    std::chrono::seconds leaseDuration
)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    transaction.exec(
        R"SQL(
            UPDATE liquidation_jobs
            SET status = 'Expired', updated_at = NOW()
            WHERE chain_id = $1 AND status = 'Claimed' AND lease_until <= NOW()
        )SQL",
        pqxx::params{chainId.ToDecimal()}
    );
    const auto rows = transaction.exec(
        R"SQL(
            WITH next_job AS
            (
                SELECT job_id
                FROM liquidation_jobs
                WHERE chain_id = $1
                  AND status IN ('Available', 'Expired')
                ORDER BY created_at, job_id
                FOR UPDATE SKIP LOCKED
                LIMIT 1
            )
            UPDATE liquidation_jobs AS job
            SET status = 'Claimed',
                worker_id = $2,
                lease_until = NOW() + ($3::BIGINT * INTERVAL '1 second'),
                fencing_token = job.fencing_token + 1,
                error_message = NULL,
                updated_at = NOW()
            FROM next_job
            WHERE job.job_id = next_job.job_id
            RETURNING
                job.job_id,
                job.chain_id,
                job.borrower_address,
                job.debt_asset,
                job.collateral_asset,
                job.health_factor,
                job.max_repay,
                job.expected_bonus,
                job.expected_collateral,
                job.expected_bad_debt,
                job.block_number,
                job.block_hash,
                job.canonical_version,
                job.worker_id,
                job.fencing_token
        )SQL",
        pqxx::params{chainId.ToDecimal(), workerId, leaseDuration.count()}
    );
    if(rows.empty())
    {
        transaction.commit();
        return std::nullopt;
    }
    auto job = ParseJob(rows.front());
    transaction.commit();
    return job;
}

bool PostgresLiquidationJobStore::Submit(
    const LiquidationJob& job,
    const LiquidationSubmission& submission,
    const ethereum::Address& wallet
)
{
    const auto transactionJobId = "liquidation-tx:" + job.jobId;
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    transaction.exec(
        R"SQL(
            INSERT INTO tx_jobs
                (job_id, chain_id, wallet_address, to_address, value, calldata, status)
            VALUES ($1, $2, $3, $4, $5, $6, 'Pending')
            ON CONFLICT (job_id) DO NOTHING
        )SQL",
        pqxx::params{
            transactionJobId,
            job.candidate.chainId.ToDecimal(),
            wallet.ToHex(),
            submission.destination.ToHex(),
            submission.value.ToDecimal(),
            ethereum::Hex::Encode(submission.data)
        }
    );
    const auto updated = transaction.exec(
        R"SQL(
            UPDATE liquidation_jobs
            SET status = 'Submitted',
                tx_job_id = $4,
                updated_at = NOW()
            WHERE job_id = $1
              AND status = 'Claimed'
              AND worker_id = $2
              AND fencing_token = $3
              AND lease_until > NOW()
              AND EXISTS
              (
                  SELECT 1
                  FROM blocks AS canonical_block
                  WHERE canonical_block.chain_id = liquidation_jobs.chain_id
                    AND canonical_block.block_number = liquidation_jobs.block_number
                    AND canonical_block.block_hash = liquidation_jobs.block_hash
                    AND canonical_block.canonical
              )
        )SQL",
        pqxx::params{job.jobId, job.workerId, job.fencingToken, transactionJobId}
    );
    if(updated.affected_rows() == 0)
    {
        return false;
    }
    transaction.commit();
    return true;
}

bool PostgresLiquidationJobStore::Invalidate(
    const LiquidationJob& job,
    std::string_view reason
)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    const auto updated = transaction.exec(
        R"SQL(
            UPDATE liquidation_jobs
            SET status = 'Invalidated', error_message = $4, updated_at = NOW()
            WHERE job_id = $1
              AND status = 'Claimed'
              AND worker_id = $2
              AND fencing_token = $3
              AND lease_until > NOW()
        )SQL",
        pqxx::params{job.jobId, job.workerId, job.fencingToken, reason}
    );
    transaction.commit();
    return updated.affected_rows() == 1;
}

std::size_t PostgresLiquidationJobStore::InvalidateNonCanonical(
    const ethereum::Uint256& chainId
)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    const auto updated = transaction.exec(
        R"SQL(
            UPDATE liquidation_jobs AS job
            SET status = 'Invalidated',
                error_message = 'source block is no longer canonical',
                updated_at = NOW()
            WHERE job.chain_id = $1
              AND job.status IN ('Available', 'Claimed', 'Expired')
              AND NOT EXISTS
              (
                  SELECT 1
                  FROM blocks AS canonical_block
                  WHERE canonical_block.chain_id = job.chain_id
                    AND canonical_block.block_number = job.block_number
                    AND canonical_block.block_hash = job.block_hash
                    AND canonical_block.canonical
              )
        )SQL",
        pqxx::params{chainId.ToDecimal()}
    );
    transaction.commit();
    return static_cast<std::size_t>(updated.affected_rows());
}

std::size_t PostgresLiquidationJobStore::ReconcileSubmitted()
{
    const auto result = ReconcileSubmittedWithStats();
    return result.completed + result.failed + result.reorged;
}

LiquidationReconcileResult PostgresLiquidationJobStore::ReconcileSubmittedWithStats()
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    const auto updated = transaction.exec(
        R"SQL(
            UPDATE liquidation_jobs AS job
            SET status = CASE tx.status
                    WHEN 'Finalized' THEN 'Completed'
                    WHEN 'Reorged' THEN 'Invalidated'
                    ELSE 'Failed'
                END,
                error_message = tx.error_message,
                updated_at = NOW()
            FROM tx_jobs AS tx
            WHERE job.tx_job_id = tx.job_id
              AND job.status = 'Submitted'
              AND tx.status IN ('Finalized', 'Failed', 'Reorged')
            RETURNING tx.status
        )SQL"
    );
    transaction.commit();
    LiquidationReconcileResult result;
    for(const auto& row : updated)
    {
        const auto status = row["status"].as<std::string>();
        if(status == "Finalized") ++result.completed;
        else if(status == "Reorged") ++result.reorged;
        else ++result.failed;
    }
    return result;
}

}
