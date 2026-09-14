#ifndef DLP_INDEXER_EVENT_DECODER_HPP
#define DLP_INDEXER_EVENT_DECODER_HPP

#include <optional>

#include "dlp/indexer/IndexerTypes.hpp"

namespace dlp::indexer
{

class EventDecoder final
{
public:
    explicit EventDecoder(ChainContracts contracts);

    [[nodiscard]] std::optional<DecodedProtocolEvent> Decode(const RawLog& log) const;

private:
    [[nodiscard]] bool IsExpectedEmitter(
        ethereum::ProtocolEventKind kind,
        const ethereum::Address& emitter
    ) const;

    ChainContracts contracts_;
};

}

#endif
