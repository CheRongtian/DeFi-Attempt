#include "dlp/api/ApiService.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "dlp/ethereum/Hex.hpp"
#include "dlp/risk/RiskCalculator.hpp"

namespace dlp::api
{

namespace
{

using Json = nlohmann::json;

[[nodiscard]] std::uint64_t UnixTime()
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count());
}

[[nodiscard]] Json Freshness(const risk::MarketSnapshot& market, std::uint64_t chainHead)
{
    return Json{
        {"indexedBlock", market.indexedBlock},
        {"indexedBlockHash", ethereum::Hex::Encode(market.indexedBlockHash)},
        {"chainHead", chainHead},
        {"indexLag", chainHead > market.indexedBlock ? chainHead - market.indexedBlock : 0U}
    };
}

[[nodiscard]] Json RiskJson(const risk::PositionRisk& value)
{
    return Json{
        {"address", value.position.user.ToHex()},
        {"wethCollateral", value.position.wethCollateral.ToDecimal()},
        {"usdcDebt", value.position.usdcDebt.ToDecimal()},
        {"collateralValueUsdWad", value.collateralValue.ToDecimal()},
        {"debtValueUsdWad", value.debtValue.ToDecimal()},
        {"maxBorrowUsdWad", value.maxBorrow.ToDecimal()},
        {"healthFactorWad", value.healthFactor.ToDecimal()},
        {"liquidatable", value.liquidatable}
    };
}

[[nodiscard]] ApiResponse JsonResponse(unsigned status, Json value)
{
    return ApiResponse{status, value.dump()};
}

}

ApiService::ApiService(
    risk::RiskEngine& riskEngine,
    ApiStore& store,
    ApiChain& chain,
    ethereum::Uint256 chainId
)
    : riskEngine_(riskEngine), store_(store), chain_(chain), chainId_(std::move(chainId))
{
}

ApiResponse ApiService::Markets() const
{
    const auto market = riskEngine_.LoadMarket();
    const auto head = chain_.GetBlockNumber();
    return JsonResponse(200, Json{
        {"data", Json::array({Json{
            {"chainId", market.chainId.ToDecimal()},
            {"pool", market.pool.ToHex()},
            {"oracle", market.oracle.ToHex()},
            {"weth", market.weth.ToHex()},
            {"usdc", market.usdc.ToHex()},
            {"borrowIndex", market.borrowIndex.ToDecimal()},
            {"wethDecimals", 18},
            {"usdcDecimals", 6},
            {"priceDecimals", 8},
            {"ltvBps", risk::RiskCalculator::LtvBasisPoints().ToDecimal()},
            {"liquidationThresholdBps", risk::RiskCalculator::LiquidationThresholdBasisPoints().ToDecimal()},
            {"wethPrice", market.wethPrice.ToDecimal()},
            {"usdcPrice", market.usdcPrice.ToDecimal()}
        }})},
        {"freshness", Freshness(market, head)}
    });
}

ApiResponse ApiService::Position(std::string_view address, bool healthOnly) const
{
    const auto user = ethereum::Address::FromHex(address);
    const auto value = riskEngine_.Evaluate(user, UnixTime());
    if(!value.has_value())
    {
        return JsonResponse(404, Json{{"error", "position not found"}});
    }
    const auto market = riskEngine_.LoadMarket();
    auto data = healthOnly
        ? Json{
            {"address", value->position.user.ToHex()},
            {"healthFactorWad", value->healthFactor.ToDecimal()},
            {"liquidatable", value->liquidatable}
        }
        : RiskJson(*value);
    if(!healthOnly)
    {
        data["usdcSupply"] = store_.LoadUsdcSupply(chainId_, user).ToDecimal();
    }
    return JsonResponse(200, Json{
        {"data", data},
        {"freshness", Freshness(market, chain_.GetBlockNumber())}
    });
}

ApiResponse ApiService::Liquidations() const
{
    const auto values = store_.LoadLiquidations(chainId_, 100);
    Json data = Json::array();
    for(const auto& value : values)
    {
        data.push_back(Json{
            {"blockNumber", value.blockNumber},
            {"transactionHash", ethereum::Hex::Encode(value.transactionHash)},
            {"liquidator", value.liquidator.ToHex()},
            {"borrower", value.borrower.ToHex()},
            {"repaidAmount", value.repaidAmount.ToDecimal()},
            {"collateralSeized", value.collateralSeized.ToDecimal()}
        });
    }
    const auto market = riskEngine_.LoadMarket();
    return JsonResponse(200, Json{
        {"data", std::move(data)},
        {"freshness", Freshness(market, chain_.GetBlockNumber())}
    });
}

ApiResponse ApiService::Stats() const
{
    const auto stats = store_.LoadProtocolStats(chainId_);
    const auto market = riskEngine_.LoadMarket();
    return JsonResponse(200, Json{
        {"data", Json{
            {"positionCount", stats.positionCount},
            {"liquidationCount", stats.liquidationCount},
            {"totalWethCollateral", stats.totalWethCollateral.ToDecimal()},
            {"totalScaledUsdcDebt", stats.totalScaledUsdcDebt.ToDecimal()},
            {"availableUsdcLiquidity", stats.availableUsdcLiquidity.ToDecimal()},
            {"protocolReserve", stats.protocolReserve.ToDecimal()},
            {"badDebt", stats.badDebt.ToDecimal()}
        }},
        {"freshness", Freshness(market, chain_.GetBlockNumber())}
    });
}

ApiResponse ApiService::Simulate(std::string_view body) const
{
    const auto request = Json::parse(body);
    auto market = riskEngine_.LoadMarket();
    if(request.contains("wethPrice"))
    {
        market.wethPrice = ethereum::Uint256::FromDecimal(request.at("wethPrice").get<std::string>());
    }
    if(request.contains("usdcPrice"))
    {
        market.usdcPrice = ethereum::Uint256::FromDecimal(request.at("usdcPrice").get<std::string>());
    }
    const auto now = UnixTime();
    market.wethPriceUpdatedAt = ethereum::Uint256{now};
    market.usdcPriceUpdatedAt = market.wethPriceUpdatedAt;
    market.wethMaxPriceAge = ethereum::Uint256{86'400};
    market.usdcMaxPriceAge = ethereum::Uint256{86'400};

    const risk::Position position{
        ethereum::Address::FromHex(request.at("address").get<std::string>()),
        ethereum::Uint256::FromDecimal(request.at("wethCollateral").get<std::string>()),
        ethereum::Uint256::FromDecimal(request.at("usdcDebt").get<std::string>())
    };
    return JsonResponse(200, Json{
        {"data", RiskJson(risk::RiskCalculator::Evaluate(position, market, now))},
        {"freshness", Freshness(market, chain_.GetBlockNumber())}
    });
}

ApiResponse ApiService::Handle(const ApiRequest& request) const
{
    try
    {
        if(request.method == "GET" && request.target == "/health")
        {
            return JsonResponse(200, Json{{"status", "healthy"}});
        }
        if(request.method == "GET" && request.target == "/ready")
        {
            return JsonResponse(200, Json{{"status", "ready"}});
        }
        if(request.method == "GET" && request.target == "/markets") return Markets();
        if(request.method == "GET" && request.target == "/liquidations") return Liquidations();
        if(request.method == "GET" && request.target == "/protocol/stats") return Stats();
        if(request.method == "POST" && request.target == "/risk/simulate") return Simulate(request.body);

        constexpr std::string_view prefix = "/positions/";
        constexpr std::string_view healthSuffix = "/health";
        if(request.method == "GET" && request.target.starts_with(prefix))
        {
            std::string_view address{request.target};
            address.remove_prefix(prefix.size());
            if(address.ends_with(healthSuffix))
            {
                address.remove_suffix(healthSuffix.size());
                return Position(address, true);
            }
            return Position(address, false);
        }
        return JsonResponse(404, Json{{"error", "route not found"}});
    }
    catch(const nlohmann::json::exception&)
    {
        return JsonResponse(400, Json{{"error", "invalid request"}});
    }
    catch(const std::invalid_argument&)
    {
        return JsonResponse(400, Json{{"error", "invalid request"}});
    }
    catch(const std::exception& exception)
    {
        std::clog << Json{
            {"event", "api_request_failed"},
            {"method", request.method},
            {"target", request.target},
            {"error", exception.what()}
        }.dump() << '\n';
        return JsonResponse(500, Json{{"error", "internal server error"}});
    }
}

}
