#ifndef DLP_INDEXER_INDEXER_TYPES_HPP
#define DLP_INDEXER_INDEXER_TYPES_HPP

#include <cstdint>
#include <map>
#include <vector>

#include "dlp/ethereum/Abi.hpp"
#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/Hex.hpp"
#include "dlp/ethereum/Keccak.hpp"
#include "dlp/ethereum/ProtocolAbi.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::indexer
{

struct ChainContracts
{
    ethereum::Address pool;
    ethereum::Address oracle;
    ethereum::Address weth;
    ethereum::Address usdc;
};

struct IndexedBlock
{
    ethereum::Uint256 chainId;
    std::uint64_t number{0};
    ethereum::Hash256 hash{};
    ethereum::Hash256 parentHash{};
    bool canonical{true};
};

struct RawLog
{
    ethereum::Uint256 chainId;
    std::uint64_t blockNumber{0};
    ethereum::Hash256 blockHash{};
    ethereum::Hash256 transactionHash{};
    std::uint64_t transactionIndex{0};
    std::uint64_t logIndex{0};
    ethereum::Address contractAddress;
    std::vector<ethereum::Hash256> topics;
    ethereum::Bytes data;
    bool canonical{true};
};

struct SyncCursor
{
    ethereum::Uint256 chainId;
    std::uint64_t blockNumber{0};
    ethereum::Hash256 blockHash{};
};

struct PositionState
{
    ethereum::Address user;
    ethereum::Uint256 wethCollateral;
    ethereum::Uint256 scaledUsdcSupply;
    ethereum::Uint256 scaledUsdcDebt;
};

struct MarketState
{
    ChainContracts contracts;
    ethereum::Uint256 totalWethCollateral;
    ethereum::Uint256 totalScaledUsdcSupply;
    ethereum::Uint256 totalScaledUsdcDebt;
    ethereum::Uint256 availableUsdcLiquidity;
    ethereum::Uint256 protocolReserve;
    ethereum::Uint256 badDebt;
    ethereum::Uint256 borrowIndex;
    ethereum::Uint256 liquidityIndex;
    ethereum::Uint256 lastInterestTimestamp;
    ethereum::Uint256 wethPrice;
    ethereum::Uint256 wethPriceUpdatedAt;
    ethereum::Uint256 wethMaxPriceAge;
    ethereum::Uint256 usdcPrice;
    ethereum::Uint256 usdcPriceUpdatedAt;
    ethereum::Uint256 usdcMaxPriceAge;
};

struct LiquidationState
{
    ethereum::Uint256 chainId;
    std::uint64_t blockNumber{0};
    ethereum::Hash256 blockHash{};
    ethereum::Hash256 transactionHash{};
    std::uint64_t logIndex{0};
    ethereum::Address liquidator;
    ethereum::Address borrower;
    ethereum::Address debtAsset;
    ethereum::Address collateralAsset;
    ethereum::Uint256 repaidAmount;
    ethereum::Uint256 collateralSeized;
};

struct DerivedState
{
    std::map<ethereum::Address, PositionState> positions;
    MarketState market;
    std::vector<LiquidationState> liquidations;
};

struct DecodedProtocolEvent
{
    ethereum::ProtocolEventKind kind;
    std::vector<ethereum::AbiValue> values;
    RawLog source;
};

}

#endif
