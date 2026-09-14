// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Exact equality is intentional for deterministic index and accounting checks.
// forge-lint: disable-start(incorrect-strict-equality)
// Repeated scenario amounts keep the financial examples independently readable.
// forge-lint: disable-start(literal-instead-of-constant)

import {LendingPool} from "../src/LendingPool.sol";
import {LiquidationManager} from "../src/LiquidationManager.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MathLib} from "../src/libraries/MathLib.sol";
import {DebtToken} from "../src/tokens/DebtToken.sol";
import {DepositToken} from "../src/tokens/DepositToken.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface IInterestAccountingVm {
    function warp(uint256 timestamp) external;
    function prank(address sender) external;
}

contract InterestAccountingTest {
    IInterestAccountingVm private constant VM = IInterestAccountingVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    uint256 private constant START_TIME = 1_000_000;
    address private constant ALICE = address(0xA11CE);
    address private constant CHARLIE = address(0xC0FFEE);
    address private constant LIQUIDATOR = address(0x1A11CE);

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
        oracle.registerAsset(address(weth), 400 days);
        oracle.registerAsset(address(usdc), 400 days);
        oracle.setPrice(address(weth), 3_000e8);
        oracle.setPrice(address(usdc), 1e8);

        riskManager = new RiskManager(oracle, address(weth), address(usdc));
        pool = new LendingPool(riskManager);
        liquidationManager = new LiquidationManager(pool);
        pool.grantRole(pool.LIQUIDATION_ROLE(), address(liquidationManager));

        usdc.mint(CHARLIE, 200_000e6);
        usdc.mint(ALICE, 100_000e6);
        usdc.mint(LIQUIDATOR, 100_000e6);
        weth.mint(ALICE, 100e18);

        VM.prank(CHARLIE);
        require(usdc.approve(address(pool), type(uint256).max), "Charlie approval failed");
        VM.prank(ALICE);
        require(usdc.approve(address(pool), type(uint256).max), "Alice USDC approval failed");
        VM.prank(ALICE);
        require(weth.approve(address(pool), type(uint256).max), "Alice WETH approval failed");
        VM.prank(LIQUIDATOR);
        require(usdc.approve(address(pool), type(uint256).max), "Liquidator approval failed");
    }

    function testInitializesIndicesAndPositionTokens() public view {
        assert(pool.borrowIndex() == MathLib.RAY);
        assert(pool.liquidityIndex() == MathLib.RAY);
        assert(pool.lastUpdateTimestamp() == START_TIME);
        assert(pool.RESERVE_FACTOR_BPS() == 1_000);
        assert(pool.reserveFactorBps() == 1_000);

        DepositToken depositToken = pool.DEPOSIT_TOKEN();
        DebtToken debtToken = pool.DEBT_TOKEN();
        assert(depositToken.POOL() == address(pool));
        assert(debtToken.POOL() == address(pool));
    }

    function testSupplyAndBorrowMintScaledPositions() public {
        _openEightyPercentMarket();

        assert(pool.totalScaledSupply() == 100_000e6);
        assert(pool.totalScaledDebt() == 80_000e6);
        assert(pool.DEPOSIT_TOKEN().scaledBalanceOf(CHARLIE) == 100_000e6);
        assert(pool.DEBT_TOKEN().scaledBalanceOf(ALICE) == 80_000e6);
    }

    function testProjectedIndicesAfterOneSecond() public {
        _assertProjectedIndicesAfter(1 seconds);
    }

    function testProjectedIndicesAfterOneDay() public {
        _assertProjectedIndicesAfter(1 days);
    }

    function testProjectedIndicesAfterThirtyDays() public {
        _assertProjectedIndicesAfter(30 days);
    }

    function testProjectedIndicesAfterOneYear() public {
        _openEightyPercentMarket();
        VM.warp(START_TIME + 365 days);

        assert(pool.borrowIndex() == MathLib.RAY);
        assert(pool.liquidityIndex() == MathLib.RAY);
        assert(pool.normalizedBorrowIndex() == 11e26);
        assert(pool.normalizedLiquidityIndex() == 1072e24);
        assert(pool.usdcDebt(ALICE) == 88_000e6);
        assert(pool.usdcSupplies(CHARLIE) == 107_200e6);
    }

    function testAccrualStoresIndicesAndReserve() public {
        _openEightyPercentMarket();
        VM.warp(START_TIME + 365 days);

        pool.accrueInterest();

        assert(pool.borrowIndex() == 11e26);
        assert(pool.liquidityIndex() == 1072e24);
        assert(pool.lastUpdateTimestamp() == START_TIME + 365 days);
        assert(pool.totalPerformingUsdcDebt() == 88_000e6);
        assert(pool.totalUsdcSupplies() == 107_200e6);
        assert(pool.protocolReserve() == 800e6);
        _assertAccountingIdentity();
    }

    function testSameTimestampDoesNotAccrueTwice() public {
        _openEightyPercentMarket();
        VM.warp(START_TIME + 365 days);
        pool.accrueInterest();

        uint256 storedBorrowIndex = pool.borrowIndex();
        uint256 storedLiquidityIndex = pool.liquidityIndex();
        uint256 storedReserve = pool.protocolReserve();
        pool.accrueInterest();

        assert(pool.borrowIndex() == storedBorrowIndex);
        assert(pool.liquidityIndex() == storedLiquidityIndex);
        assert(pool.protocolReserve() == storedReserve);
    }

    function testSupplyAccruesPreviousMarketBeforeChangingLiquidity() public {
        _openEightyPercentMarket();
        VM.warp(START_TIME + 365 days);

        VM.prank(CHARLIE);
        pool.supply(address(usdc), 10_000e6);

        assert(pool.borrowIndex() == 11e26);
        assert(pool.liquidityIndex() == 1072e24);
        assert(pool.totalPerformingUsdcDebt() == 88_000e6);
        assert(pool.availableUsdcLiquidity() == 30_000e6);
    }

    function testFullRepayClearsScaledDebtAfterInterest() public {
        _openEightyPercentMarket();
        VM.warp(START_TIME + 365 days);

        VM.prank(ALICE);
        pool.repay(address(usdc), type(uint256).max);

        assert(pool.usdcDebt(ALICE) == 0);
        assert(pool.totalScaledDebt() == 0);
        assert(pool.availableUsdcLiquidity() == 108_000e6);
        assert(pool.protocolReserve() == 800e6);
        _assertAccountingIdentity();
    }

    function testSupplierCanWithdrawPrincipalAndAccruedIncome() public {
        _openEightyPercentMarket();
        VM.warp(START_TIME + 365 days);

        VM.prank(ALICE);
        pool.repay(address(usdc), type(uint256).max);
        VM.prank(CHARLIE);
        pool.withdraw(address(usdc), 107_200e6);

        assert(pool.usdcSupplies(CHARLIE) == 0);
        assert(pool.totalScaledSupply() == 0);
        assert(pool.availableUsdcLiquidity() == 800e6);
        assert(pool.protocolReserve() == 800e6);
        _assertAccountingIdentity();
    }

    function testLiquidationBurnsScaledDebtAfterInterest() public {
        _openEightyPercentMarket();
        VM.warp(START_TIME + 365 days);
        oracle.setPrice(address(weth), 500e8);
        oracle.setPrice(address(usdc), 1e8);

        VM.prank(LIQUIDATOR);
        (uint256 repaid, uint256 seized, uint256 recognized) =
            liquidationManager.liquidate(ALICE, address(usdc), address(weth), 44_000e6, 92.4e18);

        assert(repaid == 44_000e6);
        assert(seized == 92.4e18);
        assert(recognized == 0);
        assert(pool.totalScaledDebt() == 40_000e6);
        assert(pool.usdcDebt(ALICE) == 44_000e6);
        assert(pool.wethCollateral(ALICE) == 7.6e18);
        assert(pool.protocolReserve() == 800e6);
        _assertAccountingIdentity();
    }

    function testBadDebtDoesNotAccrueInterest() public {
        _openEightyPercentMarket();
        oracle.setPrice(address(weth), 100e8);

        VM.prank(LIQUIDATOR);
        // The resulting bad debt is read from the pool after the liquidation.
        // forge-lint: disable-next-line(unused-return)
        liquidationManager.liquidate(ALICE, address(usdc), address(weth), 80_000e6, 100e18);

        uint256 recognizedBadDebt = pool.badDebt();
        uint256 storedBorrowIndex = pool.borrowIndex();
        VM.warp(START_TIME + 365 days);
        pool.accrueInterest();

        assert(pool.totalScaledDebt() == 0);
        assert(pool.totalPerformingUsdcDebt() == 0);
        assert(pool.badDebt() == recognizedBadDebt);
        assert(pool.borrowIndex() == storedBorrowIndex);
    }

    function _openEightyPercentMarket() private {
        VM.prank(CHARLIE);
        pool.supply(address(usdc), 100_000e6);
        VM.prank(ALICE);
        pool.supply(address(weth), 100e18);
        VM.prank(ALICE);
        pool.borrow(address(usdc), 80_000e6);
    }

    function _assertProjectedIndicesAfter(uint256 elapsedSeconds) private {
        _openEightyPercentMarket();
        VM.warp(START_TIME + elapsedSeconds);

        uint256 expectedBorrowIndex = MathLib.RAY + MathLib.mulDivDown(10e25, elapsedSeconds, pool.SECONDS_PER_YEAR());
        uint256 expectedLiquidityIndex =
            MathLib.RAY + MathLib.mulDivDown(72e24, elapsedSeconds, pool.SECONDS_PER_YEAR());

        assert(pool.normalizedBorrowIndex() == expectedBorrowIndex);
        assert(pool.normalizedLiquidityIndex() == expectedLiquidityIndex);
    }

    function _assertAccountingIdentity() private view {
        assert(
            pool.availableUsdcLiquidity() + pool.totalPerformingUsdcDebt() + pool.badDebt()
                == pool.totalUsdcSupplies() + pool.protocolReserve()
        );
    }
}

// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(multi-contract-file)
