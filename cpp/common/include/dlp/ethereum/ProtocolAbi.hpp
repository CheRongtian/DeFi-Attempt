#ifndef DLP_ETHEREUM_PROTOCOL_ABI_HPP
#define DLP_ETHEREUM_PROTOCOL_ABI_HPP

#include <optional>
#include <string_view>
#include <vector>

#include "dlp/ethereum/Abi.hpp"

namespace dlp::ethereum
{

enum class ProtocolEventKind
{
    Supplied,
    Withdrawn,
    Borrowed,
    Repaid,
    InterestAccrued,
    Liquidated,
    BadDebtRecognized,
    AssetRegistered,
    PriceUpdated
};

struct EventDefinition
{
    ProtocolEventKind kind;
    std::string_view signature;
    std::vector<AbiParameter> parameters;
    Hash256 topic;
};

class ProtocolAbi final
{
public:
    [[nodiscard]] static const std::vector<EventDefinition>& GetEvents();
    [[nodiscard]] static const EventDefinition& GetEvent(ProtocolEventKind kind);
    [[nodiscard]] static std::optional<ProtocolEventKind> MatchEvent(const Hash256& topic);
};

}

#endif
