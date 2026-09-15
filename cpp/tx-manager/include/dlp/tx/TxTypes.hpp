#ifndef DLP_TX_TX_TYPES_HPP
#define DLP_TX_TX_TYPES_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Keccak.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::tx
{

enum class TxStatus
{
    Pending,
    Submitted,
    Included,
    Finalized,
    Failed,
    Replaced,
    Reorged
};

[[nodiscard]] std::string_view ToString(TxStatus status) noexcept;
[[nodiscard]] TxStatus TxStatusFromString(std::string_view value);

struct TxJob
{
    std::string jobId;
    ethereum::Uint256 chainId;
    ethereum::Address wallet;
    ethereum::Address to;
    ethereum::Uint256 value;
    ethereum::Bytes data;
    TxStatus status{TxStatus::Pending};
    std::optional<ethereum::Uint256> nonce;
    std::optional<ethereum::Uint256> maxPriorityFeePerGas;
    std::optional<ethereum::Uint256> maxFeePerGas;
    std::optional<ethereum::Uint256> gasLimit;
    ethereum::Bytes rawTransaction;
    std::optional<ethereum::Hash256> transactionHash;
    std::uint32_t retryCount{0};
    std::optional<std::uint64_t> submittedBlockNumber;
    std::optional<std::uint64_t> includedBlockNumber;
    std::optional<ethereum::Hash256> includedBlockHash;
    std::uint64_t confirmationCount{0};
    std::string errorMessage;
};

struct FeeQuote
{
    ethereum::Uint256 maxPriorityFeePerGas;
    ethereum::Uint256 maxFeePerGas;
};

}

#endif
