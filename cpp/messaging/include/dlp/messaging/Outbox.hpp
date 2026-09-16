#ifndef DLP_MESSAGING_OUTBOX_HPP
#define DLP_MESSAGING_OUTBOX_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "dlp/messaging/Event.hpp"

namespace dlp::messaging
{

class EventPublisher
{
public:
    virtual ~EventPublisher() = default;
    virtual void Publish(const EventEnvelope& event) = 0;
};

class OutboxStore
{
public:
    virtual ~OutboxStore() = default;
    [[nodiscard]] virtual std::vector<EventEnvelope> LoadUnpublished(std::size_t limit) const = 0;
    virtual void MarkPublished(std::string_view eventId) = 0;
};

class PostgresOutboxStore final : public OutboxStore
{
public:
    explicit PostgresOutboxStore(std::string connectionString);
    ~PostgresOutboxStore() override;

    PostgresOutboxStore(PostgresOutboxStore&& other) noexcept;
    PostgresOutboxStore& operator=(PostgresOutboxStore&& other) noexcept;

    PostgresOutboxStore(const PostgresOutboxStore&) = delete;
    PostgresOutboxStore& operator=(const PostgresOutboxStore&) = delete;

    [[nodiscard]] std::vector<EventEnvelope> LoadUnpublished(std::size_t limit) const override;
    void MarkPublished(std::string_view eventId) override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

class OutboxPublisher final
{
public:
    OutboxPublisher(OutboxStore& store, EventPublisher& publisher);
    std::size_t RunOnce(std::size_t limit);

private:
    OutboxStore& store_;
    EventPublisher& publisher_;
};

}

#endif
