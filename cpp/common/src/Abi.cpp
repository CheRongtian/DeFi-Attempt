#include "dlp/ethereum/Abi.hpp"

#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace dlp::ethereum
{

namespace
{

constexpr std::size_t ABI_WORD_SIZE = 32;

}

FunctionSelector Abi::GetFunctionSelector(std::string_view signature)
{
    const auto hash = Keccak::Hash(signature);

    FunctionSelector selector{};
    std::copy_n(hash.begin(), selector.size(), selector.begin());
    return selector;
}

Hash256 Abi::GetEventTopic(std::string_view signature)
{
    return Keccak::Hash(signature);
}

Hash256 Abi::EncodeWord(const AbiValue& value)
{
    return std::visit(
        [](const auto& typedValue) -> Hash256 {
            using Value = std::decay_t<decltype(typedValue)>;
            Hash256 word{};

            if constexpr(std::is_same_v<Value, Address>)
            {
                const auto& bytes = typedValue.GetBytes();
                std::copy(bytes.begin(), bytes.end(), word.end() - static_cast<std::ptrdiff_t>(bytes.size()));
            }
            else if constexpr(std::is_same_v<Value, Uint256>)
            {
                const auto bytes = typedValue.ToBytes();
                std::copy(bytes.begin(), bytes.end(), word.begin());
            }
            else
            {
                std::copy(typedValue.begin(), typedValue.end(), word.begin());
            }

            return word;
        },
        value
    );
}

Bytes Abi::EncodeFunction(std::string_view signature, const std::vector<AbiValue>& arguments)
{
    const auto selector = GetFunctionSelector(signature);
    Bytes result;
    result.reserve(selector.size() + arguments.size() * ABI_WORD_SIZE);
    result.insert(result.end(), selector.begin(), selector.end());

    for(const auto& argument : arguments)
    {
        auto word = EncodeWord(argument);
        result.insert(result.end(), word.begin(), word.end());
    }

    return result;
}

AbiValue Abi::DecodeWord(AbiType type, const Hash256& word)
{
    switch(type)
    {
        case AbiType::Address:
        {
            if(std::any_of(word.begin(), word.begin() + 12, [](std::uint8_t value) { return value != 0; }))
            {
                throw std::invalid_argument("ABI address contains non-zero padding");
            }

            Address::Storage addressBytes{};
            std::copy(word.begin() + 12, word.end(), addressBytes.begin());
            return Address{addressBytes};
        }
        case AbiType::Uint256:
        {
            Uint256::Bytes32 integerBytes{};
            std::copy(word.begin(), word.end(), integerBytes.begin());
            return Uint256::FromBytes(integerBytes);
        }
        case AbiType::Bytes32:
            return word;
    }

    throw std::invalid_argument("unsupported ABI type");
}

std::vector<AbiValue> Abi::DecodeEvent(
    std::string_view signature,
    const std::vector<AbiParameter>& parameters,
    const std::vector<Hash256>& topics,
    const Bytes& data
)
{
    const auto indexedCount = static_cast<std::size_t>(std::count_if(
        parameters.begin(),
        parameters.end(),
        [](const AbiParameter& parameter) { return parameter.indexed; }
    ));

    if(topics.size() != indexedCount + 1)
    {
        throw std::invalid_argument("event topic count does not match its ABI");
    }
    if(topics.front() != GetEventTopic(signature))
    {
        throw std::invalid_argument("event signature topic does not match its ABI");
    }

    const auto dataWordCount = parameters.size() - indexedCount;
    if(data.size() != dataWordCount * ABI_WORD_SIZE)
    {
        throw std::invalid_argument("event data size does not match its ABI");
    }

    std::vector<AbiValue> result;
    result.reserve(parameters.size());
    std::size_t topicIndex = 1;
    std::size_t dataOffset = 0;

    for(const auto& parameter : parameters)
    {
        if(parameter.indexed)
        {
            result.push_back(DecodeWord(parameter.type, topics[topicIndex]));
            ++topicIndex;
        }
        else
        {
            Hash256 word{};
            std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(dataOffset), ABI_WORD_SIZE, word.begin());
            result.push_back(DecodeWord(parameter.type, word));
            dataOffset += ABI_WORD_SIZE;
        }
    }

    return result;
}

}
