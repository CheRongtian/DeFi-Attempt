#include "dlp/ethereum/Uint256Math.hpp"

#include <stdexcept>

#include <gtest/gtest.h>

namespace dlp::ethereum
{

TEST(Uint256MathTests, MultipliesBeforeDividingWithoutIntermediateOverflow)
{
    const auto value = Uint256::FromHex("0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

    EXPECT_EQ(Uint256Math::MulDivDown(value, value, value), value);
}

TEST(Uint256MathTests, AppliesFloorAndCeilingRounding)
{
    const Uint256 left{10};
    const Uint256 right{10};
    const Uint256 denominator{6};

    EXPECT_EQ(Uint256Math::MulDivDown(left, right, denominator), Uint256{16});
    EXPECT_EQ(Uint256Math::MulDivUp(left, right, denominator), Uint256{17});
}

TEST(Uint256MathTests, RejectsZeroDenominator)
{
    EXPECT_THROW(
        [] { return Uint256Math::MulDivDown(Uint256{1}, Uint256{1}, Uint256{}); }(),
        std::domain_error
    );
    EXPECT_THROW(
        [] { return Uint256Math::MulDivUp(Uint256{1}, Uint256{1}, Uint256{}); }(),
        std::domain_error
    );
}

}
