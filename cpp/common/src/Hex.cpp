#include "dlp/ethereum/Hex.hpp"

#include <stdexcept>

namespace dlp::ethereum
{

Bytes Hex::Decode(std::string_view value)
{
    if(value.size() >= 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
    {
        value.remove_prefix(2);
    }

    if(value.size() % 2 != 0)
    {
        throw std::invalid_argument("hex byte strings must contain an even number of digits");
    }

    Bytes result;
    result.reserve(value.size() / 2);

    for(std::size_t i = 0; i < value.size(); i += 2)
    {
        const auto high = DecodeNibble(value[i]);
        const auto low = DecodeNibble(value[i + 1]);
        result.push_back(static_cast<std::uint8_t>((high << 4U) | low));
    }

    return result;
}

std::string Hex::EncodeBuffer(const std::uint8_t* data, std::size_t size, bool includePrefix)
{
    static constexpr std::string_view DIGITS = "0123456789abcdef";

    std::string result;
    result.reserve(size * 2 + (includePrefix ? 2 : 0));
    if(includePrefix)
    {
        result.append("0x");
    }

    for(std::size_t i = 0; i < size; ++i)
    {
        result.push_back(DIGITS[data[i] >> 4U]);
        result.push_back(DIGITS[data[i] & 0x0fU]);
    }

    return result;
}

std::string Hex::Encode(const Bytes& data, bool includePrefix)
{
    return EncodeBuffer(data.data(), data.size(), includePrefix);
}

std::uint8_t Hex::DecodeNibble(char value)
{
    if(value >= '0' && value <= '9')
    {
        return static_cast<std::uint8_t>(value - '0');
    }
    if(value >= 'a' && value <= 'f')
    {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    if(value >= 'A' && value <= 'F')
    {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }

    throw std::invalid_argument("hex string contains an invalid character");
}

}
