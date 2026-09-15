#ifndef DLP_API_API_TYPES_HPP
#define DLP_API_API_TYPES_HPP

#include <cstdint>
#include <string>

#include "dlp/ethereum/Address.hpp"
#include "dlp/ethereum/Keccak.hpp"
#include "dlp/ethereum/Uint256.hpp"

namespace dlp::api
{

struct LiquidationRecord
{
    std::uint64_t blockNumber{0};
    ethereum::Hash256 transactionHash{};
    ethereum::Address liquidator;
    ethereum::Address borrower;
    ethereum::Uint256 repaidAmount;
    ethereum::Uint256 collateralSeized;
};

struct ProtocolStats
{
    std::uint64_t positionCount{0};
    std::uint64_t liquidationCount{0};
    ethereum::Uint256 totalWethCollateral;
    ethereum::Uint256 totalScaledUsdcDebt;
    ethereum::Uint256 availableUsdcLiquidity;
    ethereum::Uint256 protocolReserve;
    ethereum::Uint256 badDebt;
};

struct ApiRequest
{
    std::string method;
    std::string target;
    std::string body;
};

struct ApiResponse
{
    unsigned status{200};
    std::string body;
};

}

#endif
