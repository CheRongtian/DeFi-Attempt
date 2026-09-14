#include "dlp/ethereum/Address.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "dlp/ethereum/Hex.hpp"

namespace dlp::ethereum
{

Address::Address(Storage bytes) noexcept
    : bytes_(std::move(bytes))
{
}

Address Address::FromHex(std::string_view value)
{
    const auto decoded = Hex::Decode(value);
    if(decoded.size() != SIZE)
    {
        throw std::invalid_argument("Ethereum addresses must contain exactly 20 bytes");
    }

    Storage bytes{};
    std::copy(decoded.begin(), decoded.end(), bytes.begin());
    return Address{bytes};
}

const Address::Storage& Address::GetBytes() const noexcept
{
    return bytes_;
}

std::string Address::ToHex(bool includePrefix) const
{
    return Hex::Encode(bytes_, includePrefix);
}

bool Address::IsZero() const noexcept
{
    return std::all_of(bytes_.begin(), bytes_.end(), [](std::uint8_t value) { return value == 0; });
}

bool operator==(const Address& left, const Address& right) noexcept
{
    return left.bytes_ == right.bytes_;
}

bool operator!=(const Address& left, const Address& right) noexcept
{
    return !(left == right);
}

bool operator<(const Address& left, const Address& right) noexcept
{
    return left.bytes_ < right.bytes_;
}

}
