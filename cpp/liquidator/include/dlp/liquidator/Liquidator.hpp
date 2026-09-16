#ifndef DLP_LIQUIDATOR_LIQUIDATOR_HPP
#define DLP_LIQUIDATOR_LIQUIDATOR_HPP

#include <cstdint>
#include <optional>

#include "dlp/liquidator/LiquidationChain.hpp"

namespace dlp::liquidator
{

struct LiquidatorConfig
{
    ethereum::Address operatorAddress;
    ethereum::Address liquidationManager;
    ethereum::Uint256 minimumProfitUsdWad;
    std::uint64_t minimumCollateralBps{9'900};
};

struct LiquidationSubmission
{
    ethereum::Address destination;
    ethereum::Bytes data;
    ethereum::Uint256 value;
};

class Liquidator final
{
public:
    Liquidator(LiquidationChain& chain, LiquidatorConfig config);

    [[nodiscard]] std::optional<LiquidationSubmission> Prepare(
        const risk::LiquidationCandidate& indexedCandidate
    );

private:
    [[nodiscard]] ethereum::Bytes EncodeLiquidation(
        const risk::LiquidationCandidate& candidate
    ) const;
    LiquidationChain& chain_;
    LiquidatorConfig config_;
};

}

#endif
