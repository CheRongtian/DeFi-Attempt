#include "dlp/ethereum/Transaction.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include "dlp/ethereum/Rlp.hpp"

namespace dlp::ethereum
{

namespace
{

struct ContextDeleter
{
    void operator()(secp256k1_context* context) const noexcept
    {
        secp256k1_context_destroy(context);
    }
};

using ContextPointer = std::unique_ptr<secp256k1_context, ContextDeleter>;

[[nodiscard]] Bytes AddressBytes(const std::optional<Address>& address)
{
    if(!address.has_value())
    {
        return {};
    }
    return Bytes{address->GetBytes().begin(), address->GetBytes().end()};
}

[[nodiscard]] std::vector<Bytes> UnsignedItems(const Eip1559Transaction& transaction)
{
    return {
        Rlp::EncodeUint256(transaction.chainId),
        Rlp::EncodeUint256(transaction.nonce),
        Rlp::EncodeUint256(transaction.maxPriorityFeePerGas),
        Rlp::EncodeUint256(transaction.maxFeePerGas),
        Rlp::EncodeUint256(transaction.gasLimit),
        Rlp::EncodeBytes(AddressBytes(transaction.to)),
        Rlp::EncodeUint256(transaction.value),
        Rlp::EncodeBytes(transaction.data),
        Rlp::EncodeList({})
    };
}

[[nodiscard]] Bytes TypedPayload(const std::vector<Bytes>& items)
{
    Bytes result{0x02U};
    const auto encoded = Rlp::EncodeList(items);
    result.insert(result.end(), encoded.begin(), encoded.end());
    return result;
}

[[nodiscard]] Uint256 Uint256FromSignaturePart(const unsigned char* bytes)
{
    Uint256::Bytes32 value{};
    std::copy_n(bytes, value.size(), value.begin());
    return Uint256::FromBytes(value);
}

}

Bytes Eip1559Encoder::EncodeSigningPayload(const Eip1559Transaction& transaction)
{
    return TypedPayload(UnsignedItems(transaction));
}

Hash256 Eip1559Encoder::SigningHash(const Eip1559Transaction& transaction)
{
    return Keccak::Hash(EncodeSigningPayload(transaction));
}

Bytes Eip1559Encoder::EncodeSigned(
    const Eip1559Transaction& transaction,
    const Eip1559Signature& signature
)
{
    auto items = UnsignedItems(transaction);
    items.push_back(Rlp::EncodeUint256(signature.yParity));
    items.push_back(Rlp::EncodeUint256(signature.r));
    items.push_back(Rlp::EncodeUint256(signature.s));
    return TypedPayload(items);
}

Hash256 Eip1559Encoder::TransactionHash(const Bytes& rawTransaction)
{
    return Keccak::Hash(rawTransaction);
}

class Secp256k1Signer::Impl final
{
public:
    explicit Impl(std::string_view privateKeyHex)
        : context_(secp256k1_context_create(SECP256K1_CONTEXT_SIGN))
    {
        const auto decoded = Hex::Decode(privateKeyHex);
        if(decoded.size() != privateKey_.size())
        {
            throw std::invalid_argument("private key must contain exactly 32 bytes");
        }
        std::copy(decoded.begin(), decoded.end(), privateKey_.begin());
        if(secp256k1_ec_seckey_verify(context_.get(), privateKey_.data()) != 1)
        {
            throw std::invalid_argument("private key is outside the secp256k1 scalar range");
        }

        secp256k1_pubkey publicKey{};
        if(secp256k1_ec_pubkey_create(context_.get(), &publicKey, privateKey_.data()) != 1)
        {
            throw std::runtime_error("failed to derive secp256k1 public key");
        }

        std::array<unsigned char, 65> serialized{};
        std::size_t serializedSize = serialized.size();
        if(secp256k1_ec_pubkey_serialize(
               context_.get(),
               serialized.data(),
               &serializedSize,
               &publicKey,
               SECP256K1_EC_UNCOMPRESSED
           ) != 1)
        {
            throw std::runtime_error("failed to serialize secp256k1 public key");
        }

        const Bytes publicKeyBody{serialized.begin() + 1, serialized.end()};
        const auto hash = Keccak::Hash(publicKeyBody);
        Address::Storage addressBytes{};
        std::copy(hash.end() - static_cast<std::ptrdiff_t>(addressBytes.size()), hash.end(), addressBytes.begin());
        address_ = Address{addressBytes};
    }

    [[nodiscard]] Eip1559Signature Sign(const Hash256& hash) const
    {
        secp256k1_ecdsa_recoverable_signature signature{};
        if(secp256k1_ecdsa_sign_recoverable(
               context_.get(),
               &signature,
               hash.data(),
               privateKey_.data(),
               nullptr,
               nullptr
           ) != 1)
        {
            throw std::runtime_error("failed to sign transaction hash");
        }

        std::array<unsigned char, 64> compact{};
        int recoveryId = 0;
        secp256k1_ecdsa_recoverable_signature_serialize_compact(
            context_.get(), compact.data(), &recoveryId, &signature
        );
        return Eip1559Signature{
            Uint256{static_cast<std::uint64_t>(recoveryId & 1)},
            Uint256FromSignaturePart(compact.data()),
            Uint256FromSignaturePart(compact.data() + 32)
        };
    }

    [[nodiscard]] const Address& GetAddress() const noexcept
    {
        return address_;
    }

private:
    ContextPointer context_;
    std::array<std::uint8_t, 32> privateKey_{};
    Address address_;
};

Secp256k1Signer::Secp256k1Signer(std::string_view privateKeyHex)
    : implementation_(std::make_unique<Impl>(privateKeyHex))
{
}

Secp256k1Signer::~Secp256k1Signer() = default;
Secp256k1Signer::Secp256k1Signer(Secp256k1Signer&& other) noexcept = default;
Secp256k1Signer& Secp256k1Signer::operator=(Secp256k1Signer&& other) noexcept = default;

const Address& Secp256k1Signer::GetAddress() const noexcept
{
    return implementation_->GetAddress();
}

Eip1559Signature Secp256k1Signer::Sign(const Hash256& hash) const
{
    return implementation_->Sign(hash);
}

Bytes Secp256k1Signer::SignTransaction(const Eip1559Transaction& transaction) const
{
    return Eip1559Encoder::EncodeSigned(transaction, Sign(Eip1559Encoder::SigningHash(transaction)));
}

}
