// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Exact equality is required for deterministic fixed-point rate calculations.
// forge-lint: disable-start(incorrect-strict-equality)
// Repeated percentages make the rate curve examples directly readable.
// forge-lint: disable-start(literal-instead-of-constant)
// Expected-revert calls cannot return their calculated rates.
// forge-lint: disable-start(unused-return)

import {InterestRateModel} from "../src/InterestRateModel.sol";
import {MathLib} from "../src/libraries/MathLib.sol";

interface IInterestRateModelVm {
    function expectRevert(bytes calldata revertData) external;
}

contract InterestRateModelTest {
    IInterestRateModelVm private constant VM = IInterestRateModelVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    InterestRateModel private model;

    function setUp() public {
        model = new InterestRateModel();
    }

    function testStoresRateParameters() public view {
        assert(model.OPTIMAL_UTILIZATION() == 80e25);
        assert(model.BASE_RATE() == 2e25);
        assert(model.SLOPE_1() == 8e25);
        assert(model.SLOPE_2() == MathLib.RAY);
    }

    function testZeroDebtHasZeroUtilizationAndBaseBorrowRate() public view {
        uint256 utilizationRay = model.utilization(100e6, 0);

        assert(utilizationRay == 0);
        assert(model.borrowRate(utilizationRay) == 2e25);
    }

    function testLowUtilizationUsesFirstSlope() public view {
        uint256 utilizationRay = model.utilization(90e6, 10e6);

        assert(utilizationRay == 10e25);
        assert(model.borrowRate(utilizationRay) == 3e25);
    }

    function testOptimalUtilizationUsesFullFirstSlope() public view {
        uint256 utilizationRay = model.utilization(20e6, 80e6);

        assert(utilizationRay == 80e25);
        assert(model.borrowRate(utilizationRay) == 10e25);
    }

    function testAboveOptimalUtilizationUsesSecondSlope() public view {
        uint256 utilizationRay = model.utilization(10e6, 90e6);

        assert(utilizationRay == 90e25);
        assert(model.borrowRate(utilizationRay) == 60e25);
    }

    function testNearFullUtilizationUsesSteepSecondSlope() public view {
        uint256 utilizationRay = model.utilization(1, 999);

        assert(utilizationRay == 999e24);
        assert(model.borrowRate(utilizationRay) == 1095e24);
    }

    function testFullUtilizationReachesMaximumBorrowRate() public view {
        assert(model.utilization(0, 100e6) == MathLib.RAY);
        assert(model.borrowRate(MathLib.RAY) == 110e25);
    }

    function testLiquidityRateAppliesUtilizationAndReserveFactor() public view {
        uint256 liquidityRateRay = model.liquidityRate(80e25, 10e25, 1_000);

        assert(liquidityRateRay == 72e24);
    }

    function testMarketRatesReturnsOneSnapshot() public view {
        (uint256 utilizationRay, uint256 borrowRateRay, uint256 liquidityRateRay) = model.marketRates(20e6, 80e6, 1_000);

        assert(utilizationRay == 80e25);
        assert(borrowRateRay == 10e25);
        assert(liquidityRateRay == 72e24);
    }

    function testInvalidUtilizationRejected() public {
        VM.expectRevert(abi.encodeWithSelector(InterestRateModel.InvalidUtilization.selector, MathLib.RAY + 1));
        model.borrowRate(MathLib.RAY + 1);

        VM.expectRevert(abi.encodeWithSelector(InterestRateModel.InvalidUtilization.selector, MathLib.RAY + 1));
        model.liquidityRate(MathLib.RAY + 1, 1, 1_000);
    }

    function testInvalidReserveFactorRejected() public {
        VM.expectRevert(abi.encodeWithSelector(InterestRateModel.InvalidReserveFactor.selector, uint256(10_001)));
        model.liquidityRate(80e25, 10e25, 10_001);
    }
}

// forge-lint: disable-end(unused-return)
// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(multi-contract-file)
