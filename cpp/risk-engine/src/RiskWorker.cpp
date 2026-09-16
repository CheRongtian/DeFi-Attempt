#include "dlp/risk/RiskWorker.hpp"

#include <utility>

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/messaging/Event.hpp"

namespace dlp::risk
{

namespace
{

[[nodiscard]] std::string RiskEventId(const LiquidationCandidate& candidate)
{
    return std::string{messaging::RISK_LIQUIDATION_DETECTED} + ":"
        + candidate.chainId.ToDecimal() + ":"
        + ethereum::Hex::Encode(candidate.blockHash, false) + ":"
        + candidate.borrower.ToHex(false);
}

[[nodiscard]] nlohmann::json CandidatePayload(const LiquidationCandidate& candidate)
{
    return {
        {"borrower", candidate.borrower.ToHex()},
        {"healthFactor", candidate.healthFactor.ToDecimal()},
        {"debtAsset", candidate.debtAsset.ToHex()},
        {"collateralAsset", candidate.collateralAsset.ToHex()},
        {"maxRepay", candidate.maxRepay.ToDecimal()},
        {"expectedBonus", candidate.expectedBonus.ToDecimal()},
        {"expectedCollateral", candidate.expectedCollateral.ToDecimal()},
        {"expectedBadDebt", candidate.expectedBadDebt.ToDecimal()}
    };
}

}

class PostgresRiskEventStore::Impl final
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

PostgresRiskEventStore::PostgresRiskEventStore(std::string connectionString)
    : implementation_(std::make_unique<Impl>(std::move(connectionString)))
{
}

PostgresRiskEventStore::~PostgresRiskEventStore() = default;
PostgresRiskEventStore::PostgresRiskEventStore(PostgresRiskEventStore&& other) noexcept = default;
PostgresRiskEventStore& PostgresRiskEventStore::operator=(PostgresRiskEventStore&& other) noexcept = default;

bool PostgresRiskEventStore::CommitScan(
    std::string_view sourceEventId,
    const std::vector<LiquidationCandidate>& candidates
)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    const auto processed = transaction.exec(
        R"SQL(
            INSERT INTO processed_events (consumer_name, event_id)
            VALUES ('risk-engine', $1)
            ON CONFLICT (consumer_name, event_id) DO NOTHING
        )SQL",
        pqxx::params{sourceEventId}
    );
    if(processed.affected_rows() == 0)
    {
        transaction.commit();
        return false;
    }

    for(const auto& candidate : candidates)
    {
        transaction.exec(
            R"SQL(
                INSERT INTO outbox_events
                (
                    event_id,
                    event_type,
                    aggregate_id,
                    chain_id,
                    block_number,
                    block_hash,
                    canonical_version,
                    payload
                )
                VALUES ($1, $2, $3, $4, $5, $6, $7, $8::jsonb)
                ON CONFLICT (event_id) DO NOTHING
            )SQL",
            pqxx::params{
                RiskEventId(candidate),
                messaging::RISK_LIQUIDATION_DETECTED,
                "position:" + candidate.borrower.ToHex(false),
                candidate.chainId.ToDecimal(),
                candidate.blockNumber,
                ethereum::Hex::Encode(candidate.blockHash),
                candidate.canonicalVersion,
                CandidatePayload(candidate).dump()
            }
        );
    }
    transaction.commit();
    return true;
}

RiskWorker::RiskWorker(RiskEngine& engine, RiskEventStore& store, std::size_t scanLimit)
    : engine_(engine), store_(store), scanLimit_(scanLimit)
{
}

bool RiskWorker::Process(std::string_view sourceEventId, std::uint64_t evaluatedAt)
{
    return store_.CommitScan(sourceEventId, engine_.Scan(scanLimit_, evaluatedAt));
}

bool RiskWorker::Rescan(std::uint64_t evaluatedAt)
{
    const auto market = engine_.LoadMarket();
    const auto sourceEventId = "risk.rescan:" + market.chainId.ToDecimal() + ":"
        + std::to_string(market.canonicalVersion) + ":" + std::to_string(evaluatedAt);
    return Process(sourceEventId, evaluatedAt);
}

}
