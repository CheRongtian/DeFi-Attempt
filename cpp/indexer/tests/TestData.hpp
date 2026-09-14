#ifndef DLP_INDEXER_TEST_DATA_HPP
#define DLP_INDEXER_TEST_DATA_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "dlp/ethereum/Abi.hpp"
#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/ProtocolAbi.hpp"
#include "dlp/ethereum/Uint256.hpp"
#include "dlp/indexer/IndexerTypes.hpp"

namespace dlp::indexer::test
{

inline ethereum::Address Address(std::uint8_t suffix)
{
    ethereum::Address::Storage bytes{};
    bytes.back() = suffix;
    return ethereum::Address{bytes};
}

inline ethereum::Hash256 Hash(std::uint8_t prefix, std::uint64_t suffix)
{
    ethereum::Hash256 hash{};
    hash.front() = prefix;
    for(std::size_t index = 0; index < sizeof(suffix); ++index)
    {
        hash[hash.size() - 1U - index] = static_cast<std::uint8_t>(suffix & 0xffU);
        suffix >>= 8U;
    }
    return hash;
}

inline ChainContracts Contracts()
{
    return ChainContracts{Address(1), Address(2), Address(3), Address(4)};
}

inline RawLog EventLog(
    ethereum::ProtocolEventKind kind,
    const ethereum::Address& emitter,
    const std::vector<ethereum::AbiValue>& values,
    std::uint64_t blockNumber = 1,
    std::uint64_t logIndex = 0,
    ethereum::Hash256 blockHash = Hash(0, 1)
)
{
    const auto& definition = ethereum::ProtocolAbi::GetEvent(kind);
    std::vector<ethereum::Hash256> topics{definition.topic};
    ethereum::Bytes data;

    for(std::size_t index = 0; index < definition.parameters.size(); ++index)
    {
        const auto word = ethereum::Abi::EncodeWord(values.at(index));
        if(definition.parameters[index].indexed)
        {
            topics.push_back(word);
        }
        else
        {
            data.insert(data.end(), word.begin(), word.end());
        }
    }

    return RawLog{
        ethereum::Uint256{31337},
        blockNumber,
        blockHash,
        Hash(9, blockNumber),
        0,
        logIndex,
        emitter,
        std::move(topics),
        std::move(data),
        true
    };
}

}

#endif
