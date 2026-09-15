#ifndef DLP_ETHEREUM_RPC_CLIENT_HPP
#define DLP_ETHEREUM_RPC_CLIENT_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Keccak.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::ethereum
{

enum class RpcErrorKind
{
    InvalidEndpoint,
    Transport,
    Http,
    Protocol,
    Remote,
    InvalidResponse
};

class RpcException final : public std::runtime_error
{
public:
    RpcException(RpcErrorKind kind, std::string message);

    [[nodiscard]] RpcErrorKind GetKind() const noexcept;

private:
    RpcErrorKind kind_;
};

struct BlockHeader
{
    Uint256 number;
    Hash256 hash;
    Hash256 parentHash;
    Uint256 timestamp;
    std::optional<Uint256> baseFeePerGas;
};

struct LogFilter
{
    std::optional<Uint256> fromBlock;
    std::optional<Uint256> toBlock;
    std::optional<Address> address;
    std::vector<std::optional<Hash256>> topics;
};

struct RpcLog
{
    Address address;
    std::vector<Hash256> topics;
    Bytes data;
    Uint256 blockNumber;
    Hash256 blockHash;
    Hash256 transactionHash;
    Uint256 transactionIndex;
    Uint256 logIndex;
    bool removed;
};

struct TransactionCall
{
    std::optional<Address> from;
    Address to;
    Bytes data;
    Uint256 value;
};

struct TransactionReceipt
{
    Hash256 transactionHash;
    Uint256 blockNumber;
    Hash256 blockHash;
    bool succeeded;
    Uint256 gasUsed;
    Uint256 effectiveGasPrice;
};

class RpcClient final
{
public:
    explicit RpcClient(
        std::string endpoint,
        std::chrono::milliseconds timeout = std::chrono::seconds(5)
    );
    ~RpcClient();

    RpcClient(RpcClient&& other) noexcept;
    RpcClient& operator=(RpcClient&& other) noexcept;

    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;

    [[nodiscard]] Uint256 GetChainId() const;
    [[nodiscard]] Uint256 GetBlockNumber() const;
    [[nodiscard]] std::optional<BlockHeader> GetBlockByNumber(const Uint256& number) const;
    [[nodiscard]] std::vector<RpcLog> GetLogs(const LogFilter& filter) const;
    [[nodiscard]] Bytes EthCall(const TransactionCall& call, std::string_view block = "latest") const;
    [[nodiscard]] Uint256 EstimateGas(const TransactionCall& call) const;
    [[nodiscard]] Uint256 GetTransactionCount(
        const Address& address,
        std::string_view block = "pending"
    ) const;
    [[nodiscard]] Uint256 GetMaxPriorityFeePerGas() const;
    [[nodiscard]] Hash256 SendRawTransaction(const Bytes& rawTransaction) const;
    [[nodiscard]] std::optional<TransactionReceipt> GetTransactionReceipt(
        const Hash256& transactionHash
    ) const;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
