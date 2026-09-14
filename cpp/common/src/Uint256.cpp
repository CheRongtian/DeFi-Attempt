#include "dlp/ethereum/Uint256.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dlp::ethereum
{

namespace
{

[[nodiscard]] std::uint8_t DecodeNibble(char value)
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

    throw std::invalid_argument("uint256 hex string contains an invalid character");
}

}

Uint256::Uint256(std::uint64_t value) noexcept
    : value_(value)
{
}

Uint256::Uint256(Storage value) noexcept
    : value_(std::move(value))
{
}

Uint256 Uint256::FromHex(std::string_view value)
{
    if(value.size() >= 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
    {
        value.remove_prefix(2);
    }
    if(value.empty())
    {
        throw std::invalid_argument("uint256 hex string is empty");
    }
    if(value.size() > SIZE * 2)
    {
        throw std::overflow_error("uint256 hex string exceeds 256 bits");
    }

    Storage result{0};
    for(const char digit : value)
    {
        result <<= 4;
        result += DecodeNibble(digit);
    }

    return Uint256{std::move(result)};
}

Uint256 Uint256::FromDecimal(std::string_view value)
{
    if(value.empty())
    {
        throw std::invalid_argument("uint256 decimal string is empty");
    }

    Storage result{0};
    for(const char digit : value)
    {
        if(digit < '0' || digit > '9')
        {
            throw std::invalid_argument("uint256 decimal string contains an invalid character");
        }

        result *= 10;
        result += static_cast<unsigned>(digit - '0');
    }

    return Uint256{std::move(result)};
}

Uint256 Uint256::FromBytes(const Bytes32& bytes)
{
    Storage result{0};
    for(const auto byte : bytes)
    {
        result <<= 8;
        result += byte;
    }
    return Uint256{std::move(result)};
}

Uint256::Bytes32 Uint256::ToBytes() const
{
    Bytes32 result{};
    Storage remaining = value_;

    for(auto iterator = result.rbegin(); iterator != result.rend(); ++iterator)
    {
        *iterator = static_cast<std::uint8_t>((remaining & 0xff).convert_to<unsigned>());
        remaining >>= 8;
    }

    return result;
}

std::string Uint256::ToDecimal() const
{
    return value_.str();
}

std::string Uint256::ToQuantity() const
{
    if(IsZero())
    {
        return "0x0";
    }

    static constexpr std::string_view DIGITS = "0123456789abcdef";
    Storage remaining = value_;
    std::string digits;

    while(remaining != 0)
    {
        digits.push_back(DIGITS[(remaining & 0x0f).convert_to<std::size_t>()]);
        remaining >>= 4;
    }

    std::reverse(digits.begin(), digits.end());
    return "0x" + digits;
}

bool Uint256::IsZero() const noexcept
{
    return value_ == 0;
}

Uint256& Uint256::operator+=(const Uint256& other)
{
    value_ += other.value_;
    return *this;
}

Uint256& Uint256::operator-=(const Uint256& other)
{
    if(value_ < other.value_)
    {
        throw std::overflow_error("uint256 subtraction underflow");
    }
    value_ -= other.value_;
    return *this;
}

Uint256& Uint256::operator*=(const Uint256& other)
{
    value_ *= other.value_;
    return *this;
}

Uint256& Uint256::operator/=(const Uint256& other)
{
    if(other.IsZero())
    {
        throw std::domain_error("uint256 division by zero");
    }
    value_ /= other.value_;
    return *this;
}

Uint256 operator+(Uint256 left, const Uint256& right)
{
    left += right;
    return left;
}

Uint256 operator-(Uint256 left, const Uint256& right)
{
    left -= right;
    return left;
}

Uint256 operator*(Uint256 left, const Uint256& right)
{
    left *= right;
    return left;
}

Uint256 operator/(Uint256 left, const Uint256& right)
{
    left /= right;
    return left;
}

bool operator==(const Uint256& left, const Uint256& right) noexcept
{
    return left.value_ == right.value_;
}

bool operator!=(const Uint256& left, const Uint256& right) noexcept
{
    return !(left == right);
}

bool operator<(const Uint256& left, const Uint256& right) noexcept
{
    return left.value_ < right.value_;
}

bool operator<=(const Uint256& left, const Uint256& right) noexcept
{
    return left.value_ <= right.value_;
}

bool operator>(const Uint256& left, const Uint256& right) noexcept
{
    return left.value_ > right.value_;
}

bool operator>=(const Uint256& left, const Uint256& right) noexcept
{
    return left.value_ >= right.value_;
}

}
