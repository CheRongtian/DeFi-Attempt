#ifndef DLP_ETHEREUM_ADDRESS_HPP
#define DLP_ETHEREUM_ADDRESS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace dlp::ethereum
{

class Address final
{
public:
    static constexpr std::size_t SIZE = 20;
    using Storage = std::array<std::uint8_t, SIZE>;

    Address() noexcept = default;
    explicit Address(Storage bytes) noexcept;

    [[nodiscard]] static Address FromHex(std::string_view value);

    [[nodiscard]] const Storage& GetBytes() const noexcept;
    [[nodiscard]] std::string ToHex(bool includePrefix = true) const;
    [[nodiscard]] bool IsZero() const noexcept;

    friend bool operator==(const Address& left, const Address& right) noexcept;
    friend bool operator!=(const Address& left, const Address& right) noexcept;
    friend bool operator<(const Address& left, const Address& right) noexcept;

private:
    Storage bytes_{};
};

}

#endif
