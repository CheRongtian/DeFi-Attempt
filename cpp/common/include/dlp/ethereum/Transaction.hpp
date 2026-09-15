#ifndef DLP_ETHEREUM_TRANSACTION_HPP
#define DLP_ETHEREUM_TRANSACTION_HPP

#include <memory>
#include <optional>
#include <string_view>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Keccak.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::ethereum
{

struct Eip1559Transaction
{
    Uint256 chainId;
    Uint256 nonce;
    Uint256 maxPriorityFeePerGas;
    Uint256 maxFeePerGas;
    Uint256 gasLimit;
    std::optional<Address> to;
    Uint256 value;
    Bytes data;
};

struct Eip1559Signature
{
    Uint256 yParity;
    Uint256 r;
    Uint256 s;
};

class Eip1559Encoder final
{
public:
    [[nodiscard]] static Bytes EncodeSigningPayload(const Eip1559Transaction& transaction);
    [[nodiscard]] static Hash256 SigningHash(const Eip1559Transaction& transaction);
    [[nodiscard]] static Bytes EncodeSigned(
        const Eip1559Transaction& transaction,
        const Eip1559Signature& signature
    );
    [[nodiscard]] static Hash256 TransactionHash(const Bytes& rawTransaction);
};

class Secp256k1Signer final
{
public:
    explicit Secp256k1Signer(std::string_view privateKeyHex);
    ~Secp256k1Signer();

    Secp256k1Signer(Secp256k1Signer&& other) noexcept;
    Secp256k1Signer& operator=(Secp256k1Signer&& other) noexcept;

    Secp256k1Signer(const Secp256k1Signer&) = delete;
    Secp256k1Signer& operator=(const Secp256k1Signer&) = delete;

    [[nodiscard]] const Address& GetAddress() const noexcept;
    [[nodiscard]] Eip1559Signature Sign(const Hash256& hash) const;
    [[nodiscard]] Bytes SignTransaction(const Eip1559Transaction& transaction) const;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
