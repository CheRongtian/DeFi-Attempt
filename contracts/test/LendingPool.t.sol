// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Repeated literals keep each lending scenario independently readable.
// forge-lint: disable-start(literal-instead-of-constant)
// Exact equality is intentional for token balances and internal accounting.
// forge-lint: disable-start(incorrect-strict-equality)

import {LendingPool} from "../src/LendingPool.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface ILendingPoolVm {
    function warp(uint256 timestamp) external;
    function prank(address sender) external;
    function expectRevert(bytes calldata revertData) external;
}

contract LendingPoolTest {
    ILendingPoolVm private constant VM = ILendingPoolVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    uint256 private constant START_TIME = 1_000_000;
    uint256 private constant MAX_PRICE_AGE = 1 hours;
    address private constant ALICE = address(0xA11CE);
    address private constant CHARLIE = address(0xC0FFEE);
    address private constant UNSUPPORTED_ASSET = address(0xCAFE);

    PriceOracle private oracle;
    RiskManager private riskManager;
    LendingPool private pool;
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

        weth.mint(ALICE, 100e18);
        usdc.mint(CHARLIE, 100_000e6);

        VM.prank(ALICE);
        require(weth.approve(address(pool), type(uint256).max), "WETH approval failed");
        VM.prank(CHARLIE);
        require(usdc.approve(address(pool), type(uint256).max), "Charlie USDC approval failed");
        VM.prank(ALICE);
        require(usdc.approve(address(pool), type(uint256).max), "Alice USDC approval failed");
    }

    function testStoresMarketDependencies() public view {
        assert(address(pool.RISK_MANAGER()) == address(riskManager));
        assert(address(pool.WETH()) == address(weth));
        assert(address(pool.USDC()) == address(usdc));
        assert(pool.MINIMUM_USDC_BORROW() == 1e6);
    }

    function testInvalidRiskManagerRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.InvalidAddress.selector));
        new LendingPool(RiskManager(address(0)));
    }

    function testCharlieSuppliesUsdcLiquidity() public {
        _supplyUsdc(50_000e6);

        assert(pool.usdcSupplies(CHARLIE) == 50_000e6);
        assert(pool.availableUsdcLiquidity() == 50_000e6);
        assert(usdc.balanceOf(address(pool)) == 50_000e6);
        assert(usdc.balanceOf(CHARLIE) == 50_000e6);
    }

    function testAliceSuppliesWethCollateral() public {
        _supplyWeth(10e18);

        assert(pool.wethCollateral(ALICE) == 10e18);
        assert(weth.balanceOf(address(pool)) == 10e18);
        assert(weth.balanceOf(ALICE) == 90e18);
        assert(pool.availableUsdcLiquidity() == 0);
    }

    function testSupplyAccumulatesPerUserBalance() public {
        _supplyUsdc(20_000e6);
        _supplyUsdc(30_000e6);
        _supplyWeth(4e18);
        _supplyWeth(6e18);

        assert(pool.usdcSupplies(CHARLIE) == 50_000e6);
        assert(pool.wethCollateral(ALICE) == 10e18);
        assert(pool.availableUsdcLiquidity() == 50_000e6);
    }

    function testZeroSupplyRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.ZeroAmount.selector));
        VM.prank(CHARLIE);
        pool.supply(address(usdc), 0);

        VM.expectRevert(abi.encodeWithSelector(LendingPool.ZeroAmount.selector));
        VM.prank(ALICE);
        pool.supply(address(weth), 0);
    }

    function testUnsupportedSupplyAssetRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.UnsupportedAsset.selector, UNSUPPORTED_ASSET));
        VM.prank(ALICE);
        pool.supply(UNSUPPORTED_ASSET, 1);
    }

    function testSupplyAllowedWithStalePrices() public {
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        _supplyUsdc(1_000e6);
        _supplyWeth(1e18);

        assert(pool.usdcSupplies(CHARLIE) == 1_000e6);
        assert(pool.wethCollateral(ALICE) == 1e18);
    }

    function testAliceBorrowsUsdc() public {
        _openPosition(50_000e6, 10e18, 10_000e6);

        assert(pool.usdcDebt(ALICE) == 10_000e6);
        assert(pool.availableUsdcLiquidity() == 40_000e6);
        assert(usdc.balanceOf(ALICE) == 10_000e6);
        assert(usdc.balanceOf(address(pool)) == 40_000e6);
    }

    function testBorrowAtExactLtvAllowed() public {
        _openPosition(50_000e6, 10e18, 22_500e6);

        assert(pool.usdcDebt(ALICE) == 22_500e6);
        assert(pool.availableUsdcLiquidity() == 27_500e6);
    }

    function testBorrowAboveLtvRejected() public {
        _supplyUsdc(50_000e6);
        _supplyWeth(10e18);

        uint256 requested = 22_500e6 + 1;
        VM.expectRevert(
            abi.encodeWithSelector(LendingPool.BorrowCapacityExceeded.selector, 22_500e18 + 1e12, uint256(22_500e18))
        );
        VM.prank(ALICE);
        pool.borrow(address(usdc), requested);
    }

    function testUsdcSupplyCannotBeUsedAsCollateral() public {
        VM.prank(ALICE);
        usdc.mint(ALICE, 20_000e6);
        VM.prank(ALICE);
        pool.supply(address(usdc), 20_000e6);

        VM.expectRevert(abi.encodeWithSelector(LendingPool.BorrowCapacityExceeded.selector, 1_000e18, uint256(0)));
        VM.prank(ALICE);
        pool.borrow(address(usdc), 1_000e6);
    }

    function testBorrowAboveAvailableLiquidityRejected() public {
        _supplyUsdc(5_000e6);
        _supplyWeth(10e18);

        VM.expectRevert(
            abi.encodeWithSelector(LendingPool.InsufficientLiquidity.selector, uint256(5_000e6), uint256(6_000e6))
        );
        VM.prank(ALICE);
        pool.borrow(address(usdc), 6_000e6);
    }

    function testStalePriceBlocksBorrow() public {
        _supplyUsdc(50_000e6);
        _supplyWeth(10e18);
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        VM.expectRevert(abi.encodeWithSelector(PriceOracle.StalePrice.selector, address(weth)));
        VM.prank(ALICE);
        pool.borrow(address(usdc), 10_000e6);
    }

    function testWethCannotBeBorrowed() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.AssetNotBorrowable.selector, address(weth)));
        VM.prank(ALICE);
        pool.borrow(address(weth), 1e18);
    }

    function testUnsupportedBorrowAssetRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.UnsupportedAsset.selector, UNSUPPORTED_ASSET));
        VM.prank(ALICE);
        pool.borrow(UNSUPPORTED_ASSET, 1e6);
    }

    function testZeroBorrowRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.ZeroAmount.selector));
        VM.prank(ALICE);
        pool.borrow(address(usdc), 0);
    }

    function testBorrowBelowMinimumRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.BorrowBelowMinimum.selector, uint256(1e6 - 1), uint256(1e6)));
        VM.prank(ALICE);
        pool.borrow(address(usdc), 1e6 - 1);
    }

    function testPartialRepay() public {
        _openPosition(50_000e6, 10e18, 10_000e6);

        _repay(4_000e6);

        assert(pool.usdcDebt(ALICE) == 6_000e6);
        assert(pool.availableUsdcLiquidity() == 44_000e6);
        assert(usdc.balanceOf(ALICE) == 6_000e6);
    }

    function testFullRepay() public {
        _openPosition(50_000e6, 10e18, 10_000e6);

        _repay(10_000e6);

        assert(pool.usdcDebt(ALICE) == 0);
        assert(pool.availableUsdcLiquidity() == 50_000e6);
        assert(usdc.balanceOf(ALICE) == 0);
    }

    function testRepayAboveDebtOnlyCollectsOutstandingDebt() public {
        _openPosition(50_000e6, 10e18, 10_000e6);

        _repay(15_000e6);

        assert(pool.usdcDebt(ALICE) == 0);
        assert(pool.availableUsdcLiquidity() == 50_000e6);
        assert(usdc.balanceOf(ALICE) == 0);
    }

    function testRepayAllowedWithStalePrices() public {
        _openPosition(50_000e6, 10e18, 10_000e6);
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        _repay(10_000e6);

        assert(pool.usdcDebt(ALICE) == 0);
        assert(pool.availableUsdcLiquidity() == 50_000e6);
    }

    function testRepayCannotLeaveDustDebt() public {
        _openPosition(10_000e6, 10e18, 1e6);

        VM.expectRevert(
            abi.encodeWithSelector(LendingPool.RemainingDebtBelowMinimum.selector, uint256(1e6 - 1), uint256(1e6))
        );
        VM.prank(ALICE);
        pool.repay(address(usdc), 1);
    }

    function testRepayWithoutDebtRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.NoDebt.selector));
        VM.prank(ALICE);
        pool.repay(address(usdc), 1e6);
    }

    function testWethCannotBeRepaid() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.AssetNotRepayable.selector, address(weth)));
        VM.prank(ALICE);
        pool.repay(address(weth), 1e18);
    }

    function testSafeCollateralWithdrawal() public {
        _openPosition(50_000e6, 10e18, 10_000e6);

        _withdrawWeth(5e18);

        assert(pool.wethCollateral(ALICE) == 5e18);
        assert(weth.balanceOf(ALICE) == 95e18);
        assert(riskManager.healthFactor(pool.wethCollateral(ALICE), pool.usdcDebt(ALICE)) == 1.2e18);
    }

    function testUnsafeCollateralWithdrawalRejected() public {
        _openPosition(50_000e6, 10e18, 10_000e6);

        VM.expectRevert(abi.encodeWithSelector(LendingPool.UnhealthyPosition.selector, uint256(0.96e18)));
        VM.prank(ALICE);
        pool.withdraw(address(weth), 6e18);

        assert(pool.wethCollateral(ALICE) == 10e18);
    }

    function testUsdcSupplierWithdrawal() public {
        _supplyUsdc(50_000e6);

        _withdrawUsdc(20_000e6);

        assert(pool.usdcSupplies(CHARLIE) == 30_000e6);
        assert(pool.availableUsdcLiquidity() == 30_000e6);
        assert(usdc.balanceOf(CHARLIE) == 70_000e6);
        assert(usdc.balanceOf(address(pool)) == 30_000e6);
    }

    function testUsdcWithdrawalAboveAvailableLiquidityRejected() public {
        _openPosition(50_000e6, 10e18, 10_000e6);

        VM.expectRevert(
            abi.encodeWithSelector(LendingPool.InsufficientLiquidity.selector, uint256(40_000e6), uint256(45_000e6))
        );
        VM.prank(CHARLIE);
        pool.withdraw(address(usdc), 45_000e6);
    }

    function testWithdrawalAboveUserSupplyRejected() public {
        _supplyUsdc(10_000e6);

        VM.expectRevert(
            abi.encodeWithSelector(LendingPool.InsufficientSupply.selector, uint256(10_000e6), uint256(11_000e6))
        );
        VM.prank(CHARLIE);
        pool.withdraw(address(usdc), 11_000e6);
    }

    function testStalePriceBlocksCollateralWithdrawalWithoutDebt() public {
        _supplyWeth(10e18);
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        VM.expectRevert(abi.encodeWithSelector(PriceOracle.StalePrice.selector, address(weth)));
        VM.prank(ALICE);
        pool.withdraw(address(weth), 1e18);
    }

    function testUsdcWithdrawalAllowedWithStalePrices() public {
        _supplyUsdc(10_000e6);
        VM.warp(START_TIME + MAX_PRICE_AGE + 1);

        _withdrawUsdc(10_000e6);

        assert(pool.usdcSupplies(CHARLIE) == 0);
        assert(pool.availableUsdcLiquidity() == 0);
    }

    function testZeroWithdrawalRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.ZeroAmount.selector));
        VM.prank(ALICE);
        pool.withdraw(address(weth), 0);
    }

    function testUnsupportedWithdrawalAssetRejected() public {
        VM.expectRevert(abi.encodeWithSelector(LendingPool.UnsupportedAsset.selector, UNSUPPORTED_ASSET));
        VM.prank(ALICE);
        pool.withdraw(UNSUPPORTED_ASSET, 1);
    }

    function _openPosition(uint256 liquidity, uint256 collateral, uint256 debt) private {
        _supplyUsdc(liquidity);
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

    function _repay(uint256 amount) private {
        VM.prank(ALICE);
        pool.repay(address(usdc), amount);
    }

    function _withdrawUsdc(uint256 amount) private {
        VM.prank(CHARLIE);
        pool.withdraw(address(usdc), amount);
    }

    function _withdrawWeth(uint256 amount) private {
        VM.prank(ALICE);
        pool.withdraw(address(weth), amount);
    }
}

// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(multi-contract-file)
