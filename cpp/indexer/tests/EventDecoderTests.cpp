#include <stdexcept>
#include <variant>

#include <gtest/gtest.h>

#include "dlp/indexer/EventDecoder.hpp"
#include "TestData.hpp"

namespace dlp::indexer
{

TEST(EventDecoderTests, DecodesKnownEventFromItsProtocolContract)
{
    const auto contracts = test::Contracts();
    const auto user = test::Address(10);
    const ethereum::Uint256 amount{25};
    const auto log = test::EventLog(
        ethereum::ProtocolEventKind::Supplied,
        contracts.pool,
        {user, contracts.weth, amount}
    );

    const auto event = EventDecoder{contracts}.Decode(log);

    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->kind, ethereum::ProtocolEventKind::Supplied);
    EXPECT_EQ(std::get<ethereum::Address>(event->values.at(0)), user);
    EXPECT_EQ(std::get<ethereum::Address>(event->values.at(1)), contracts.weth);
    EXPECT_EQ(std::get<ethereum::Uint256>(event->values.at(2)), amount);
}

TEST(EventDecoderTests, IgnoresUnknownTopicsAndUnexpectedEmitters)
{
    const auto contracts = test::Contracts();
    auto unknown = test::EventLog(
        ethereum::ProtocolEventKind::Supplied,
        contracts.pool,
        {test::Address(10), contracts.weth, ethereum::Uint256{1}}
    );
    unknown.topics.front() = test::Hash(7, 7);

    auto wrongEmitter = test::EventLog(
        ethereum::ProtocolEventKind::PriceUpdated,
        contracts.pool,
        {contracts.weth, ethereum::Uint256{3000}, ethereum::Uint256{10}}
    );

    const EventDecoder decoder{contracts};
    EXPECT_FALSE(decoder.Decode(unknown).has_value());
    EXPECT_FALSE(decoder.Decode(wrongEmitter).has_value());
}

TEST(EventDecoderTests, RejectsMalformedKnownEvent)
{
    const auto contracts = test::Contracts();
    auto log = test::EventLog(
        ethereum::ProtocolEventKind::Borrowed,
        contracts.pool,
        {test::Address(10), contracts.usdc, ethereum::Uint256{1}}
    );
    log.data.clear();

    EXPECT_THROW(
        [&] { return EventDecoder{contracts}.Decode(log); }(),
        std::invalid_argument
    );
}

}
