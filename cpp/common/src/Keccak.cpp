#include "dlp/ethereum/Keccak.hpp"

#include <algorithm>
#include <iterator>

#include <ethash/keccak.h>

namespace dlp::ethereum
{

Hash256 Keccak::HashBuffer(const std::uint8_t* data, std::size_t size) noexcept
{
    const auto result = ethash_keccak256(data, size);
    Hash256 hash{};
    std::copy(std::begin(result.bytes), std::end(result.bytes), hash.begin());
    return hash;
}

Hash256 Keccak::Hash(const Bytes& data) noexcept
{
    return HashBuffer(data.data(), data.size());
}

Hash256 Keccak::Hash(std::string_view data) noexcept
{
    return HashBuffer(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

}
