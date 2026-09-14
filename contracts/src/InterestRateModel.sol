// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {MathLib} from "./libraries/MathLib.sol";

/// @title Kink Interest Rate Model
/// @notice Calculates utilization, annual borrow rate, and annual supplier rate in RAY precision.
contract InterestRateModel {
    uint256 public constant OPTIMAL_UTILIZATION = 80e25;
    uint256 public constant BASE_RATE = 2e25;
    uint256 public constant SLOPE_1 = 8e25;
    uint256 public constant SLOPE_2 = 1e27;

    error InvalidUtilization(uint256 utilization);
    error InvalidReserveFactor(uint256 reserveFactorBps);

    function utilization(uint256 availableLiquidity, uint256 totalDebt) public pure returns (uint256) {
        if (totalDebt == 0) return 0;
        return MathLib.mulDivDown(totalDebt, MathLib.RAY, availableLiquidity + totalDebt);
    }

    function borrowRate(uint256 utilizationRay) public pure returns (uint256) {
        if (utilizationRay > MathLib.RAY) revert InvalidUtilization(utilizationRay);

        if (utilizationRay <= OPTIMAL_UTILIZATION) {
            return BASE_RATE + MathLib.mulDivDown(utilizationRay, SLOPE_1, OPTIMAL_UTILIZATION);
        }

        uint256 excessUtilization = utilizationRay - OPTIMAL_UTILIZATION;
        uint256 excessRange = MathLib.RAY - OPTIMAL_UTILIZATION;
        return BASE_RATE + SLOPE_1 + MathLib.mulDivDown(excessUtilization, SLOPE_2, excessRange);
    }

    function liquidityRate(uint256 utilizationRay, uint256 borrowRateRay, uint256 reserveFactorBps)
        public
        pure
        returns (uint256)
    {
        if (utilizationRay > MathLib.RAY) revert InvalidUtilization(utilizationRay);
        if (reserveFactorBps > MathLib.BPS) revert InvalidReserveFactor(reserveFactorBps);

        uint256 grossLiquidityRate = MathLib.mulDivDown(borrowRateRay, utilizationRay, MathLib.RAY);
        return MathLib.mulDivDown(grossLiquidityRate, MathLib.BPS - reserveFactorBps, MathLib.BPS);
    }

    function marketRates(uint256 availableLiquidity, uint256 totalDebt, uint256 reserveFactorBps)
        external
        pure
        returns (uint256 utilizationRay, uint256 borrowRateRay, uint256 liquidityRateRay)
    {
        utilizationRay = utilization(availableLiquidity, totalDebt);
        borrowRateRay = borrowRate(utilizationRay);
        liquidityRateRay = liquidityRate(utilizationRay, borrowRateRay, reserveFactorBps);
    }
}
