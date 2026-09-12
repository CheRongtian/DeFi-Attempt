// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {PriceOracle} from "./PriceOracle.sol";
import {MathLib} from "./libraries/MathLib.sol";

/// @title Lending Risk Manager
/// @notice Calculates USD WAD values for the MVP's WETH collateral and USDC debt.
contract RiskManager {
    uint8 private constant WETH_DECIMALS = 18;
    uint8 private constant USDC_DECIMALS = 6;

    uint256 public constant WETH_LTV_BPS = 7_500;
    uint256 public constant WETH_LIQUIDATION_THRESHOLD_BPS = 8_000;

    PriceOracle public immutable PRICE_ORACLE;
    address public immutable WETH;
    address public immutable USDC;

    error InvalidAddress();

    constructor(PriceOracle priceOracle_, address weth_, address usdc_) {
        if (address(priceOracle_) == address(0) || weth_ == address(0) || usdc_ == address(0)) {
            revert InvalidAddress();
        }

        PRICE_ORACLE = priceOracle_;
        WETH = weth_;
        USDC = usdc_;
    }

    /// @notice Returns the USD WAD value of WETH collateral, rounded down.
    /// @dev Reverts through PriceOracle when the WETH price is missing or stale.
    function collateralValue(uint256 wethAmount) public view returns (uint256) {
        uint256 wethPrice = PRICE_ORACLE.getPrice(WETH);
        return MathLib.toUsdWadDown(wethAmount, wethPrice, WETH_DECIMALS);
    }

    /// @notice Returns the USD WAD value of USDC debt, rounded up.
    /// @dev Reverts through PriceOracle when the USDC price is missing or stale.
    function debtValue(uint256 usdcAmount) public view returns (uint256) {
        uint256 usdcPrice = PRICE_ORACLE.getPrice(USDC);
        return MathLib.toUsdWadUp(usdcAmount, usdcPrice, USDC_DECIMALS);
    }

    /// @notice Converts a USD WAD value to WETH base units, rounding down.
    function collateralAmountForValueDown(uint256 usdValueWad) public view returns (uint256) {
        uint256 wethPrice = PRICE_ORACLE.getPrice(WETH);
        return MathLib.mulDivDown(usdValueWad, MathLib.ORACLE_PRICE_SCALE, wethPrice);
    }

    /// @notice Converts a USD WAD value to USDC base units, rounding down.
    function debtAmountForValueDown(uint256 usdValueWad) public view returns (uint256) {
        uint256 usdcPrice = PRICE_ORACLE.getPrice(USDC);
        uint256 valueAtTokenPrecision = MathLib.mulDivDown(usdValueWad, 1, usdcPrice);
        return MathLib.mulDivDown(valueAtTokenPrecision, 1, MathLib.BPS);
    }

    /// @notice Returns the maximum USDC debt value in USD WAD allowed by the WETH LTV.
    function maxBorrow(uint256 wethCollateralAmount) public view returns (uint256) {
        return MathLib.mulDivDown(collateralValue(wethCollateralAmount), WETH_LTV_BPS, MathLib.BPS);
    }

    /// @notice Returns the position health factor in WAD using the liquidation threshold.
    /// @dev A position without debt is always healthy and does not require current oracle prices.
    function healthFactor(uint256 wethCollateralAmount, uint256 usdcDebtAmount) public view returns (uint256) {
        if (usdcDebtAmount == 0) return type(uint256).max;

        uint256 adjustedCollateralValue =
            MathLib.mulDivDown(collateralValue(wethCollateralAmount), WETH_LIQUIDATION_THRESHOLD_BPS, MathLib.BPS);
        return MathLib.mulDivDown(adjustedCollateralValue, MathLib.WAD, debtValue(usdcDebtAmount));
    }
}
