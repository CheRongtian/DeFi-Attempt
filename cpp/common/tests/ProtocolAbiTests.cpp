#include <set>

#include <gtest/gtest.h>

#include "dlp/ethereum/ProtocolAbi.hpp"

namespace dlp::ethereum
{

TEST(ProtocolAbiTests, RegistersCurrentProtocolEvents)
{
    const auto& events = ProtocolAbi::GetEvents();
    EXPECT_EQ(events.size(), 9U);

    std::set<Hash256> topics;
    for(const auto& event : events)
    {
        EXPECT_EQ(event.topic, Abi::GetEventTopic(event.signature));
        EXPECT_TRUE(topics.insert(event.topic).second);
        ASSERT_TRUE(ProtocolAbi::MatchEvent(event.topic).has_value());
        EXPECT_EQ(*ProtocolAbi::MatchEvent(event.topic), event.kind);
    }
}

TEST(ProtocolAbiTests, IncludesInterestAccrualAndPriceUpdates)
{
    EXPECT_EQ(
        ProtocolAbi::GetEvent(ProtocolEventKind::InterestAccrued).signature,
        "InterestAccrued(uint256,uint256,uint256,uint256)"
    );
    EXPECT_EQ(
        ProtocolAbi::GetEvent(ProtocolEventKind::PriceUpdated).signature,
        "PriceUpdated(address,uint256,uint256)"
    );
}

TEST(ProtocolAbiTests, ReturnsNoMatchForUnknownTopic)
{
    Hash256 unknown{};
    unknown.front() = 1;
    EXPECT_FALSE(ProtocolAbi::MatchEvent(unknown).has_value());
}

}
