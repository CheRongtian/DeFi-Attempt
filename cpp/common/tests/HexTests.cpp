#include <array>
#include <cstdint>
#include <stdexcept>

#include <gtest/gtest.h>

#include "dlp/ethereum/Hex.hpp"

namespace dlp::ethereum
{

TEST(HexTests, DecodesPrefixedAndUnprefixedBytes)
{
    const Bytes expected{0x00, 0xab, 0xcd, 0xef};
    EXPECT_EQ(Hex::Decode("0x00abcdef"), expected);
    EXPECT_EQ(Hex::Decode("00ABCDEF"), expected);
}

TEST(HexTests, EncodesLowercaseBytes)
{
    const Bytes value{0x00, 0xab, 0xcd, 0xef};
    EXPECT_EQ(Hex::Encode(value), "0x00abcdef");
    EXPECT_EQ(Hex::Encode(value, false), "00abcdef");
}

TEST(HexTests, SupportsEmptyData)
{
    EXPECT_TRUE(Hex::Decode("0x").empty());
    EXPECT_EQ(Hex::Encode(Bytes{}), "0x");
}

TEST(HexTests, RejectsOddLengthAndInvalidCharacters)
{
    EXPECT_THROW([] { return Hex::Decode("0xabc"); }(), std::invalid_argument);
    EXPECT_THROW([] { return Hex::Decode("0xgg"); }(), std::invalid_argument);
}

}
