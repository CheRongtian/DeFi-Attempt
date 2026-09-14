// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Exact equality is required for deterministic index and accounting checks.
// forge-lint: disable-start(incorrect-strict-equality)
// Fixed setup amounts define the fuzz market boundaries.
// forge-lint: disable-start(literal-instead-of-constant)

import {InterestRateModel} from "../src/InterestRateModel.sol";
import {LendingPool} from "../src/LendingPool.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MathLib} from "../src/libraries/MathLib.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface IInterestFuzzVm {
    function warp(uint256 timestamp) external;
    function prank(address sender) external;
}

contract InterestFuzzTest {
    IInterestFuzzVm private constant VM = IInterestFuzzVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 private constant START_TIME = 1_000_000;
    address private constant ALICE = address(0xA11CE);
    address private constant CHARLIE = address(0xC0FFEE);

    LendingPool private pool;
    InterestRateModel private model;
    MockUSDC private usdc;
    MockWETH private weth;

    function setUp() public {
        VM.warp(START_TIME);
        PriceOracle oracle = new PriceOracle();
        weth = new MockWETH();
        usdc = new MockUSDC();
        oracle.registerAsset(address(weth), 400 days);
        oracle.registerAsset(address(usdc), 400 days);
        oracle.setPrice(address(weth), 3_000e8);
        oracle.setPrice(address(usdc), 1e8);

        RiskManager riskManager = new RiskManager(oracle, address(weth), address(usdc));
        pool = new LendingPool(riskManager);
        model = pool.INTEREST_RATE_MODEL();

        usdc.mint(CHARLIE, 100_000e6);
        usdc.mint(ALICE, 200_000e6);
        weth.mint(ALICE, 100e18);
        VM.prank(CHARLIE);
        require(usdc.approve(address(pool), type(uint256).max), "Supplier approval failed");
        VM.prank(ALICE);
        require(weth.approve(address(pool), type(uint256).max), "Collateral approval failed");
        VM.prank(ALICE);
        require(usdc.approve(address(pool), type(uint256).max), "Borrower approval failed");
        VM.prank(CHARLIE);
        pool.supply(address(usdc), 100_000e6);
        VM.prank(ALICE);
        pool.supply(address(weth), 100e18);
    }

    function testFuzzProjectedIndicesMatchRateSnapshot(uint256 debtSeed, uint256 elapsedSeed) public {
        (uint256 debt, uint256 elapsedSeconds) = _borrowAndWarp(debtSeed, elapsedSeed);

        uint256 utilizationRay = model.utilization(100_000e6 - debt, debt);
        uint256 borrowRateRay = model.borrowRate(utilizationRay);
        uint256 liquidityRateRay = model.liquidityRate(utilizationRay, borrowRateRay, pool.RESERVE_FACTOR_BPS());
        uint256 expectedBorrowIndex =
            MathLib.RAY + MathLib.mulDivDown(borrowRateRay, elapsedSeconds, pool.SECONDS_PER_YEAR());
        uint256 expectedLiquidityIndex =
            MathLib.RAY + MathLib.mulDivDown(liquidityRateRay, elapsedSeconds, pool.SECONDS_PER_YEAR());

        assert(pool.normalizedBorrowIndex() == expectedBorrowIndex);
        assert(pool.normalizedLiquidityIndex() == expectedLiquidityIndex);
    }

    function testFuzzAccrualPreservesAccounting(uint256 debtSeed, uint256 elapsedSeed) public {
        _borrowAndWarp(debtSeed, elapsedSeed);
        pool.accrueInterest();

        assert(pool.borrowIndex() >= MathLib.RAY);
        assert(pool.liquidityIndex() >= MathLib.RAY);
        assert(
            pool.availableUsdcLiquidity() + pool.totalPerformingUsdcDebt() + pool.badDebt()
                == pool.totalUsdcSupplies() + pool.protocolReserve()
        );
    }

    function testFuzzFullRepayClearsScaledDebt(uint256 debtSeed, uint256 elapsedSeed) public {
        _borrowAndWarp(debtSeed, elapsedSeed);

        VM.prank(ALICE);
        pool.repay(address(usdc), type(uint256).max);

        assert(pool.usdcDebt(ALICE) == 0);
        assert(pool.totalScaledDebt() == 0);
    }

    function _borrowAndWarp(uint256 debtSeed, uint256 elapsedSeed)
        private
        returns (uint256 debt, uint256 elapsedSeconds)
    {
        debt = _bound(debtSeed, pool.MINIMUM_USDC_BORROW(), 100_000e6);
        elapsedSeconds = _bound(elapsedSeed, 1, 365 days);

        VM.prank(ALICE);
        pool.borrow(address(usdc), debt);
        VM.warp(START_TIME + elapsedSeconds);
    }

    function _bound(uint256 value, uint256 minimum, uint256 maximum) private pure returns (uint256) {
        if (minimum == maximum) return minimum;
        return minimum + value % (maximum - minimum + 1);
    }
}

// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(multi-contract-file)
