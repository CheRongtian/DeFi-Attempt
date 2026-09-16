#ifndef DLP_MESSAGING_EVENT_HPP
#define DLP_MESSAGING_EVENT_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "dlp/ethereum/Keccak.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::messaging
{

inline constexpr std::string_view CHAIN_BLOCK_PROCESSED = "chain.block.processed";
inline constexpr std::string_view CHAIN_REORG = "chain.reorg";
inline constexpr std::string_view RISK_LIQUIDATION_DETECTED = "risk.liquidation.detected";
inline constexpr std::string_view EVENT_STREAM = "DLP_EVENTS";

struct EventEnvelope
{
    std::string eventId;
    std::string eventType;
    std::string aggregateId;
    ethereum::Uint256 chainId;
    std::uint64_t blockNumber{0};
    ethereum::Hash256 blockHash{};
    std::optional<ethereum::Hash256> transactionHash;
    std::optional<std::uint64_t> logIndex;
    std::uint64_t canonicalVersion{0};
    std::uint64_t createdAt{0};
    nlohmann::json payload;
};

[[nodiscard]] std::string Serialize(const EventEnvelope& event);
[[nodiscard]] EventEnvelope Deserialize(std::string_view value);

}

#endif
