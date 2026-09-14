#include "dlp/ethereum/Uint256Math.hpp"

#include <stdexcept>

#include <boost/multiprecision/cpp_int.hpp>

namespace dlp::ethereum
{

namespace
{

using WideInteger = boost::multiprecision::uint512_t;

[[nodiscard]] WideInteger ToWide(const Uint256& value)
{
    WideInteger result{0};
    for(const auto byte : value.ToBytes())
    {
        result <<= 8;
        result += byte;
    }
    return result;
}

[[nodiscard]] Uint256 FromWide(const WideInteger& value)
{
    const WideInteger maximum = (WideInteger{1} << 256U) - 1U;
    if(value > maximum)
    {
        throw std::overflow_error("mulDiv result exceeds uint256");
    }

    Uint256::Bytes32 bytes{};
    auto remaining = value;
    for(auto iterator = bytes.rbegin(); iterator != bytes.rend(); ++iterator)
    {
        *iterator = static_cast<std::uint8_t>((remaining & 0xffU).convert_to<unsigned>());
        remaining >>= 8;
    }
    return Uint256::FromBytes(bytes);
}

}

Uint256 Uint256Math::MulDivDown(
    const Uint256& left,
    const Uint256& right,
    const Uint256& denominator
)
{
    if(denominator.IsZero())
    {
        throw std::domain_error("mulDiv denominator is zero");
    }

    return FromWide(ToWide(left) * ToWide(right) / ToWide(denominator));
}

Uint256 Uint256Math::MulDivUp(
    const Uint256& left,
    const Uint256& right,
    const Uint256& denominator
)
{
    if(denominator.IsZero())
    {
        throw std::domain_error("mulDiv denominator is zero");
    }

    const auto product = ToWide(left) * ToWide(right);
    const auto divisor = ToWide(denominator);
    const auto quotient = product / divisor;
    return FromWide(quotient + (product % divisor == 0 ? 0U : 1U));
}

}
