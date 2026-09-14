#ifndef DLP_ETHEREUM_UINT256_MATH_HPP
#define DLP_ETHEREUM_UINT256_MATH_HPP

#include "dlp/ethereum/Uint256.hpp"

namespace dlp::ethereum
{

class Uint256Math final
{
public:
    [[nodiscard]] static Uint256 MulDivDown(
        const Uint256& left,
        const Uint256& right,
        const Uint256& denominator
    );

    [[nodiscard]] static Uint256 MulDivUp(
        const Uint256& left,
        const Uint256& right,
        const Uint256& denominator
    );
};

}

#endif
