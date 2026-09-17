#include "dlp/api/ApiStore.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <pqxx/pqxx>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Uint256Math.hpp"
#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::api
{

namespace
{

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

}

class PostgresApiStore::Impl final
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

PostgresApiStore::PostgresApiStore(std::string connectionString)
    : implementation_(std::make_unique<Impl>(std::move(connectionString)))
{
}

PostgresApiStore::~PostgresApiStore() = default;
PostgresApiStore::PostgresApiStore(PostgresApiStore&& other) noexcept = default;
PostgresApiStore& PostgresApiStore::operator=(PostgresApiStore&& other) noexcept = default;

std::vector<LiquidationRecord> PostgresApiStore::LoadLiquidations(
    const ethereum::Uint256& chainId,
    std::size_t limit
) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT
                block_number,
                transaction_hash,
                liquidator_address,
                borrower_address,
                repaid_amount,
                collateral_seized
            FROM liquidations
            WHERE chain_id = $1
            ORDER BY block_number DESC, log_index DESC
            LIMIT $2
        )SQL",
        pqxx::params{chainId.ToDecimal(), limit}
    );

    std::vector<LiquidationRecord> result;
    result.reserve(static_cast<std::size_t>(rows.size()));
    for(const auto& row : rows)
    {
        result.push_back(LiquidationRecord{
            row["block_number"].as<std::uint64_t>(),
            ParseHash(row["transaction_hash"].as<std::string>()),
            ethereum::Address::FromHex(row["liquidator_address"].as<std::string>()),
            ethereum::Address::FromHex(row["borrower_address"].as<std::string>()),
            ethereum::Uint256::FromDecimal(row["repaid_amount"].as<std::string>()),
            ethereum::Uint256::FromDecimal(row["collateral_seized"].as<std::string>())
        });
    }
    return result;
}

ProtocolStats PostgresApiStore::LoadProtocolStats(const ethereum::Uint256& chainId) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT
                (SELECT COUNT(*) FROM positions WHERE chain_id = $1) AS position_count,
                (SELECT COUNT(*) FROM liquidations WHERE chain_id = $1) AS liquidation_count,
                total_weth_collateral,
                total_scaled_usdc_debt,
                available_usdc_liquidity,
                protocol_reserve,
                bad_debt
            FROM markets
            WHERE chain_id = $1
        )SQL",
        pqxx::params{chainId.ToDecimal()}
    );
    if(rows.empty())
    {
        throw std::runtime_error("indexed protocol statistics are unavailable");
    }
    const auto& row = rows.front();
    return ProtocolStats{
        row["position_count"].as<std::uint64_t>(),
        row["liquidation_count"].as<std::uint64_t>(),
        ethereum::Uint256::FromDecimal(row["total_weth_collateral"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["total_scaled_usdc_debt"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["available_usdc_liquidity"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["protocol_reserve"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["bad_debt"].as<std::string>())
    };
}

ethereum::Uint256 PostgresApiStore::LoadUsdcSupply(
    const ethereum::Uint256& chainId,
    const ethereum::Address& user
) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT p.scaled_usdc_supply, m.liquidity_index
            FROM positions p
            JOIN markets m ON m.chain_id = p.chain_id
            WHERE p.chain_id = $1 AND p.user_address = $2
        )SQL",
        pqxx::params{chainId.ToDecimal(), user.ToHex()}
    );
    if(rows.empty())
    {
        return ethereum::Uint256{};
    }

    const auto& row = rows.front();
    return ethereum::Uint256Math::MulDivDown(
        ethereum::Uint256::FromDecimal(row["scaled_usdc_supply"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["liquidity_index"].as<std::string>()),
        risk::RiskCalculator::Ray()
    );
}

}
