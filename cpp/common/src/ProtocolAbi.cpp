#include "dlp/ethereum/ProtocolAbi.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dlp::ethereum
{

namespace
{

[[nodiscard]] EventDefinition DefineEvent(
    ProtocolEventKind kind,
    std::string_view signature,
    std::vector<AbiParameter> parameters
)
{
    return EventDefinition{kind, signature, std::move(parameters), Abi::GetEventTopic(signature)};
}

}

const std::vector<EventDefinition>& ProtocolAbi::GetEvents()
{
    static const std::vector<EventDefinition> events{
        DefineEvent(
            ProtocolEventKind::Supplied,
            "Supplied(address,address,uint256)",
            {{AbiType::Address, true}, {AbiType::Address, true}, {AbiType::Uint256, false}}
        ),
        DefineEvent(
            ProtocolEventKind::Withdrawn,
            "Withdrawn(address,address,uint256)",
            {{AbiType::Address, true}, {AbiType::Address, true}, {AbiType::Uint256, false}}
        ),
        DefineEvent(
            ProtocolEventKind::Borrowed,
            "Borrowed(address,address,uint256)",
            {{AbiType::Address, true}, {AbiType::Address, true}, {AbiType::Uint256, false}}
        ),
        DefineEvent(
            ProtocolEventKind::Repaid,
            "Repaid(address,address,uint256,uint256)",
            {
                {AbiType::Address, true},
                {AbiType::Address, true},
                {AbiType::Uint256, false},
                {AbiType::Uint256, false}
            }
        ),
        DefineEvent(
            ProtocolEventKind::InterestAccrued,
            "InterestAccrued(uint256,uint256,uint256,uint256)",
            {
                {AbiType::Uint256, true},
                {AbiType::Uint256, false},
                {AbiType::Uint256, false},
                {AbiType::Uint256, false}
            }
        ),
        DefineEvent(
            ProtocolEventKind::Liquidated,
            "Liquidated(address,address,address,address,uint256,uint256)",
            {
                {AbiType::Address, true},
                {AbiType::Address, true},
                {AbiType::Address, true},
                {AbiType::Address, false},
                {AbiType::Uint256, false},
                {AbiType::Uint256, false}
            }
        ),
        DefineEvent(
            ProtocolEventKind::BadDebtRecognized,
            "BadDebtRecognized(address,uint256)",
            {{AbiType::Address, true}, {AbiType::Uint256, false}}
        ),
        DefineEvent(
            ProtocolEventKind::AssetRegistered,
            "AssetRegistered(address,uint256)",
            {{AbiType::Address, true}, {AbiType::Uint256, false}}
        ),
        DefineEvent(
            ProtocolEventKind::PriceUpdated,
            "PriceUpdated(address,uint256,uint256)",
            {{AbiType::Address, true}, {AbiType::Uint256, false}, {AbiType::Uint256, false}}
        )
    };

    return events;
}

const EventDefinition& ProtocolAbi::GetEvent(ProtocolEventKind kind)
{
    const auto& events = GetEvents();
    const auto iterator = std::find_if(
        events.begin(),
        events.end(),
        [kind](const EventDefinition& event) { return event.kind == kind; }
    );

    if(iterator == events.end())
    {
        throw std::invalid_argument("unknown protocol event kind");
    }
    return *iterator;
}

std::optional<ProtocolEventKind> ProtocolAbi::MatchEvent(const Hash256& topic)
{
    const auto& events = GetEvents();
    const auto iterator = std::find_if(
        events.begin(),
        events.end(),
        [&topic](const EventDefinition& event) { return event.topic == topic; }
    );

    if(iterator == events.end())
    {
        return std::nullopt;
    }
    return iterator->kind;
}

}
