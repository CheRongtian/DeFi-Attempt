#ifndef DLP_ETHEREUM_RLP_HPP
#define DLP_ETHEREUM_RLP_HPP

#include <vector>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::ethereum
{

class Rlp final
{
public:
    [[nodiscard]] static Bytes EncodeBytes(const Bytes& value);
    [[nodiscard]] static Bytes EncodeUint256(const Uint256& value);
    [[nodiscard]] static Bytes EncodeList(const std::vector<Bytes>& encodedItems);
};

}

#endif
