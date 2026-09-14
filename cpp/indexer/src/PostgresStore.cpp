#include "dlp/indexer/PostgresStore.hpp"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::indexer
{

namespace
{

using ethereum::Address;
using ethereum::Hash256;
using ethereum::Hex;
using ethereum::Uint256;

[[nodiscard]] Hash256 ParseHash(std::string_view value)
{
    const auto bytes = Hex::Decode(value);
    if(bytes.size() != Hash256{}.size())
    {
        throw std::invalid_argument("database hash must contain 32 bytes");
    }

    Hash256 result{};
    std::copy(bytes.begin(), bytes.end(), result.begin());
    return result;
}

[[nodiscard]] std::string EncodeTopics(const std::vector<Hash256>& topics)
{
    nlohmann::json result = nlohmann::json::array();
    for(const auto& topic : topics)
    {
        result.push_back(Hex::Encode(topic));
    }
    return result.dump();
}

[[nodiscard]] std::vector<Hash256> ParseTopics(std::string_view value)
{
    std::vector<Hash256> result;
    for(const auto& topic : nlohmann::json::parse(value))
    {
        result.push_back(ParseHash(topic.get<std::string>()));
    }
    return result;
}

[[nodiscard]] RawLog ParseRawLog(const pqxx::row_ref& row)
{
    return RawLog{
        Uint256::FromDecimal(row["chain_id"].as<std::string>()),
        row["block_number"].as<std::uint64_t>(),
        ParseHash(row["block_hash"].as<std::string>()),
        ParseHash(row["transaction_hash"].as<std::string>()),
        row["transaction_index"].as<std::uint64_t>(),
        row["log_index"].as<std::uint64_t>(),
        Address::FromHex(row["contract_address"].as<std::string>()),
        ParseTopics(row["topics"].as<std::string>()),
        Hex::Decode(row["data"].as<std::string>()),
        row["canonical"].as<bool>()
    };
}

void ReplaceDerivedState(
    pqxx::transaction_base& transaction,
    const Uint256& chainId,
    const DerivedState& state
)
{
    const auto chain = chainId.ToDecimal();
    transaction.exec("DELETE FROM positions WHERE chain_id = $1", pqxx::params{chain});
    transaction.exec("DELETE FROM markets WHERE chain_id = $1", pqxx::params{chain});
    transaction.exec("DELETE FROM liquidations WHERE chain_id = $1", pqxx::params{chain});

    for(const auto& [address, position] : state.positions)
    {
        transaction.exec(
            R"SQL(
                INSERT INTO positions
                    (chain_id, user_address, weth_collateral, scaled_usdc_supply, scaled_usdc_debt)
                VALUES ($1, $2, $3, $4, $5)
            )SQL",
            pqxx::params{
                chain,
                address.ToHex(),
                position.wethCollateral.ToDecimal(),
                position.scaledUsdcSupply.ToDecimal(),
                position.scaledUsdcDebt.ToDecimal()
            }
        );
    }

    const auto& market = state.market;
    transaction.exec(
        R"SQL(
            INSERT INTO markets
            (
                chain_id,
                pool_address,
                oracle_address,
                weth_address,
                usdc_address,
                total_weth_collateral,
                total_scaled_usdc_supply,
                total_scaled_usdc_debt,
                available_usdc_liquidity,
                protocol_reserve,
                bad_debt,
                borrow_index,
                liquidity_index,
                last_interest_timestamp,
                weth_price,
                weth_price_updated_at,
                weth_max_price_age,
                usdc_price,
                usdc_price_updated_at,
                usdc_max_price_age
            )
            VALUES
            (
                $1, $2, $3, $4, $5, $6, $7, $8, $9,
                $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20
            )
        )SQL",
        pqxx::params{
            chain,
            market.contracts.pool.ToHex(),
            market.contracts.oracle.ToHex(),
            market.contracts.weth.ToHex(),
            market.contracts.usdc.ToHex(),
            market.totalWethCollateral.ToDecimal(),
            market.totalScaledUsdcSupply.ToDecimal(),
            market.totalScaledUsdcDebt.ToDecimal(),
            market.availableUsdcLiquidity.ToDecimal(),
            market.protocolReserve.ToDecimal(),
            market.badDebt.ToDecimal(),
            market.borrowIndex.ToDecimal(),
            market.liquidityIndex.ToDecimal(),
            market.lastInterestTimestamp.ToDecimal(),
            market.wethPrice.ToDecimal(),
            market.wethPriceUpdatedAt.ToDecimal(),
            market.wethMaxPriceAge.ToDecimal(),
            market.usdcPrice.ToDecimal(),
            market.usdcPriceUpdatedAt.ToDecimal(),
            market.usdcMaxPriceAge.ToDecimal()
        }
    );

    for(const auto& liquidation : state.liquidations)
    {
        transaction.exec(
            R"SQL(
                INSERT INTO liquidations
                (
                    chain_id,
                    block_number,
                    block_hash,
                    transaction_hash,
                    log_index,
                    liquidator_address,
                    borrower_address,
                    debt_asset,
                    collateral_asset,
                    repaid_amount,
                    collateral_seized
                )
                VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
            )SQL",
            pqxx::params{
                chain,
                liquidation.blockNumber,
                Hex::Encode(liquidation.blockHash),
                Hex::Encode(liquidation.transactionHash),
                liquidation.logIndex,
                liquidation.liquidator.ToHex(),
                liquidation.borrower.ToHex(),
                liquidation.debtAsset.ToHex(),
                liquidation.collateralAsset.ToHex(),
                liquidation.repaidAmount.ToDecimal(),
                liquidation.collateralSeized.ToDecimal()
            }
        );
    }
}

}

class PostgresStore::Impl final
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

PostgresStore::PostgresStore(std::string connectionString)
    : implementation_(std::make_unique<Impl>(std::move(connectionString)))
{
}

PostgresStore::~PostgresStore() = default;

PostgresStore::PostgresStore(PostgresStore&& other) noexcept = default;

PostgresStore& PostgresStore::operator=(PostgresStore&& other) noexcept = default;

std::optional<SyncCursor> PostgresStore::LoadCursor(const Uint256& chainId) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        "SELECT block_number, block_hash FROM sync_state WHERE chain_id = $1",
        pqxx::params{chainId.ToDecimal()}
    );
    if(rows.empty())
    {
        return std::nullopt;
    }
    return SyncCursor{
        chainId,
        rows.front()["block_number"].as<std::uint64_t>(),
        ParseHash(rows.front()["block_hash"].as<std::string>())
    };
}

std::optional<IndexedBlock> PostgresStore::LoadCanonicalBlock(
    const Uint256& chainId,
    std::uint64_t blockNumber
) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT block_hash, parent_hash
            FROM blocks
            WHERE chain_id = $1 AND block_number = $2 AND canonical
        )SQL",
        pqxx::params{chainId.ToDecimal(), blockNumber}
    );
    if(rows.empty())
    {
        return std::nullopt;
    }
    return IndexedBlock{
        chainId,
        blockNumber,
        ParseHash(rows.front()["block_hash"].as<std::string>()),
        ParseHash(rows.front()["parent_hash"].as<std::string>()),
        true
    };
}

std::vector<RawLog> PostgresStore::LoadCanonicalLogsThrough(
    const Uint256& chainId,
    std::uint64_t blockNumber
) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT
                chain_id,
                block_number,
                block_hash,
                transaction_hash,
                transaction_index,
                log_index,
                contract_address,
                topics,
                data,
                canonical
            FROM raw_logs
            WHERE chain_id = $1 AND canonical AND block_number <= $2
            ORDER BY block_number, transaction_index, log_index
        )SQL",
        pqxx::params{chainId.ToDecimal(), blockNumber}
    );

    std::vector<RawLog> result;
    for(const auto& row : rows)
    {
        result.push_back(ParseRawLog(row));
    }
    return result;
}

DerivedState PostgresStore::LoadDerivedState(
    const Uint256& chainId,
    const ChainContracts& contracts
) const
{
    DerivedState state;
    state.market.contracts = contracts;
    state.market.borrowIndex = Uint256::FromDecimal("1000000000000000000000000000");
    state.market.liquidityIndex = state.market.borrowIndex;

    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto positions = transaction.exec(
        R"SQL(
            SELECT user_address, weth_collateral, scaled_usdc_supply, scaled_usdc_debt
            FROM positions
            WHERE chain_id = $1
        )SQL",
        pqxx::params{chainId.ToDecimal()}
    );
    for(const auto& row : positions)
    {
        const auto address = Address::FromHex(row["user_address"].as<std::string>());
        state.positions.emplace(
            address,
            PositionState{
                address,
                Uint256::FromDecimal(row["weth_collateral"].as<std::string>()),
                Uint256::FromDecimal(row["scaled_usdc_supply"].as<std::string>()),
                Uint256::FromDecimal(row["scaled_usdc_debt"].as<std::string>())
            }
        );
    }

    const auto markets = transaction.exec(
        R"SQL(
            SELECT
                pool_address,
                oracle_address,
                weth_address,
                usdc_address,
                total_weth_collateral,
                total_scaled_usdc_supply,
                total_scaled_usdc_debt,
                available_usdc_liquidity,
                protocol_reserve,
                bad_debt,
                borrow_index,
                liquidity_index,
                last_interest_timestamp,
                weth_price,
                weth_price_updated_at,
                weth_max_price_age,
                usdc_price,
                usdc_price_updated_at,
                usdc_max_price_age
            FROM markets
            WHERE chain_id = $1
        )SQL",
        pqxx::params{chainId.ToDecimal()}
    );
    if(!markets.empty())
    {
        const auto& row = markets.front();
        const ChainContracts storedContracts{
            Address::FromHex(row["pool_address"].as<std::string>()),
            Address::FromHex(row["oracle_address"].as<std::string>()),
            Address::FromHex(row["weth_address"].as<std::string>()),
            Address::FromHex(row["usdc_address"].as<std::string>())
        };
        if(storedContracts.pool != contracts.pool
            || storedContracts.oracle != contracts.oracle
            || storedContracts.weth != contracts.weth
            || storedContracts.usdc != contracts.usdc)
        {
            throw std::runtime_error("configured contract addresses do not match indexed market state");
        }
        state.market.totalWethCollateral = Uint256::FromDecimal(row["total_weth_collateral"].as<std::string>());
        state.market.totalScaledUsdcSupply = Uint256::FromDecimal(row["total_scaled_usdc_supply"].as<std::string>());
        state.market.totalScaledUsdcDebt = Uint256::FromDecimal(row["total_scaled_usdc_debt"].as<std::string>());
        state.market.availableUsdcLiquidity = Uint256::FromDecimal(row["available_usdc_liquidity"].as<std::string>());
        state.market.protocolReserve = Uint256::FromDecimal(row["protocol_reserve"].as<std::string>());
        state.market.badDebt = Uint256::FromDecimal(row["bad_debt"].as<std::string>());
        state.market.borrowIndex = Uint256::FromDecimal(row["borrow_index"].as<std::string>());
        state.market.liquidityIndex = Uint256::FromDecimal(row["liquidity_index"].as<std::string>());
        state.market.lastInterestTimestamp = Uint256::FromDecimal(row["last_interest_timestamp"].as<std::string>());
        state.market.wethPrice = Uint256::FromDecimal(row["weth_price"].as<std::string>());
        state.market.wethPriceUpdatedAt = Uint256::FromDecimal(row["weth_price_updated_at"].as<std::string>());
        state.market.wethMaxPriceAge = Uint256::FromDecimal(row["weth_max_price_age"].as<std::string>());
        state.market.usdcPrice = Uint256::FromDecimal(row["usdc_price"].as<std::string>());
        state.market.usdcPriceUpdatedAt = Uint256::FromDecimal(row["usdc_price_updated_at"].as<std::string>());
        state.market.usdcMaxPriceAge = Uint256::FromDecimal(row["usdc_max_price_age"].as<std::string>());
    }

    const auto liquidations = transaction.exec(
        R"SQL(
            SELECT
                block_number,
                block_hash,
                transaction_hash,
                log_index,
                liquidator_address,
                borrower_address,
                debt_asset,
                collateral_asset,
                repaid_amount,
                collateral_seized
            FROM liquidations
            WHERE chain_id = $1
            ORDER BY block_number, log_index
        )SQL",
        pqxx::params{chainId.ToDecimal()}
    );
    for(const auto& row : liquidations)
    {
        state.liquidations.push_back(LiquidationState{
            chainId,
            row["block_number"].as<std::uint64_t>(),
            ParseHash(row["block_hash"].as<std::string>()),
            ParseHash(row["transaction_hash"].as<std::string>()),
            row["log_index"].as<std::uint64_t>(),
            Address::FromHex(row["liquidator_address"].as<std::string>()),
            Address::FromHex(row["borrower_address"].as<std::string>()),
            Address::FromHex(row["debt_asset"].as<std::string>()),
            Address::FromHex(row["collateral_asset"].as<std::string>()),
            Uint256::FromDecimal(row["repaid_amount"].as<std::string>()),
            Uint256::FromDecimal(row["collateral_seized"].as<std::string>())
        });
    }

    return state;
}

void PostgresStore::CommitBlock(
    const IndexedBlock& block,
    const std::vector<RawLog>& logs,
    const DerivedState& state
)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    const auto chain = block.chainId.ToDecimal();

    transaction.exec(
        R"SQL(
            INSERT INTO blocks (chain_id, block_number, block_hash, parent_hash, canonical)
            VALUES ($1, $2, $3, $4, TRUE)
            ON CONFLICT (chain_id, block_hash)
            DO UPDATE SET block_number = EXCLUDED.block_number,
                          parent_hash = EXCLUDED.parent_hash,
                          canonical = TRUE,
                          indexed_at = NOW()
        )SQL",
        pqxx::params{
            chain,
            block.number,
            Hex::Encode(block.hash),
            Hex::Encode(block.parentHash)
        }
    );

    for(const auto& log : logs)
    {
        transaction.exec(
            R"SQL(
                INSERT INTO raw_logs
                (
                    chain_id,
                    block_number,
                    block_hash,
                    transaction_hash,
                    transaction_index,
                    log_index,
                    contract_address,
                    topics,
                    data,
                    canonical
                )
                VALUES ($1, $2, $3, $4, $5, $6, $7, $8::jsonb, $9, TRUE)
                ON CONFLICT (chain_id, block_hash, transaction_hash, log_index)
                DO UPDATE SET block_number = EXCLUDED.block_number,
                              transaction_index = EXCLUDED.transaction_index,
                              contract_address = EXCLUDED.contract_address,
                              topics = EXCLUDED.topics,
                              data = EXCLUDED.data,
                              canonical = TRUE,
                              indexed_at = NOW()
            )SQL",
            pqxx::params{
                chain,
                log.blockNumber,
                Hex::Encode(log.blockHash),
                Hex::Encode(log.transactionHash),
                log.transactionIndex,
                log.logIndex,
                log.contractAddress.ToHex(),
                EncodeTopics(log.topics),
                Hex::Encode(log.data)
            }
        );
    }

    ReplaceDerivedState(transaction, block.chainId, state);
    transaction.exec(
        R"SQL(
            INSERT INTO sync_state (chain_id, block_number, block_hash)
            VALUES ($1, $2, $3)
            ON CONFLICT (chain_id)
            DO UPDATE SET block_number = EXCLUDED.block_number,
                          block_hash = EXCLUDED.block_hash,
                          updated_at = NOW()
        )SQL",
        pqxx::params{chain, block.number, Hex::Encode(block.hash)}
    );
    transaction.commit();
}

void PostgresStore::RewindTo(
    const Uint256& chainId,
    const std::optional<IndexedBlock>& ancestor,
    const DerivedState& state
)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    const auto chain = chainId.ToDecimal();

    if(ancestor.has_value())
    {
        transaction.exec(
            "UPDATE blocks SET canonical = FALSE WHERE chain_id = $1 AND canonical AND block_number > $2",
            pqxx::params{chain, ancestor->number}
        );
        transaction.exec(
            "UPDATE raw_logs SET canonical = FALSE WHERE chain_id = $1 AND canonical AND block_number > $2",
            pqxx::params{chain, ancestor->number}
        );
    }
    else
    {
        transaction.exec(
            "UPDATE blocks SET canonical = FALSE WHERE chain_id = $1 AND canonical",
            pqxx::params{chain}
        );
        transaction.exec(
            "UPDATE raw_logs SET canonical = FALSE WHERE chain_id = $1 AND canonical",
            pqxx::params{chain}
        );
    }

    ReplaceDerivedState(transaction, chainId, state);
    if(ancestor.has_value())
    {
        transaction.exec(
            R"SQL(
                INSERT INTO sync_state (chain_id, block_number, block_hash)
                VALUES ($1, $2, $3)
                ON CONFLICT (chain_id)
                DO UPDATE SET block_number = EXCLUDED.block_number,
                              block_hash = EXCLUDED.block_hash,
                              updated_at = NOW()
            )SQL",
            pqxx::params{chain, ancestor->number, Hex::Encode(ancestor->hash)}
        );
    }
    else
    {
        transaction.exec("DELETE FROM sync_state WHERE chain_id = $1", pqxx::params{chain});
    }
    transaction.commit();
}

}
