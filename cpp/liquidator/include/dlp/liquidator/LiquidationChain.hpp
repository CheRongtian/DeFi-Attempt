#ifndef DLP_LIQUIDATOR_LIQUIDATION_CHAIN_HPP
#define DLP_LIQUIDATOR_LIQUIDATION_CHAIN_HPP

#include <memory>
#include <string>

#include "dlp/ethereum/RpcClient.hpp"
#include "dlp/risk/RiskTypes.hpp"
#include "dlp/tx/TxTypes.hpp"

namespace dlp::liquidator
{

struct LiquidationContracts
{
    ethereum::Address pool;
    ethereum::Address liquidationManager;
    ethereum::Address oracle;
    ethereum::Address weth;
    ethereum::Address usdc;
};

class LiquidationChain
{
public:
    virtual ~LiquidationChain() = default;

    [[nodiscard]] virtual risk::MarketSnapshot LoadMarket() const = 0;
    [[nodiscard]] virtual risk::Position LoadPosition(const ethereum::Address& borrower) const = 0;
    [[nodiscard]] virtual ethereum::Uint256 EstimateGas(
        const ethereum::Address& sender,
        const ethereum::Address& destination,
        const ethereum::Bytes& data
    ) const = 0;
    [[nodiscard]] virtual tx::FeeQuote GetFeeQuote() const = 0;
};

class RpcLiquidationChain final : public LiquidationChain
{
public:
    RpcLiquidationChain(std::string endpoint, LiquidationContracts contracts);
    ~RpcLiquidationChain() override;

    RpcLiquidationChain(RpcLiquidationChain&& other) noexcept;
    RpcLiquidationChain& operator=(RpcLiquidationChain&& other) noexcept;

    RpcLiquidationChain(const RpcLiquidationChain&) = delete;
    RpcLiquidationChain& operator=(const RpcLiquidationChain&) = delete;

    [[nodiscard]] risk::MarketSnapshot LoadMarket() const override;
    [[nodiscard]] risk::Position LoadPosition(const ethereum::Address& borrower) const override;
    [[nodiscard]] ethereum::Uint256 EstimateGas(
        const ethereum::Address& sender,
        const ethereum::Address& destination,
        const ethereum::Bytes& data
    ) const override;
    [[nodiscard]] tx::FeeQuote GetFeeQuote() const override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
