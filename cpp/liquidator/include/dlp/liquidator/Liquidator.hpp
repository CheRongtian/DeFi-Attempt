#ifndef DLP_LIQUIDATOR_LIQUIDATOR_HPP
#define DLP_LIQUIDATOR_LIQUIDATOR_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "dlp/liquidator/LiquidationChain.hpp"
#include "dlp/risk/RiskEngine.hpp"
#include "dlp/tx/TxManager.hpp"

namespace dlp::liquidator
{

struct LiquidatorConfig
{
    ethereum::Address operatorAddress;
    ethereum::Address liquidationManager;
    ethereum::Uint256 minimumProfitUsdWad;
    std::uint64_t minimumCollateralBps{9'900};
};

class Liquidator final
{
public:
    Liquidator(
        risk::RiskEngine& riskEngine,
        LiquidationChain& chain,
        tx::TransactionQueue& transactionQueue,
        LiquidatorConfig config
    );

    [[nodiscard]] std::size_t RunOnce(std::size_t scanLimit, std::uint64_t evaluatedAt);
    [[nodiscard]] bool Process(const risk::LiquidationCandidate& indexedCandidate);

private:
    [[nodiscard]] ethereum::Bytes EncodeLiquidation(
        const risk::LiquidationCandidate& candidate
    ) const;
    [[nodiscard]] std::string JobId(const risk::LiquidationCandidate& candidate) const;

    risk::RiskEngine& riskEngine_;
    LiquidationChain& chain_;
    tx::TransactionQueue& transactionQueue_;
    LiquidatorConfig config_;
};

}

#endif
