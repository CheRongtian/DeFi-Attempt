#include <gtest/gtest.h>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Keccak.hpp"

namespace dlp::ethereum
{

TEST(KeccakTests, MatchesEthereumEmptyInputVector)
{
    const auto hash = Keccak::Hash(std::string_view{});
    EXPECT_EQ(
        Hex::Encode(hash),
        "0xc5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
    );
}

TEST(KeccakTests, MatchesEthereumHelloVector)
{
    const auto hash = Keccak::Hash("hello");
    EXPECT_EQ(
        Hex::Encode(hash),
        "0x1c8aff950685c2ed4bc3174f3472287b56d9517b9c948127319a09a7a36deac8"
    );
}

}
