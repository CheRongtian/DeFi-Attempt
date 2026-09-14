#include <algorithm>
#include <stdexcept>
#include <variant>

#include <gtest/gtest.h>

#include "dlp/ethereum/Abi.hpp"

namespace dlp::ethereum
{

TEST(AbiTests, MatchesKnownFunctionSelector)
{
    const auto selector = Abi::GetFunctionSelector("transfer(address,uint256)");
    EXPECT_EQ(Hex::Encode(selector), "0xa9059cbb");
}

TEST(AbiTests, MatchesKnownEventTopic)
{
    const auto topic = Abi::GetEventTopic("Transfer(address,address,uint256)");
    EXPECT_EQ(
        Hex::Encode(topic),
        "0xddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3ef"
    );
}

TEST(AbiTests, EncodesStaticFunctionArguments)
{
    const auto recipient = Address::FromHex("0x1111111111111111111111111111111111111111");
    const auto encoded = Abi::EncodeFunction(
        "transfer(address,uint256)",
        {AbiValue{recipient}, AbiValue{Uint256{42}}}
    );

    EXPECT_EQ(encoded.size(), 68U);
    const Bytes selectorBytes{encoded.begin(), encoded.begin() + 4};
    EXPECT_EQ(Hex::Encode(selectorBytes), "0xa9059cbb");
    const Bytes addressWord{encoded.begin() + 4, encoded.begin() + 36};
    EXPECT_EQ(
        Hex::Encode(addressWord),
        "0x0000000000000000000000001111111111111111111111111111111111111111"
    );
    Uint256::Bytes32 amountBytes{};
    std::copy_n(encoded.data() + 36, amountBytes.size(), amountBytes.begin());
    EXPECT_EQ(Uint256::FromBytes(amountBytes), Uint256{42});
}

TEST(AbiTests, DecodesIndexedAndDataEventParameters)
{
    const auto user = Address::FromHex("0x1111111111111111111111111111111111111111");
    const auto asset = Address::FromHex("0x2222222222222222222222222222222222222222");
    const std::string_view signature = "Supplied(address,address,uint256)";
    const std::vector<AbiParameter> parameters{
        {AbiType::Address, true},
        {AbiType::Address, true},
        {AbiType::Uint256, false}
    };
    const std::vector<Hash256> topics{
        Abi::GetEventTopic(signature),
        Abi::EncodeWord(user),
        Abi::EncodeWord(asset)
    };
    const auto amountWord = Abi::EncodeWord(Uint256{123});
    const Bytes data{amountWord.begin(), amountWord.end()};

    const auto decoded = Abi::DecodeEvent(signature, parameters, topics, data);
    ASSERT_EQ(decoded.size(), 3U);
    EXPECT_EQ(std::get<Address>(decoded[0]), user);
    EXPECT_EQ(std::get<Address>(decoded[1]), asset);
    EXPECT_EQ(std::get<Uint256>(decoded[2]), Uint256{123});
}

TEST(AbiTests, RejectsMismatchedTopicAndAddressPadding)
{
    const auto signature = std::string_view{"BadDebtRecognized(address,uint256)"};
    const std::vector<AbiParameter> parameters{{AbiType::Address, true}, {AbiType::Uint256, false}};
    std::vector<Hash256> topics{Abi::GetEventTopic(signature), Hash256{}};
    topics.front()[0] ^= 0xffU;
    const auto amountWord = Abi::EncodeWord(Uint256{1});
    const Bytes data{amountWord.begin(), amountWord.end()};
    EXPECT_THROW(
        [&] { return Abi::DecodeEvent(signature, parameters, topics, data); }(),
        std::invalid_argument
    );

    Hash256 invalidAddressWord{};
    invalidAddressWord[0] = 1;
    EXPECT_THROW(
        [&] { return Abi::DecodeWord(AbiType::Address, invalidAddressWord); }(),
        std::invalid_argument
    );
}

}
