// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Repeated literals keep each liquidation scenario independently readable.
// forge-lint: disable-start(literal-instead-of-constant)
// Exact equality is intentional for token balances and protocol accounting.
// forge-lint: disable-start(incorrect-strict-equality)
// Expected-revert calls intentionally ignore values that can never be returned.
// forge-lint: disable-start(unused-return)
// Emitted events are templates consumed by Foundry's expectEmit cheatcode.
// forge-lint: disable-start(reentrancy-events)

import {IAccessControl} from "@openzeppelin/contracts/access/IAccessControl.sol";
import {IERC20Errors} from "@openzeppelin/contracts/interfaces/draft-IERC6093.sol";
import {LendingPool} from "../src/LendingPool.sol";
import {LiquidationManager} from "../src/LiquidationManager.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface ILiquidationManagerVm {
    function warp(uint256 timestamp) external;
    function prank(address sender) external;
    function expectRevert(bytes calldata revertData) external;
    function expectEmit(bool checkTopic1, bool checkTopic2, bool checkTopic3, bool checkData, address emitter) external;
}

contract LiquidationManagerTest {
    ILiquidationManagerVm private constant VM = ILiquidationManagerVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    uint256 private constant START_TIME = 1_000_000;
    uint256 private constant MAX_PRICE_AGE = 1 hours;
    address private constant ALICE = address(0xA11CE);
    address private constant CHARLIE = address(0xC0FFEE);
    address private constant LIQUIDATOR = address(0x1A11CE);
    address private constant UNAPPROVED_LIQUIDATOR = address(0xBAD1);
    address private constant UNFUNDED_LIQUIDATOR = address(0xBAD2);
    address private constant UNSUPPORTED_ASSET = address(0xCAFE);

    PriceOracle private oracle;
    RiskManager private riskManager;
    LendingPool private pool;
    LiquidationManager private liquidationManager;
    MockWETH private weth;
    MockUSDC private usdc;

    function setUp() public {
        VM.warp(START_TIME);

        oracle = new PriceOracle();
        weth = new MockWETH();
        usdc = new MockUSDC();

        oracle.registerAsset(address(weth), MAX_PRICE_AGE);
        oracle.registerAsset(address(usdc), MAX_PRICE_AGE);
        oracle.setPrice(address(weth), 3_000e8);
        oracle.setPrice(address(usdc), 1e8);

        riskManager = new RiskManager(oracle, address(weth), address(usdc));
        pool = new LendingPool(riskManager);
        liquidationManager = new LiquidationManager(pool);
        pool.grantRole(pool.LIQUIDATION_ROLE(), address(liquidationManager));

        usdc.mint(CHARLIE, 100_000e6);
        usdc.mint(LIQUIDATOR, 100_000e6);
        weth.mint(ALICE, 100e18);

        VM.prank(CHARLIE);
        require(usdc.approve(address(pool), type(uint256).max), "Charlie USDC approval failed");
        VM.prank(ALICE);
        require(weth.approve(address(pool), type(uint256).max), "Alice WETH approval failed");
        VM.prank(LIQUIDATOR);
        require(usdc.approve(address(pool), type(uint256).max), "Liquidator USDC approval failed");
    }

    function testStoresConfiguration() public view {
        assert(address(liquidationManager.LENDING_POOL()) == address(pool));
        assert(address(liquidationManager.RISK_MANAGER()) == address(riskManager));
        assert(liquidationManager.WETH() == address(weth));
        assert(liquidationManager.USDC() == address(usdc));
        assert(liquidationManager.CLOSE_FACTOR_BPS() == 5_000);
        assert(liquidationManager.LIQUIDATION_BONUS_BPS() == 500);
        assert(pool.hasRole(pool.LIQUIDATION_ROLE(), address(liquidationManager)));
    }

    function testInvalidPoolRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LiquidationManager.InvalidAddress.selector));
        new LiquidationManager(LendingPool(address(0)));
    }

    function testHealthyPositionIsNotLiquidatable() public {
        _openPosition(10e18, 10_000e6);

        assert(!liquidationManager.isLiquidatable(ALICE));
        VM.expectRevert(abi.encodeWithSelector(LiquidationManager.PositionIsHealthy.selector, uint256(2.4e18)));
        _liquidateAs(LIQUIDATOR, 5_000e6, 0);
    }

    function testPositionWithoutDebtIsNotLiquidatable() public {
        _supplyWeth(10e18);

        assert(!liquidationManager.isLiquidatable(ALICE));
        VM.expectRevert(abi.encodeWithSelector(LiquidationManager.PositionHasNoDebt.selector, ALICE));
        _liquidateAs(LIQUIDATOR, 1e6, 0);
    }

    function testUnderwaterPositionIsLiquidatable() public {
        _openPosition(10e18, 10_000e6);
        oracle.setPrice(address(weth), 1_000e8);

        assert(liquidationManager.isLiquidatable(ALICE));
    }

    function testPartialLiquidationIncludesFivePercentBonus() public {
        _openPosition(10e18, 20_000e6);
        oracle.setPrice(address(weth), 2_000e8);

        (uint256 repaid, uint256 seized, uint256 recognized) = _liquidateAs(LIQUIDATOR, 5_000e6, 2.625e18);

        assert(repaid == 5_000e6);
        assert(seized == 2.625e18);
        assert(recognized == 0);
        assert(pool.usdcDebt(ALICE) == 15_000e6);
        assert(pool.wethCollateral(ALICE) == 7.375e18);
        assert(pool.availableUsdcLiquidity() == 35_000e6);
        assert(pool.totalPerformingUsdcDebt() == 15_000e6);
        assert(usdc.balanceOf(LIQUIDATOR) == 95_000e6);
        assert(weth.balanceOf(LIQUIDATOR) == 2.625e18);
    }

    function testCloseFactorCapsRepaymentAtFiftyPercent() public {
        _openPosition(10e18, 20_000e6);
        oracle.setPrice(address(weth), 1_500e8);

        (uint256 repaid, uint256 seized, uint256 recognized) = _liquidateAs(LIQUIDATOR, 20_000e6, 7e18);

        assert(repaid == 10_000e6);
        assert(seized == 7e18);
        assert(recognized == 0);
        assert(pool.usdcDebt(ALICE) == 10_000e6);
        assert(pool.wethCollateral(ALICE) == 3e18);
    }

    function testLiquidationCollateralRoundsDown() public {
        _openPosition(1e18, 300e6);
        oracle.setPrice(address(weth), 333e8);

        (uint256 repaid, uint256 seized,) = _liquidateAs(LIQUIDATOR, 1e6, 3_153_153_153_153_153);

        assert(repaid == 1e6);
        assert(seized == 3_153_153_153_153_153);
    }

    function testSmallDebtCanBeFullyLiquidatedWithoutLeavingDust() public {
        _openPosition(1e15, 1e6);
        oracle.setPrice(address(weth), 1_100e8);

        (uint256 repaid, uint256 seized, uint256 recognized) = _liquidateAs(LIQUIDATOR, 1e6, 954_545_454_545_454);

        assert(repaid == 1e6);
        assert(seized == 954_545_454_545_454);
        assert(recognized == 0);
        assert(pool.usdcDebt(ALICE) == 0);
        assert(pool.wethCollateral(ALICE) == 45_454_545_454_546);
    }

    function testRequestedRepayIsReducedRatherThanLeavingDebtDust() public {
        _openPosition(1e15, 1_500_000);
        oracle.setPrice(address(weth), 1_500e8);

        (uint256 repaid, uint256 seized, uint256 recognized) = _liquidateAs(LIQUIDATOR, 750_000, 350_000_000_000_000);

        assert(repaid == 500_000);
        assert(seized == 350_000_000_000_000);
        assert(recognized == 0);
        assert(pool.usdcDebt(ALICE) == 1e6);
        assert(pool.wethCollateral(ALICE) == 650_000_000_000_000);
    }

    function testMinimumCollateralOutProtectsLiquidator() public {
        _openPosition(10e18, 20_000e6);
        oracle.setPrice(address(weth), 2_000e8);

        VM.expectRevert(
            abi.encodeWithSelector(
                LiquidationManager.CollateralOutputBelowMinimum.selector, uint256(2.625e18), uint256(2.625e18 + 1)
            )
        );
        _liquidateAs(LIQUIDATOR, 5_000e6, 2.625e18 + 1);

        assert(pool.usdcDebt(ALICE) == 20_000e6);
        assert(pool.wethCollateral(ALICE) == 10e18);
    }

    function testStalePriceBlocksLiquidation() public {
        _openPosition(10e18, 10_000e6);
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        VM.expectRevert(abi.encodeWithSelector(PriceOracle.StalePrice.selector, address(weth)));
        _liquidateAs(LIQUIDATOR, 5_000e6, 0);
    }

    function testUnsupportedAssetsAndZeroAmountRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LiquidationManager.UnsupportedDebtAsset.selector, UNSUPPORTED_ASSET));
        VM.prank(LIQUIDATOR);
        liquidationManager.liquidate(ALICE, UNSUPPORTED_ASSET, address(weth), 1e6, 0);

        VM.expectRevert(
            abi.encodeWithSelector(LiquidationManager.UnsupportedCollateralAsset.selector, UNSUPPORTED_ASSET)
        );
        VM.prank(LIQUIDATOR);
        liquidationManager.liquidate(ALICE, address(usdc), UNSUPPORTED_ASSET, 1e6, 0);

        VM.expectRevert(abi.encodeWithSelector(LiquidationManager.ZeroAmount.selector));
        VM.prank(LIQUIDATOR);
        liquidationManager.liquidate(ALICE, address(usdc), address(weth), 0, 0);
    }

    function testOnlyAuthorizedManagerCanApplyLiquidation() public {
        VM.expectRevert(
            abi.encodeWithSelector(
                IAccessControl.AccessControlUnauthorizedAccount.selector, address(this), pool.LIQUIDATION_ROLE()
            )
        );
        pool.executeLiquidation(ALICE, LIQUIDATOR, 1e6, 1);
    }

    function testLiquidatorMustApprovePool() public {
        _openPosition(10e18, 20_000e6);
        oracle.setPrice(address(weth), 2_000e8);
        usdc.mint(UNAPPROVED_LIQUIDATOR, 5_000e6);

        VM.expectRevert(
            abi.encodeWithSelector(
                IERC20Errors.ERC20InsufficientAllowance.selector, address(pool), uint256(0), uint256(5_000e6)
            )
        );
        _liquidateAs(UNAPPROVED_LIQUIDATOR, 5_000e6, 0);
    }

    function testLiquidatorMustHoldUsdc() public {
        _openPosition(10e18, 20_000e6);
        oracle.setPrice(address(weth), 2_000e8);
        VM.prank(UNFUNDED_LIQUIDATOR);
        require(usdc.approve(address(pool), type(uint256).max), "Unfunded liquidator approval failed");

        VM.expectRevert(
            abi.encodeWithSelector(
                IERC20Errors.ERC20InsufficientBalance.selector, UNFUNDED_LIQUIDATOR, uint256(0), uint256(5_000e6)
            )
        );
        _liquidateAs(UNFUNDED_LIQUIDATOR, 5_000e6, 0);
    }

    function testExtremeCrashRecognizesBadDebtOnce() public {
        _openPosition(10e18, 10_000e6);
        oracle.setPrice(address(weth), 100e8);

        VM.expectEmit(true, true, true, true, address(pool));
        // Template event required by Foundry's expectEmit assertion.
        // forge-lint: disable-next-line(reentrancy-events)
        emit LendingPool.Liquidated(LIQUIDATOR, ALICE, address(usdc), address(weth), 952_380_952, uint256(10e18));
        VM.expectEmit(true, false, false, true, address(pool));
        // forge-lint: disable-next-line(reentrancy-events)
        emit LendingPool.BadDebtRecognized(ALICE, 9_047_619_048);

        (uint256 repaid, uint256 seized, uint256 recognized) = _liquidateAs(LIQUIDATOR, 10_000e6, 10e18);

        assert(repaid == 952_380_952);
        assert(seized == 10e18);
        assert(recognized == 9_047_619_048);
        assert(pool.usdcDebt(ALICE) == 0);
        assert(pool.wethCollateral(ALICE) == 0);
        assert(pool.totalPerformingUsdcDebt() == 0);
        assert(pool.totalWethCollateral() == 0);
        assert(pool.badDebt() == 9_047_619_048);
        assert(pool.availableUsdcLiquidity() == 40_952_380_952);

        VM.expectRevert(abi.encodeWithSelector(LiquidationManager.PositionHasNoDebt.selector, ALICE));
        _liquidateAs(LIQUIDATOR, 1e6, 0);
        assert(pool.badDebt() == 9_047_619_048);
    }

    function _openPosition(uint256 collateral, uint256 debt) private {
        _supplyUsdc(50_000e6);
        _supplyWeth(collateral);
        VM.prank(ALICE);
        pool.borrow(address(usdc), debt);
    }

    function _supplyUsdc(uint256 amount) private {
        VM.prank(CHARLIE);
        pool.supply(address(usdc), amount);
    }

    function _supplyWeth(uint256 amount) private {
        VM.prank(ALICE);
        pool.supply(address(weth), amount);
    }

    function _liquidateAs(address liquidator, uint256 requestedRepay, uint256 minCollateralOut)
        private
        returns (uint256 repaid, uint256 seized, uint256 recognized)
    {
        VM.prank(liquidator);
        return liquidationManager.liquidate(ALICE, address(usdc), address(weth), requestedRepay, minCollateralOut);
    }
}

// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(unused-return)
// forge-lint: disable-end(reentrancy-events)
// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(multi-contract-file)
