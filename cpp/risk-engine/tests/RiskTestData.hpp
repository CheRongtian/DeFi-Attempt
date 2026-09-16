#ifndef DLP_RISK_TEST_DATA_HPP
#define DLP_RISK_TEST_DATA_HPP

#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::risk::test
{

inline ethereum::Address Address(std::uint8_t suffix)
{
    ethereum::Address::Storage bytes{};
    bytes.back() = suffix;
    return ethereum::Address{bytes};
}

inline MarketSnapshot Market(std::uint64_t timestamp = 1'700'000'000)
{
    ethereum::Hash256 hash{};
    hash.back() = 1;
    return MarketSnapshot{
        ethereum::Uint256{31337},
        10,
        hash,
        Address(1),
        Address(2),
        Address(3),
        Address(4),
        RiskCalculator::Ray(),
        ethereum::Uint256{300'000'000'000},
        ethereum::Uint256{timestamp},
        ethereum::Uint256{86'400},
        ethereum::Uint256{100'000'000},
        ethereum::Uint256{timestamp},
        ethereum::Uint256{86'400},
        timestamp,
        ethereum::Uint256::FromDecimal("90000000000"),
        ethereum::Uint256{},
        ethereum::Uint256{timestamp},
        1
    };
}

}

#endif
