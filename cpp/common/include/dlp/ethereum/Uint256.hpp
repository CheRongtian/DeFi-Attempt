#ifndef DLP_ETHEREUM_UINT256_HPP
#define DLP_ETHEREUM_UINT256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <boost/multiprecision/cpp_int.hpp>

namespace dlp::ethereum
{

class Uint256 final
{
public:
    static constexpr std::size_t SIZE = 32;
    using Bytes32 = std::array<std::uint8_t, SIZE>;

    Uint256() noexcept = default;
    explicit Uint256(std::uint64_t value) noexcept;

    [[nodiscard]] static Uint256 FromHex(std::string_view value);
    [[nodiscard]] static Uint256 FromDecimal(std::string_view value);
    [[nodiscard]] static Uint256 FromBytes(const Bytes32& bytes);

    [[nodiscard]] Bytes32 ToBytes() const;
    [[nodiscard]] std::string ToDecimal() const;
    [[nodiscard]] std::string ToQuantity() const;
    [[nodiscard]] bool IsZero() const noexcept;

    Uint256& operator+=(const Uint256& other);
    Uint256& operator-=(const Uint256& other);
    Uint256& operator*=(const Uint256& other);
    Uint256& operator/=(const Uint256& other);

    friend Uint256 operator+(Uint256 left, const Uint256& right);
    friend Uint256 operator-(Uint256 left, const Uint256& right);
    friend Uint256 operator*(Uint256 left, const Uint256& right);
    friend Uint256 operator/(Uint256 left, const Uint256& right);

    friend bool operator==(const Uint256& left, const Uint256& right) noexcept;
    friend bool operator!=(const Uint256& left, const Uint256& right) noexcept;
    friend bool operator<(const Uint256& left, const Uint256& right) noexcept;
    friend bool operator<=(const Uint256& left, const Uint256& right) noexcept;
    friend bool operator>(const Uint256& left, const Uint256& right) noexcept;
    friend bool operator>=(const Uint256& left, const Uint256& right) noexcept;

private:
    using Storage = boost::multiprecision::checked_uint256_t;

    explicit Uint256(Storage value) noexcept;

    Storage value_{0};
};

}

#endif
