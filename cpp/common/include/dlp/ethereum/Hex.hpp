#ifndef DLP_ETHEREUM_HEX_HPP
#define DLP_ETHEREUM_HEX_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace dlp::ethereum
{

using Bytes = std::vector<std::uint8_t>;

class Hex final
{
public:
    [[nodiscard]] static Bytes Decode(std::string_view value);

    [[nodiscard]] static std::string Encode(
        const Bytes& data,
        bool includePrefix = true
    );

    template<std::size_t Size>
    [[nodiscard]] static std::string Encode(
        const std::array<std::uint8_t, Size>& data,
        bool includePrefix = true
    )
    {
        return EncodeBuffer(data.data(), data.size(), includePrefix);
    }

private:
    [[nodiscard]] static std::string EncodeBuffer(
        const std::uint8_t* data,
        std::size_t size,
        bool includePrefix
    );

    [[nodiscard]] static std::uint8_t DecodeNibble(char value);
};

}

#endif
