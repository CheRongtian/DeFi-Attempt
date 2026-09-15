#include "dlp/tx/PostgresTransactionStore.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <pqxx/pqxx>

#include "dlp/ethereum/Hex.hpp"

namespace dlp::tx
{

namespace
{

template<typename Value>
[[nodiscard]] std::optional<Value> OptionalField(
    const pqxx::row_ref& row,
    std::string_view field,
    auto parser
)
{
    const auto value = row[pqxx::zview{field}];
    return value.is_null() ? std::nullopt : std::optional<Value>{parser(value)};
}

[[nodiscard]] ethereum::Hash256 ParseHash(std::string_view value)
{
    const auto bytes = ethereum::Hex::Decode(value);
    if(bytes.size() != ethereum::Hash256{}.size())
    {
        throw std::invalid_argument("database transaction hash must contain 32 bytes");
    }
    ethereum::Hash256 result{};
    std::copy(bytes.begin(), bytes.end(), result.begin());
    return result;
}

[[nodiscard]] TxJob ParseJob(const pqxx::row_ref& row)
{
    return TxJob{
        row["job_id"].as<std::string>(),
        ethereum::Uint256::FromDecimal(row["chain_id"].as<std::string>()),
        ethereum::Address::FromHex(row["wallet_address"].as<std::string>()),
        ethereum::Address::FromHex(row["to_address"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["value"].as<std::string>()),
        ethereum::Hex::Decode(row["calldata"].as<std::string>()),
        TxStatusFromString(row["status"].as<std::string>()),
        OptionalField<ethereum::Uint256>(row, "nonce", [](const auto& field) {
            return ethereum::Uint256::FromDecimal(field.template as<std::string>());
        }),
        OptionalField<ethereum::Uint256>(row, "max_priority_fee_per_gas", [](const auto& field) {
            return ethereum::Uint256::FromDecimal(field.template as<std::string>());
        }),
        OptionalField<ethereum::Uint256>(row, "max_fee_per_gas", [](const auto& field) {
            return ethereum::Uint256::FromDecimal(field.template as<std::string>());
        }),
        OptionalField<ethereum::Uint256>(row, "gas_limit", [](const auto& field) {
            return ethereum::Uint256::FromDecimal(field.template as<std::string>());
        }),
        row["raw_transaction"].is_null()
            ? ethereum::Bytes{}
            : ethereum::Hex::Decode(row["raw_transaction"].as<std::string>()),
        OptionalField<ethereum::Hash256>(row, "tx_hash", [](const auto& field) {
            return ParseHash(field.template as<std::string>());
        }),
        row["retry_count"].as<std::uint32_t>(),
        OptionalField<std::uint64_t>(row, "submitted_block_number", [](const auto& field) {
            return field.template as<std::uint64_t>();
        }),
        OptionalField<std::uint64_t>(row, "included_block_number", [](const auto& field) {
            return field.template as<std::uint64_t>();
        }),
        OptionalField<ethereum::Hash256>(row, "included_block_hash", [](const auto& field) {
            return ParseHash(field.template as<std::string>());
        }),
        row["confirmation_count"].as<std::uint64_t>(),
        row["error_message"].is_null() ? std::string{} : row["error_message"].as<std::string>()
    };
}

constexpr std::string_view SELECT_COLUMNS = R"SQL(
    SELECT
        job_id,
        chain_id,
        wallet_address,
        to_address,
        value,
        calldata,
        status,
        nonce,
        max_priority_fee_per_gas,
        max_fee_per_gas,
        gas_limit,
        raw_transaction,
        tx_hash,
        retry_count,
        submitted_block_number,
        included_block_number,
        included_block_hash,
        confirmation_count,
        error_message
    FROM tx_jobs
)SQL";

}

class PostgresTransactionStore::Impl final
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

PostgresTransactionStore::PostgresTransactionStore(std::string connectionString)
    : implementation_(std::make_unique<Impl>(std::move(connectionString)))
{
}

PostgresTransactionStore::~PostgresTransactionStore() = default;
PostgresTransactionStore::PostgresTransactionStore(PostgresTransactionStore&& other) noexcept = default;
PostgresTransactionStore& PostgresTransactionStore::operator=(PostgresTransactionStore&& other) noexcept = default;

bool PostgresTransactionStore::Insert(const TxJob& job)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    const auto result = transaction.exec(
        R"SQL(
            INSERT INTO tx_jobs
                (job_id, chain_id, wallet_address, to_address, value, calldata, status)
            VALUES ($1, $2, $3, $4, $5, $6, $7)
            ON CONFLICT (job_id) DO NOTHING
        )SQL",
        pqxx::params{
            job.jobId,
            job.chainId.ToDecimal(),
            job.wallet.ToHex(),
            job.to.ToHex(),
            job.value.ToDecimal(),
            ethereum::Hex::Encode(job.data),
            std::string{ToString(job.status)}
        }
    );
    transaction.commit();
    return result.affected_rows() == 1;
}

std::vector<TxJob> PostgresTransactionStore::LoadActive(
    const ethereum::Uint256& chainId,
    const ethereum::Address& wallet
) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        std::string{SELECT_COLUMNS}
            + " WHERE chain_id = $1 AND wallet_address = $2"
              " AND status IN ('Pending', 'Submitted', 'Included') ORDER BY created_at, job_id",
        pqxx::params{chainId.ToDecimal(), wallet.ToHex()}
    );
    std::vector<TxJob> result;
    result.reserve(static_cast<std::size_t>(rows.size()));
    for(const auto& row : rows)
    {
        result.push_back(ParseJob(row));
    }
    return result;
}

std::optional<ethereum::Uint256> PostgresTransactionStore::LoadHighestNonce(
    const ethereum::Uint256& chainId,
    const ethereum::Address& wallet
) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT MAX(nonce) AS nonce
            FROM tx_jobs
            WHERE chain_id = $1
              AND wallet_address = $2
              AND nonce IS NOT NULL
              AND status NOT IN ('Failed', 'Reorged')
        )SQL",
        pqxx::params{chainId.ToDecimal(), wallet.ToHex()}
    );
    if(rows.empty() || rows.front()["nonce"].is_null())
    {
        return std::nullopt;
    }
    return ethereum::Uint256::FromDecimal(rows.front()["nonce"].as<std::string>());
}

void PostgresTransactionStore::Save(const TxJob& job)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    transaction.exec(
        R"SQL(
            UPDATE tx_jobs SET
                status = $2,
                nonce = $3,
                max_priority_fee_per_gas = $4,
                max_fee_per_gas = $5,
                gas_limit = $6,
                raw_transaction = $7,
                tx_hash = $8,
                retry_count = $9,
                submitted_block_number = $10,
                included_block_number = $11,
                included_block_hash = $12,
                confirmation_count = $13,
                error_message = $14,
                updated_at = NOW()
            WHERE job_id = $1
        )SQL",
        pqxx::params{
            job.jobId,
            std::string{ToString(job.status)},
            job.nonce.has_value() ? std::optional<std::string>{job.nonce->ToDecimal()} : std::nullopt,
            job.maxPriorityFeePerGas.has_value()
                ? std::optional<std::string>{job.maxPriorityFeePerGas->ToDecimal()}
                : std::nullopt,
            job.maxFeePerGas.has_value()
                ? std::optional<std::string>{job.maxFeePerGas->ToDecimal()}
                : std::nullopt,
            job.gasLimit.has_value() ? std::optional<std::string>{job.gasLimit->ToDecimal()} : std::nullopt,
            job.rawTransaction.empty()
                ? std::optional<std::string>{}
                : std::optional<std::string>{ethereum::Hex::Encode(job.rawTransaction)},
            job.transactionHash.has_value()
                ? std::optional<std::string>{ethereum::Hex::Encode(*job.transactionHash)}
                : std::nullopt,
            job.retryCount,
            job.submittedBlockNumber,
            job.includedBlockNumber,
            job.includedBlockHash.has_value()
                ? std::optional<std::string>{ethereum::Hex::Encode(*job.includedBlockHash)}
                : std::nullopt,
            job.confirmationCount,
            job.errorMessage.empty() ? std::optional<std::string>{} : std::optional<std::string>{job.errorMessage}
        }
    );
    transaction.commit();
}

}
