#include <stdexcept>

#include <gtest/gtest.h>

#include "dlp/ethereum/Address.hpp"

namespace dlp::ethereum
{

TEST(AddressTests, ParsesAndFormatsTwentyByteAddress)
{
    const auto address = Address::FromHex("0x00112233445566778899aabbccddeeff00112233");
    EXPECT_EQ(address.ToHex(), "0x00112233445566778899aabbccddeeff00112233");
    EXPECT_EQ(address.ToHex(false), "00112233445566778899aabbccddeeff00112233");
    EXPECT_FALSE(address.IsZero());
}

TEST(AddressTests, DefaultAddressIsZero)
{
    EXPECT_TRUE(Address{}.IsZero());
}

TEST(AddressTests, SupportsValueComparison)
{
    const auto low = Address::FromHex("0x0000000000000000000000000000000000000001");
    const auto high = Address::FromHex("0x0000000000000000000000000000000000000002");

    EXPECT_EQ(low, low);
    EXPECT_NE(low, high);
    EXPECT_LT(low, high);
}

TEST(AddressTests, RejectsIncorrectLengthAndInvalidHex)
{
    EXPECT_THROW([] { return Address::FromHex("0x1234"); }(), std::invalid_argument);
    EXPECT_THROW(
        [] { return Address::FromHex("0x00112233445566778899aabbccddeeff001122zz"); }(),
        std::invalid_argument
    );
}

}
