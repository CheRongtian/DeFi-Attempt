#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "dlp/messaging/Outbox.hpp"
#include "dlp/ethereum/Hex.hpp"

namespace dlp::messaging
{

namespace
{

EventEnvelope Event(std::string id)
{
    ethereum::Hash256 blockHash{};
    blockHash.back() = 7;
    return EventEnvelope{
        std::move(id),
        std::string{CHAIN_BLOCK_PROCESSED},
        "chain:31337",
        ethereum::Uint256{31337},
        12,
        blockHash,
        std::nullopt,
        std::nullopt,
        4,
        1'700'000'000,
        {{"parentHash", ethereum::Hex::Encode(blockHash)}}
    };
}

class MemoryOutbox final : public OutboxStore
{
public:
    std::vector<EventEnvelope> pending;
    std::vector<std::string> published;

    [[nodiscard]] std::vector<EventEnvelope> LoadUnpublished(std::size_t limit) const override
    {
        const auto count = std::min(limit, pending.size());
        return {pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(count)};
    }

    void MarkPublished(std::string_view eventId) override
    {
        published.emplace_back(eventId);
        pending.erase(pending.begin());
    }
};

class RecordingPublisher final : public EventPublisher
{
public:
    bool fail{false};
    std::vector<std::string> eventIds;

    void Publish(const EventEnvelope& event) override
    {
        if(fail)
        {
            throw std::runtime_error("publish failed");
        }
        eventIds.push_back(event.eventId);
    }
};

}

TEST(MessagingTests, SerializesAnEventEnvelope)
{
    const auto original = Event("block:12");
    const auto decoded = Deserialize(Serialize(original));

    EXPECT_EQ(decoded.eventId, original.eventId);
    EXPECT_EQ(decoded.chainId, original.chainId);
    EXPECT_EQ(decoded.blockHash, original.blockHash);
    EXPECT_EQ(decoded.canonicalVersion, original.canonicalVersion);
    EXPECT_EQ(decoded.payload, original.payload);
}

TEST(MessagingTests, RetriesAnUnpublishedEvent)
{
    MemoryOutbox store;
    store.pending.push_back(Event("block:12"));
    RecordingPublisher transport;
    OutboxPublisher publisher{store, transport};

    transport.fail = true;
    EXPECT_THROW(publisher.RunOnce(10), std::runtime_error);
    EXPECT_TRUE(store.published.empty());

    transport.fail = false;
    EXPECT_EQ(publisher.RunOnce(10), 1U);
    EXPECT_EQ(store.published, std::vector<std::string>{"block:12"});
    EXPECT_EQ(transport.eventIds, std::vector<std::string>{"block:12"});
}

}
