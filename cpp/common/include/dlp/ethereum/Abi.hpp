#ifndef DLP_ETHEREUM_ABI_HPP
#define DLP_ETHEREUM_ABI_HPP

#include <array>
#include <cstdint>
#include <string_view>
#include <variant>
#include <vector>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Keccak.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::ethereum
{

enum class AbiType
{
    Address,
    Uint256,
    Bytes32
};

struct AbiParameter
{
    AbiType type;
    bool indexed;
};

using FunctionSelector = std::array<std::uint8_t, 4>;
using AbiValue = std::variant<Address, Uint256, Hash256>;

class Abi final
{
public:
    [[nodiscard]] static FunctionSelector GetFunctionSelector(std::string_view signature);
    [[nodiscard]] static Hash256 GetEventTopic(std::string_view signature);

    [[nodiscard]] static Hash256 EncodeWord(const AbiValue& value);
    [[nodiscard]] static Bytes EncodeFunction(
        std::string_view signature,
        const std::vector<AbiValue>& arguments
    );

    [[nodiscard]] static AbiValue DecodeWord(AbiType type, const Hash256& word);

    [[nodiscard]] static std::vector<AbiValue> DecodeEvent(
        std::string_view signature,
        const std::vector<AbiParameter>& parameters,
        const std::vector<Hash256>& topics,
        const Bytes& data
    );
};

}

#endif
