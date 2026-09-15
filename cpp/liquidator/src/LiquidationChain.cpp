#include "dlp/liquidator/LiquidationChain.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "dlp/ethereum/Abi.hpp"
#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::liquidator
{

namespace
{

[[nodiscard]] ethereum::Uint256 Word(const ethereum::Bytes& data, std::size_t index)
{
    const auto offset = index * ethereum::Uint256::SIZE;
    if(data.size() < offset + ethereum::Uint256::SIZE)
    {
        throw std::runtime_error("eth_call returned too little ABI data");
    }
    ethereum::Uint256::Bytes32 bytes{};
    std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(offset), bytes.size(), bytes.begin());
    return ethereum::Uint256::FromBytes(bytes);
}

}

class RpcLiquidationChain::Impl final
{
public:
    Impl(std::string endpoint, LiquidationContracts configuredContracts)
        : client(std::move(endpoint)), contracts(std::move(configuredContracts))
    {
    }

    [[nodiscard]] ethereum::Bytes Call(
        const ethereum::Address& destination,
        std::string_view signature,
        const std::vector<ethereum::AbiValue>& arguments = {}
    ) const
    {
        return client.EthCall(ethereum::TransactionCall{
            std::nullopt,
            destination,
            ethereum::Abi::EncodeFunction(signature, arguments),
            ethereum::Uint256{}
        });
    }

    ethereum::RpcClient client;
    LiquidationContracts contracts;
};

RpcLiquidationChain::RpcLiquidationChain(std::string endpoint, LiquidationContracts contracts)
    : implementation_(std::make_unique<Impl>(std::move(endpoint), std::move(contracts)))
{
}

RpcLiquidationChain::~RpcLiquidationChain() = default;
RpcLiquidationChain::RpcLiquidationChain(RpcLiquidationChain&& other) noexcept = default;
RpcLiquidationChain& RpcLiquidationChain::operator=(RpcLiquidationChain&& other) noexcept = default;

risk::MarketSnapshot RpcLiquidationChain::LoadMarket() const
{
    const auto chainId = implementation_->client.GetChainId();
    const auto head = implementation_->client.GetBlockNumber();
    const auto block = implementation_->client.GetBlockByNumber(head);
    if(!block.has_value())
    {
        throw std::runtime_error("latest block is unavailable");
    }

    const auto wethPriceData = implementation_->Call(
        implementation_->contracts.oracle,
        "assetPrices(address)",
        {implementation_->contracts.weth}
    );
    const auto usdcPriceData = implementation_->Call(
        implementation_->contracts.oracle,
        "assetPrices(address)",
        {implementation_->contracts.usdc}
    );

    return risk::MarketSnapshot{
        chainId,
        head.ToUint64(),
        block->hash,
        implementation_->contracts.pool,
        implementation_->contracts.oracle,
        implementation_->contracts.weth,
        implementation_->contracts.usdc,
        risk::RiskCalculator::Ray(),
        Word(wethPriceData, 0),
        Word(wethPriceData, 1),
        Word(wethPriceData, 2),
        Word(usdcPriceData, 0),
        Word(usdcPriceData, 1),
        Word(usdcPriceData, 2),
        block->timestamp.ToUint64(),
        ethereum::Uint256{},
        ethereum::Uint256{},
        ethereum::Uint256{}
    };
}

risk::Position RpcLiquidationChain::LoadPosition(const ethereum::Address& borrower) const
{
    return risk::Position{
        borrower,
        Word(
            implementation_->Call(
                implementation_->contracts.pool,
                "wethCollateral(address)",
                {borrower}
            ),
            0
        ),
        Word(
            implementation_->Call(
                implementation_->contracts.pool,
                "usdcDebt(address)",
                {borrower}
            ),
            0
        )
    };
}

ethereum::Uint256 RpcLiquidationChain::EstimateGas(
    const ethereum::Address& sender,
    const ethereum::Address& destination,
    const ethereum::Bytes& data
) const
{
    return implementation_->client.EstimateGas(
        ethereum::TransactionCall{sender, destination, data, ethereum::Uint256{}}
    );
}

tx::FeeQuote RpcLiquidationChain::GetFeeQuote() const
{
    const auto blockNumber = implementation_->client.GetBlockNumber();
    const auto block = implementation_->client.GetBlockByNumber(blockNumber);
    if(!block.has_value() || !block->baseFeePerGas.has_value())
    {
        throw std::runtime_error("latest block does not contain an EIP-1559 base fee");
    }
    const auto priorityFee = implementation_->client.GetMaxPriorityFeePerGas();
    return tx::FeeQuote{
        priorityFee,
        *block->baseFeePerGas * ethereum::Uint256{2} + priorityFee
    };
}

}
