#include <stdexcept>

#include <gtest/gtest.h>

#include "dlp/ethereum/Uint256.hpp"

namespace dlp::ethereum
{

namespace
{

constexpr std::string_view MAX_UINT256_DECIMAL =
    "115792089237316195423570985008687907853269984665640564039457584007913129639935";

}

TEST(Uint256Tests, ParsesHexAndFormatsCanonicalQuantity)
{
    EXPECT_EQ(Uint256::FromHex("0x000abc").ToQuantity(), "0xabc");
    EXPECT_EQ(Uint256::FromHex("ABC").ToQuantity(), "0xabc");
    EXPECT_EQ(Uint256{}.ToQuantity(), "0x0");
}

TEST(Uint256Tests, RoundTripsMaximumValueThroughBytes)
{
    const auto maximum = Uint256::FromDecimal(MAX_UINT256_DECIMAL);
    EXPECT_EQ(maximum.ToDecimal(), MAX_UINT256_DECIMAL);
    EXPECT_EQ(Uint256::FromBytes(maximum.ToBytes()), maximum);
}

TEST(Uint256Tests, PerformsCheckedArithmetic)
{
    const Uint256 left{21};
    const Uint256 right{2};

    EXPECT_EQ((left + right).ToDecimal(), "23");
    EXPECT_EQ((left - right).ToDecimal(), "19");
    EXPECT_EQ((left * right).ToDecimal(), "42");
    EXPECT_EQ((left / right).ToDecimal(), "10");
}

TEST(Uint256Tests, RejectsOverflowUnderflowAndDivisionByZero)
{
    const auto maximum = Uint256::FromDecimal(MAX_UINT256_DECIMAL);
    EXPECT_THROW(maximum + Uint256{1}, std::overflow_error);
    EXPECT_THROW(Uint256{} - Uint256{1}, std::overflow_error);
    EXPECT_THROW(Uint256{1} / Uint256{}, std::domain_error);
    EXPECT_THROW(
        [] {
            return Uint256::FromDecimal(
                "115792089237316195423570985008687907853269984665640564039457584007913129639936"
            );
        }(),
        std::overflow_error
    );
}

TEST(Uint256Tests, RejectsMalformedInput)
{
    EXPECT_THROW([] { return Uint256::FromHex("0x"); }(), std::invalid_argument);
    EXPECT_THROW([] { return Uint256::FromHex("0xzz"); }(), std::invalid_argument);
    EXPECT_THROW([] { return Uint256::FromDecimal(""); }(), std::invalid_argument);
    EXPECT_THROW([] { return Uint256::FromDecimal("12a"); }(), std::invalid_argument);
}

}
