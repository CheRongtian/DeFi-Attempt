#ifndef DLP_RISK_RISK_TYPES_HPP
#define DLP_RISK_RISK_TYPES_HPP

#include <cstdint>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/Keccak.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::risk
{

struct IndexedPosition
{
    ethereum::Address user;
    ethereum::Uint256 wethCollateral;
    ethereum::Uint256 scaledUsdcDebt;
};

struct Position
{
    ethereum::Address user;
    ethereum::Uint256 wethCollateral;
    ethereum::Uint256 usdcDebt;
};

struct MarketSnapshot
{
    ethereum::Uint256 chainId;
    std::uint64_t indexedBlock{0};
    ethereum::Hash256 indexedBlockHash{};
    ethereum::Address pool;
    ethereum::Address oracle;
    ethereum::Address weth;
    ethereum::Address usdc;
    ethereum::Uint256 borrowIndex;
    ethereum::Uint256 wethPrice;
    ethereum::Uint256 wethPriceUpdatedAt;
    ethereum::Uint256 wethMaxPriceAge;
    ethereum::Uint256 usdcPrice;
    ethereum::Uint256 usdcPriceUpdatedAt;
    ethereum::Uint256 usdcMaxPriceAge;
    std::uint64_t observedAt{0};
    ethereum::Uint256 availableUsdcLiquidity;
    ethereum::Uint256 totalScaledUsdcDebt;
    ethereum::Uint256 lastInterestTimestamp;
};

struct PositionRisk
{
    Position position;
    ethereum::Uint256 collateralValue;
    ethereum::Uint256 debtValue;
    ethereum::Uint256 maxBorrow;
    ethereum::Uint256 healthFactor;
    bool liquidatable{false};
    std::uint64_t blockNumber{0};
    ethereum::Hash256 blockHash{};
};

struct LiquidationCandidate
{
    ethereum::Address borrower;
    ethereum::Uint256 healthFactor;
    ethereum::Address debtAsset;
    ethereum::Address collateralAsset;
    ethereum::Uint256 maxRepay;
    ethereum::Uint256 expectedBonus;
    ethereum::Uint256 expectedCollateral;
    ethereum::Uint256 expectedBadDebt;
    std::uint64_t blockNumber{0};
    ethereum::Hash256 blockHash{};
};

}

#endif
