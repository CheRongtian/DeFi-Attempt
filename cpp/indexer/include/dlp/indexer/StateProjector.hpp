#ifndef DLP_INDEXER_STATE_PROJECTOR_HPP
#define DLP_INDEXER_STATE_PROJECTOR_HPP

#include <vector>

#include "dlp/indexer/EventDecoder.hpp"
#include "dlp/indexer/IndexerTypes.hpp"

namespace dlp::indexer
{

class StateProjector final
{
public:
    explicit StateProjector(ChainContracts contracts);

    [[nodiscard]] DerivedState CreateInitialState() const;
    [[nodiscard]] DerivedState Rebuild(const std::vector<RawLog>& logs) const;
    void Apply(DerivedState& state, const RawLog& log) const;

private:
    void ApplyEvent(DerivedState& state, const DecodedProtocolEvent& event) const;

    ChainContracts contracts_;
    EventDecoder decoder_;
};

}

#endif
