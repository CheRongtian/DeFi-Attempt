#include "dlp/messaging/Event.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

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
        throw std::invalid_argument("event hash must contain 32 bytes");
    }

    ethereum::Hash256 hash{};
    std::copy(bytes.begin(), bytes.end(), hash.begin());
    return hash;
}

}

std::string Serialize(const EventEnvelope& event)
{
    nlohmann::json value{
        {"eventId", event.eventId},
        {"eventType", event.eventType},
        {"aggregateId", event.aggregateId},
        {"chainId", event.chainId.ToDecimal()},
        {"blockNumber", event.blockNumber},
        {"blockHash", ethereum::Hex::Encode(event.blockHash)},
        {"canonicalVersion", event.canonicalVersion},
        {"createdAt", event.createdAt},
        {"payload", event.payload}
    };
    if(event.transactionHash.has_value())
    {
        value["transactionHash"] = ethereum::Hex::Encode(*event.transactionHash);
    }
    if(event.logIndex.has_value())
    {
        value["logIndex"] = *event.logIndex;
    }
    return value.dump();
}

EventEnvelope Deserialize(std::string_view value)
{
    const auto json = nlohmann::json::parse(value);
    EventEnvelope event{
        json.at("eventId").get<std::string>(),
        json.at("eventType").get<std::string>(),
        json.at("aggregateId").get<std::string>(),
        ethereum::Uint256::FromDecimal(json.at("chainId").get<std::string>()),
        json.at("blockNumber").get<std::uint64_t>(),
        ParseHash(json.at("blockHash").get<std::string>()),
        std::nullopt,
        std::nullopt,
        json.at("canonicalVersion").get<std::uint64_t>(),
        json.at("createdAt").get<std::uint64_t>(),
        json.at("payload")
    };
    if(json.contains("transactionHash"))
    {
        event.transactionHash = ParseHash(json.at("transactionHash").get<std::string>());
    }
    if(json.contains("logIndex"))
    {
        event.logIndex = json.at("logIndex").get<std::uint64_t>();
    }
    return event;
}

}
