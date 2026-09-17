#include "dlp/ethereum/Rlp.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace dlp::ethereum
{

namespace
{

[[nodiscard]] Bytes LengthBytes(std::size_t length)
{
    Bytes result;
    while(length != 0U)
    {
        result.push_back(static_cast<std::uint8_t>(length & 0xffU));
        length >>= 8U;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

[[nodiscard]] Bytes EncodePayload(const Bytes& payload, std::uint8_t shortOffset, std::uint8_t longOffset)
{
    if(payload.size() <= 55U)
    {
        Bytes result{static_cast<std::uint8_t>(shortOffset + payload.size())};
        result.insert(result.end(), payload.begin(), payload.end());
        return result;
    }

    const auto length = LengthBytes(payload.size());
    Bytes result{static_cast<std::uint8_t>(longOffset + length.size())};
    result.insert(result.end(), length.begin(), length.end());
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

}

Bytes Rlp::EncodeBytes(const Bytes& value)
{
    if(value.size() == 1U && value.front() < 0x80U)
    {
        return Bytes{value.front()};
    }
    return EncodePayload(value, 0x80U, 0xb7U);
}

Bytes Rlp::EncodeUint256(const Uint256& value)
{
    if(value.IsZero())
    {
        return EncodeBytes({});
    }

    const auto bytes = value.ToBytes();
    const auto first = std::find_if(bytes.begin(), bytes.end(), [](std::uint8_t byte) { return byte != 0U; });
    return EncodeBytes(Bytes{first, bytes.end()});
}

Bytes Rlp::EncodeList(const std::vector<Bytes>& encodedItems)
{
    Bytes payload;
    for(const auto& item : encodedItems)
    {
        payload.insert(payload.end(), item.begin(), item.end());
    }
    return EncodePayload(payload, 0xc0U, 0xf7U);
}

}
