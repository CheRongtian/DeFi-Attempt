#include "dlp/messaging/Outbox.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <utility>

#include <pqxx/pqxx>

#include "dlp/ethereum/Hex.hpp"

namespace dlp::messaging
{

namespace
{

[[nodiscard]] ethereum::Hash256 ParseHash(std::string_view value)
{
    const auto bytes = ethereum::Hex::Decode(value);
    if(bytes.size() != ethereum::Hash256{}.size())
    {
        throw std::invalid_argument("outbox hash must contain 32 bytes");
    }

    ethereum::Hash256 hash{};
    std::copy(bytes.begin(), bytes.end(), hash.begin());
    return hash;
}

[[nodiscard]] EventEnvelope ParseEvent(const pqxx::row_ref& row)
{
    EventEnvelope event{
        row["event_id"].as<std::string>(),
        row["event_type"].as<std::string>(),
        row["aggregate_id"].as<std::string>(),
        ethereum::Uint256::FromDecimal(row["chain_id"].as<std::string>()),
        row["block_number"].as<std::uint64_t>(),
        ParseHash(row["block_hash"].as<std::string>()),
        std::nullopt,
        std::nullopt,
        row["canonical_version"].as<std::uint64_t>(),
        row["created_at"].as<std::uint64_t>(),
        nlohmann::json::parse(row["payload"].as<std::string>())
    };
    if(!row["transaction_hash"].is_null())
    {
        event.transactionHash = ParseHash(row["transaction_hash"].as<std::string>());
    }
    if(!row["log_index"].is_null())
    {
        event.logIndex = row["log_index"].as<std::uint64_t>();
    }
    return event;
}

}

class PostgresOutboxStore::Impl final
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

PostgresOutboxStore::PostgresOutboxStore(std::string connectionString)
    : implementation_(std::make_unique<Impl>(std::move(connectionString)))
{
}

PostgresOutboxStore::~PostgresOutboxStore() = default;
PostgresOutboxStore::PostgresOutboxStore(PostgresOutboxStore&& other) noexcept = default;
PostgresOutboxStore& PostgresOutboxStore::operator=(PostgresOutboxStore&& other) noexcept = default;

std::vector<EventEnvelope> PostgresOutboxStore::LoadUnpublished(std::size_t limit) const
{
    auto connection = implementation_->Connect();
    pqxx::read_transaction transaction{connection};
    const auto rows = transaction.exec(
        R"SQL(
            SELECT
                event_id,
                event_type,
                aggregate_id,
                chain_id,
                block_number,
                block_hash,
                transaction_hash,
                log_index,
                canonical_version,
                payload,
                EXTRACT(EPOCH FROM created_at)::BIGINT AS created_at
            FROM outbox_events
            WHERE published_at IS NULL
            ORDER BY created_at, event_id
            LIMIT $1
        )SQL",
        pqxx::params{limit}
    );

    std::vector<EventEnvelope> events;
    events.reserve(static_cast<std::size_t>(rows.size()));
    for(const auto& row : rows)
    {
        events.push_back(ParseEvent(row));
    }
    return events;
}

void PostgresOutboxStore::MarkPublished(std::string_view eventId)
{
    auto connection = implementation_->Connect();
    pqxx::work transaction{connection};
    transaction.exec(
        "UPDATE outbox_events SET published_at = NOW() WHERE event_id = $1",
        pqxx::params{eventId}
    );
    transaction.commit();
}

OutboxPublisher::OutboxPublisher(OutboxStore& store, EventPublisher& publisher)
    : store_(store), publisher_(publisher)
{
}

std::size_t OutboxPublisher::RunOnce(std::size_t limit)
{
    const auto events = store_.LoadUnpublished(limit);
    for(const auto& event : events)
    {
        publisher_.Publish(event);
        store_.MarkPublished(event.eventId);
    }
    return events.size();
}

}
