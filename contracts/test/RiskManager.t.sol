// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Repeated literals keep the financial examples explicit and independently readable.
// forge-lint: disable-start(literal-instead-of-constant)

import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface IRiskManagerVm {
    function warp(uint256 timestamp) external;
    function expectRevert(bytes calldata revertData) external;
}

contract RiskManagerTest {
    IRiskManagerVm private constant VM = IRiskManagerVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    uint256 private constant START_TIME = 1_000_000;
    uint256 private constant MAX_PRICE_AGE = 1 hours;
    uint256 private constant WETH_PRICE = 3_000e8;
    uint256 private constant USDC_PRICE = 1e8;

    PriceOracle private oracle;
    RiskManager private riskManager;
    MockWETH private weth;
    MockUSDC private usdc;

    function setUp() public {
        VM.warp(START_TIME);

        oracle = new PriceOracle();
        weth = new MockWETH();
        usdc = new MockUSDC();

        oracle.registerAsset(address(weth), MAX_PRICE_AGE);
        oracle.registerAsset(address(usdc), MAX_PRICE_AGE);
        oracle.setPrice(address(weth), WETH_PRICE);
        oracle.setPrice(address(usdc), USDC_PRICE);

        riskManager = new RiskManager(oracle, address(weth), address(usdc));
    }

    function testStoresMarketDependencies() public view {
        assert(address(riskManager.PRICE_ORACLE()) == address(oracle));
        assert(riskManager.WETH() == address(weth));
        assert(riskManager.USDC() == address(usdc));
    }

    function testInvalidConstructorAddressRejected() public {
        VM.expectRevert(abi.encodeWithSelector(RiskManager.InvalidAddress.selector));
        new RiskManager(PriceOracle(address(0)), address(weth), address(usdc));

        VM.expectRevert(abi.encodeWithSelector(RiskManager.InvalidAddress.selector));
        new RiskManager(oracle, address(0), address(usdc));

        VM.expectRevert(abi.encodeWithSelector(RiskManager.InvalidAddress.selector));
        new RiskManager(oracle, address(weth), address(0));
    }

    function testTenWethCollateralIsThirtyThousandDollars() public view {
        assert(riskManager.collateralValue(10e18) == 30_000e18);
    }

    function testTenThousandUsdcDebtIsTenThousandDollars() public view {
        assert(riskManager.debtValue(10_000e6) == 10_000e18);
    }

    function testZeroAmountsHaveZeroValueWithFreshPrices() public view {
        assert(riskManager.collateralValue(0) == 0);
        assert(riskManager.debtValue(0) == 0);
    }

    function testCollateralValueRoundsDown() public {
        oracle.setPrice(address(weth), 300_000_000_001);

        assert(riskManager.collateralValue(1) == 3_000);
    }

    function testUsdcDebtPreservesSixAndEightDecimalPrecision() public {
        oracle.setPrice(address(usdc), 100_012_345);

        assert(riskManager.debtValue(1_234_567) == 1_234_719_407_296_150_000);
        assert(riskManager.debtValue(1) == 1_000_123_450_000);
    }

    function testUpdatedPricesAreUsed() public {
        oracle.setPrice(address(weth), 2_500e8);
        oracle.setPrice(address(usdc), 101_000_000);

        assert(riskManager.collateralValue(2e18) == 5_000e18);
        assert(riskManager.debtValue(100e6) == 101e18);
    }

    function testStaleWethPriceRejectsCollateralValue() public {
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        VM.expectRevert(abi.encodeWithSelector(PriceOracle.StalePrice.selector, address(weth)));
        // The return value is intentionally unused because this call must revert.
        // forge-lint: disable-next-line(unused-return)
        riskManager.collateralValue(10e18);
    }

    function testStaleUsdcPriceRejectsDebtValue() public {
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        VM.expectRevert(abi.encodeWithSelector(PriceOracle.StalePrice.selector, address(usdc)));
        // The return value is intentionally unused because this call must revert.
        // forge-lint: disable-next-line(unused-return)
        riskManager.debtValue(10_000e6);
    }

    function testRiskParameters() public view {
        assert(riskManager.WETH_LTV_BPS() == 7_500);
        assert(riskManager.WETH_LIQUIDATION_THRESHOLD_BPS() == 8_000);
    }

    function testMaximumBorrowUsesSeventyFivePercentLtv() public view {
        assert(riskManager.maxBorrow(10e18) == 22_500e18);
    }

    function testMaximumBorrowRoundsDown() public {
        oracle.setPrice(address(weth), 100_000_001);

        assert(riskManager.collateralValue(1) == 1);
        assert(riskManager.maxBorrow(1) == 0);
    }

    function testHealthFactorWithZeroDebtReturnsMaximum() public view {
        assert(riskManager.healthFactor(0, 0) == type(uint256).max);
        assert(riskManager.healthFactor(10e18, 0) == type(uint256).max);
    }

    function testZeroDebtDoesNotRequireFreshPrices() public {
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        assert(riskManager.healthFactor(10e18, 0) == type(uint256).max);
    }

    function testHealthyPositionHealthFactor() public view {
        assert(riskManager.healthFactor(10e18, 10_000e6) == 2.4e18);
    }

    function testHealthFactorAtExactLiquidationThreshold() public view {
        assert(riskManager.healthFactor(10e18, 24_000e6) == 1e18);
    }

    function testHealthFactorBelowLiquidationThreshold() public view {
        assert(riskManager.healthFactor(10e18, 24_000e6 + 1) < 1e18);
    }

    function testDebtWithoutCollateralHasZeroHealthFactor() public view {
        assert(riskManager.healthFactor(0, 1e6) == 0);
    }

    function testPriceChangesAffectHealthFactor() public {
        assert(riskManager.healthFactor(10e18, 10_000e6) == 2.4e18);

        oracle.setPrice(address(weth), 1_000e8);

        assert(riskManager.healthFactor(10e18, 10_000e6) == 0.8e18);
    }
}

// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(multi-contract-file)
