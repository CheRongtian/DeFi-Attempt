#include "dlp/indexer/EventDecoder.hpp"

#include <utility>

#include "dlp/ethereum/Abi.hpp"
#include "dlp/ethereum/ProtocolAbi.hpp"

namespace dlp::indexer
{

EventDecoder::EventDecoder(ChainContracts contracts)
    : contracts_(std::move(contracts))
{
}

std::optional<DecodedProtocolEvent> EventDecoder::Decode(const RawLog& log) const
{
    if(log.topics.empty())
    {
        return std::nullopt;
    }

    const auto kind = ethereum::ProtocolAbi::MatchEvent(log.topics.front());
    if(!kind.has_value() || !IsExpectedEmitter(*kind, log.contractAddress))
    {
        return std::nullopt;
    }

    const auto& definition = ethereum::ProtocolAbi::GetEvent(*kind);
    return DecodedProtocolEvent{
        *kind,
        ethereum::Abi::DecodeEvent(
            definition.signature,
            definition.parameters,
            log.topics,
            log.data
        ),
        log
    };
}

bool EventDecoder::IsExpectedEmitter(
    ethereum::ProtocolEventKind kind,
    const ethereum::Address& emitter
) const
{
    switch(kind)
    {
        case ethereum::ProtocolEventKind::AssetRegistered:
        case ethereum::ProtocolEventKind::PriceUpdated:
            return emitter == contracts_.oracle;
        case ethereum::ProtocolEventKind::Supplied:
        case ethereum::ProtocolEventKind::Withdrawn:
        case ethereum::ProtocolEventKind::Borrowed:
        case ethereum::ProtocolEventKind::Repaid:
        case ethereum::ProtocolEventKind::InterestAccrued:
        case ethereum::ProtocolEventKind::Liquidated:
        case ethereum::ProtocolEventKind::BadDebtRecognized:
            return emitter == contracts_.pool;
    }
    return false;
}

}
