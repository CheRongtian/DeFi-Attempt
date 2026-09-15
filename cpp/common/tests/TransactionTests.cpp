#include <gtest/gtest.h>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Transaction.hpp"

namespace dlp::ethereum
{

TEST(TransactionTests, SignsAnEip1559TransactionWithTheExpectedSender)
{
    const Secp256k1Signer signer{
        "0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80"
    };
    EXPECT_EQ(signer.GetAddress().ToHex(), "0xf39fd6e51aad88f6f4ce6ab8827279cfffb92266");

    const Eip1559Transaction transaction{
        Uint256{31337},
        Uint256{},
        Uint256{1'000'000'000},
        Uint256{2'000'000'000},
        Uint256{21'000},
        Address::FromHex("0x1111111111111111111111111111111111111111"),
        Uint256{1},
        {}
    };
    const auto raw = signer.SignTransaction(transaction);
    ASSERT_FALSE(raw.empty());
    EXPECT_EQ(raw.front(), 0x02U);
    EXPECT_EQ(Eip1559Encoder::TransactionHash(raw), Keccak::Hash(raw));
}

}
