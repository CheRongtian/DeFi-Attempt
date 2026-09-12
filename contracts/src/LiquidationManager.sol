// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {LendingPool} from "./LendingPool.sol";
import {RiskManager} from "./RiskManager.sol";
import {MathLib} from "./libraries/MathLib.sol";

/// @title MVP Liquidation Manager
/// @notice Validates unhealthy positions and calculates capped WETH-for-USDC liquidations.
contract LiquidationManager {
    uint256 public constant CLOSE_FACTOR_BPS = 5_000;
    uint256 public constant LIQUIDATION_BONUS_BPS = 500;

    LendingPool public immutable LENDING_POOL;
    RiskManager public immutable RISK_MANAGER;
    address public immutable WETH;
    address public immutable USDC;

    error InvalidAddress();
    error ZeroAmount();
    error UnsupportedDebtAsset(address asset);
    error UnsupportedCollateralAsset(address asset);
    error PositionHasNoDebt(address borrower);
    error PositionIsHealthy(uint256 healthFactor);
    error PositionHasNoCollateral(address borrower);
    error LiquidationAmountTooSmall();
    error CollateralOutputBelowMinimum(uint256 collateralOut, uint256 minimum);

    constructor(LendingPool lendingPool_) {
        if (address(lendingPool_) == address(0)) revert InvalidAddress();

        LENDING_POOL = lendingPool_;
        RISK_MANAGER = lendingPool_.RISK_MANAGER();
        WETH = address(lendingPool_.WETH());
        USDC = address(lendingPool_.USDC());
    }

    /// @notice Returns true when a borrower has debt and a health factor below one WAD.
    /// @dev Reverts through the oracle when a required price is missing or stale.
    function isLiquidatable(address borrower) public view returns (bool) {
        uint256 debt = LENDING_POOL.usdcDebt(borrower);
        if (debt == 0) return false;

        return RISK_MANAGER.healthFactor(LENDING_POOL.wethCollateral(borrower), debt) < MathLib.WAD;
    }

    /// @notice Repays unhealthy USDC debt and transfers the calculated WETH collateral to the caller.
    /// @return repaidAmount Actual USDC base units collected after all caps.
    /// @return collateralSeized Actual WETH base units transferred to the liquidator.
    /// @return recognizedBadDebt USDC debt written off when all collateral is exhausted.
    function liquidate(
        address borrower,
        address debtAsset,
        address collateralAsset,
        uint256 requestedRepay,
        uint256 minCollateralOut
    ) external returns (uint256 repaidAmount, uint256 collateralSeized, uint256 recognizedBadDebt) {
        if (borrower == address(0)) revert InvalidAddress();
        if (debtAsset != USDC) revert UnsupportedDebtAsset(debtAsset);
        if (collateralAsset != WETH) revert UnsupportedCollateralAsset(collateralAsset);
        if (requestedRepay == 0) revert ZeroAmount();

        (uint256 debt, uint256 collateral) = _validatedPosition(borrower);
        (repaidAmount, collateralSeized) = _calculateLiquidation(debt, collateral, requestedRepay);

        if (collateralSeized < minCollateralOut) {
            revert CollateralOutputBelowMinimum(collateralSeized, minCollateralOut);
        }

        recognizedBadDebt = LENDING_POOL.executeLiquidation(borrower, msg.sender, repaidAmount, collateralSeized);
    }

    function _validatedPosition(address borrower) private view returns (uint256 debt, uint256 collateral) {
        debt = LENDING_POOL.usdcDebt(borrower);
        if (debt == 0) revert PositionHasNoDebt(borrower);

        collateral = LENDING_POOL.wethCollateral(borrower);
        if (collateral == 0) revert PositionHasNoCollateral(borrower);

        uint256 healthFactor = RISK_MANAGER.healthFactor(collateral, debt);
        if (healthFactor >= MathLib.WAD) revert PositionIsHealthy(healthFactor);
    }

    function _calculateLiquidation(uint256 debt, uint256 collateral, uint256 requestedRepay)
        private
        view
        returns (uint256 repaidAmount, uint256 collateralSeized)
    {
        uint256 closeFactorLimit = MathLib.mulDivDown(debt, CLOSE_FACTOR_BPS, MathLib.BPS);

        // Resolve a sub-1-USDC remainder in one transaction so liquidation cannot create debt dust.
        if (debt - closeFactorLimit < LENDING_POOL.MINIMUM_USDC_BORROW()) closeFactorLimit = debt;

        uint256 collateralLimitedValueWad = MathLib.mulDivDown(
            RISK_MANAGER.collateralValue(collateral), MathLib.BPS, MathLib.BPS + LIQUIDATION_BONUS_BPS
        );
        uint256 collateralLimitedRepay = RISK_MANAGER.debtAmountForValueDown(collateralLimitedValueWad);

        repaidAmount = _min(requestedRepay, _min(closeFactorLimit, collateralLimitedRepay));

        // A request-limited liquidation must leave either zero debt or at least the protocol minimum.
        uint256 remainingDebt = debt - repaidAmount;
        bool collateralLimited = collateralLimitedRepay <= requestedRepay && collateralLimitedRepay <= closeFactorLimit;
        if (!collateralLimited && remainingDebt != 0 && remainingDebt < LENDING_POOL.MINIMUM_USDC_BORROW()) {
            repaidAmount = debt - LENDING_POOL.MINIMUM_USDC_BORROW();
        }
        if (repaidAmount == 0) revert LiquidationAmountTooSmall();

        uint256 collateralValueWithBonusWad =
            MathLib.mulDivDown(RISK_MANAGER.debtValue(repaidAmount), MathLib.BPS + LIQUIDATION_BONUS_BPS, MathLib.BPS);
        collateralSeized = RISK_MANAGER.collateralAmountForValueDown(collateralValueWithBonusWad);
        if (collateralSeized == 0) revert LiquidationAmountTooSmall();

        // A collateral-limited liquidation consumes the final rounding remainder and enables explicit bad-debt state.
        if (collateralLimited) {
            collateralSeized = collateral;
        }
    }

    function _min(uint256 a, uint256 b) private pure returns (uint256) {
        return a < b ? a : b;
    }
}
