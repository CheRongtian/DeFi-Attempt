#ifndef DLP_ETHEREUM_KECCAK_HPP
#define DLP_ETHEREUM_KECCAK_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "dlp/ethereum/Hex.hpp"

namespace dlp::ethereum
{

using Hash256 = std::array<std::uint8_t, 32>;

class Keccak final
{
public:
    [[nodiscard]] static Hash256 Hash(const Bytes& data) noexcept;
    [[nodiscard]] static Hash256 Hash(std::string_view data) noexcept;

private:
    [[nodiscard]] static Hash256 HashBuffer(const std::uint8_t* data, std::size_t size) noexcept;
};

}

#endif
