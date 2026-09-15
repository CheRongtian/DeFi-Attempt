#include <gtest/gtest.h>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Rlp.hpp"

namespace dlp::ethereum
{

TEST(RlpTests, EncodesEthereumByteAndListExamples)
{
    EXPECT_EQ(Hex::Encode(Rlp::EncodeBytes({})), "0x80");
    EXPECT_EQ(Hex::Encode(Rlp::EncodeBytes(Bytes{'d', 'o', 'g'})), "0x83646f67");
    EXPECT_EQ(
        Hex::Encode(Rlp::EncodeList({Rlp::EncodeBytes(Bytes{'c', 'a', 't'}), Rlp::EncodeBytes(Bytes{'d', 'o', 'g'})})),
        "0xc88363617483646f67"
    );
}

TEST(RlpTests, EncodesIntegersWithoutLeadingZeroes)
{
    EXPECT_EQ(Hex::Encode(Rlp::EncodeUint256(Uint256{})), "0x80");
    EXPECT_EQ(Hex::Encode(Rlp::EncodeUint256(Uint256{15})), "0x0f");
    EXPECT_EQ(Hex::Encode(Rlp::EncodeUint256(Uint256{1024})), "0x820400");
}

}
