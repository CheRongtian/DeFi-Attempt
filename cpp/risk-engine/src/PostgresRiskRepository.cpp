#include "dlp/risk/PostgresRiskRepository.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <pqxx/pqxx>

#include "dlp/ethereum/Hex.hpp"

namespace dlp::risk
{

namespace
{

[[nodiscard]] ethereum::Hash256 ParseHash(std::string_view value)
{
    const auto bytes = ethereum::Hex::Decode(value);
    if(bytes.size() != ethereum::Hash256{}.size())
    {
        throw std::invalid_argument("database block hash must contain 32 bytes");
    }
    ethereum::Hash256 result{};
    std::copy(bytes.begin(), bytes.end(), result.begin());
    return result;
}

[[nodiscard]] IndexedPosition ParsePosition(const pqxx::row_ref& row)
{
    return IndexedPosition{
        ethereum::Address::FromHex(row["user_address"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["weth_collateral"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["scaled_usdc_debt"].as<std::string>())
    };
}

}

class PostgresRiskRepository::Impl final
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

PostgresRiskRepository::PostgresRiskRepository(std::string connectionString)
    : implementation_(std::make_unique<Impl>(std::move(connectionString)))
{
}

PostgresRiskRepository::~PostgresRiskRepository() = default;
PostgresRiskRepository::PostgresRiskRepository(PostgresRiskRepository&& other) noexcept = default;
PostgresRiskRepository& PostgresRiskRepository::operator=(PostgresRiskRepository&& other) noexcept = default;

MarketSnapshot PostgresRiskRepository::LoadMarket(const ethereum::Uint256& chainId) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT
                m.pool_address,
                m.oracle_address,
                m.weth_address,
                m.usdc_address,
                m.borrow_index,
                m.available_usdc_liquidity,
                m.total_scaled_usdc_debt,
                m.last_interest_timestamp,
                m.weth_price,
                m.weth_price_updated_at,
                m.weth_max_price_age,
                m.usdc_price,
                m.usdc_price_updated_at,
                m.usdc_max_price_age,
                s.block_number,
                s.block_hash
            FROM markets m
            JOIN sync_state s ON s.chain_id = m.chain_id
            WHERE m.chain_id = $1
        )SQL",
        pqxx::params{chainId.ToDecimal()}
    );
    if(rows.empty())
    {
        throw std::runtime_error("indexed market state is unavailable");
    }

    const auto& row = rows.front();
    return MarketSnapshot{
        chainId,
        row["block_number"].as<std::uint64_t>(),
        ParseHash(row["block_hash"].as<std::string>()),
        ethereum::Address::FromHex(row["pool_address"].as<std::string>()),
        ethereum::Address::FromHex(row["oracle_address"].as<std::string>()),
        ethereum::Address::FromHex(row["weth_address"].as<std::string>()),
        ethereum::Address::FromHex(row["usdc_address"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["borrow_index"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["weth_price"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["weth_price_updated_at"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["weth_max_price_age"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["usdc_price"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["usdc_price_updated_at"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["usdc_max_price_age"].as<std::string>()),
        0,
        ethereum::Uint256::FromDecimal(row["available_usdc_liquidity"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["total_scaled_usdc_debt"].as<std::string>()),
        ethereum::Uint256::FromDecimal(row["last_interest_timestamp"].as<std::string>())
    };
}

std::optional<IndexedPosition> PostgresRiskRepository::LoadPosition(
    const ethereum::Uint256& chainId,
    const ethereum::Address& user
) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT user_address, weth_collateral, scaled_usdc_debt
            FROM positions
            WHERE chain_id = $1 AND user_address = $2
        )SQL",
        pqxx::params{chainId.ToDecimal(), user.ToHex()}
    );
    return rows.empty() ? std::nullopt : std::optional<IndexedPosition>{ParsePosition(rows.front())};
}

std::vector<IndexedPosition> PostgresRiskRepository::LoadPositions(
    const ethereum::Uint256& chainId,
    std::size_t limit
) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT user_address, weth_collateral, scaled_usdc_debt
            FROM positions
            WHERE chain_id = $1 AND scaled_usdc_debt > 0
            ORDER BY user_address
            LIMIT $2
        )SQL",
        pqxx::params{chainId.ToDecimal(), limit}
    );

    std::vector<IndexedPosition> result;
    result.reserve(static_cast<std::size_t>(rows.size()));
    for(const auto& row : rows)
    {
        result.push_back(ParsePosition(row));
    }
    return result;
}

}
