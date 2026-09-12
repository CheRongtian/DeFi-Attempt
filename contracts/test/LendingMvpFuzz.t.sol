// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Exact equality is intentional for token balances and protocol accounting.
// forge-lint: disable-start(incorrect-strict-equality)
// Repeated literals keep each fuzz scenario independently readable.
// forge-lint: disable-start(literal-instead-of-constant)

import {LendingPool} from "../src/LendingPool.sol";
import {LiquidationManager} from "../src/LiquidationManager.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MathLib} from "../src/libraries/MathLib.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface ILendingMvpFuzzVm {
    function prank(address sender) external;
}

contract LendingMvpFuzzTest {
    ILendingMvpFuzzVm private constant VM = ILendingMvpFuzzVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

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
        oracle = new PriceOracle();
        weth = new MockWETH();
        usdc = new MockUSDC();

        oracle.registerAsset(address(weth), 1 days);
        oracle.registerAsset(address(usdc), 1 days);
        oracle.setPrice(address(weth), 3_000e8);
        oracle.setPrice(address(usdc), 1e8);

        riskManager = new RiskManager(oracle, address(weth), address(usdc));
        pool = new LendingPool(riskManager);
        liquidationManager = new LiquidationManager(pool);
        pool.grantRole(pool.LIQUIDATION_ROLE(), address(liquidationManager));

        usdc.mint(CHARLIE, 1_000_000e6);
        usdc.mint(LIQUIDATOR, 1_000_000e6);
        weth.mint(ALICE, 1_000e18);

        VM.prank(CHARLIE);
        require(usdc.approve(address(pool), type(uint256).max), "Charlie USDC approval failed");
        VM.prank(LIQUIDATOR);
        require(usdc.approve(address(pool), type(uint256).max), "Liquidator USDC approval failed");
        VM.prank(ALICE);
        require(weth.approve(address(pool), type(uint256).max), "Alice WETH approval failed");

        VM.prank(CHARLIE);
        pool.supply(address(usdc), 1_000_000e6);
    }

    function testFuzzBorrowWithinLtvAndLiquidity(uint256 collateralSeed, uint256 borrowSeed) public {
        uint256 collateral = _bound(collateralSeed, 1e15, 100e18);

        VM.prank(ALICE);
        pool.supply(address(weth), collateral);

        // With USDC at $1, each native USDC unit is worth 1e12 USD WAD units.
        uint256 maxDebtNative = riskManager.maxBorrow(collateral) / 1e12;
        uint256 borrowAmount = _bound(borrowSeed, pool.MINIMUM_USDC_BORROW(), maxDebtNative);

        VM.prank(ALICE);
        pool.borrow(address(usdc), borrowAmount);

        assert(riskManager.debtValue(pool.usdcDebt(ALICE)) <= riskManager.maxBorrow(collateral));
        assert(riskManager.healthFactor(collateral, pool.usdcDebt(ALICE)) >= MathLib.WAD);
        _assertAccountingIdentity();
    }

    function testFuzzLiquidationRespectsRequestCloseFactorAndCollateral(uint256 requestedSeed) public {
        VM.prank(ALICE);
        pool.supply(address(weth), 10e18);
        VM.prank(ALICE);
        pool.borrow(address(usdc), 20_000e6);
        oracle.setPrice(address(weth), 2_000e8);

        uint256 requestedRepay = _bound(requestedSeed, 1, 20_000e6);
        VM.prank(LIQUIDATOR);
        (uint256 repaidAmount, uint256 collateralSeized, uint256 recognizedBadDebt) =
            liquidationManager.liquidate(ALICE, address(usdc), address(weth), requestedRepay, 0);

        assert(repaidAmount <= requestedRepay);
        assert(repaidAmount <= 10_000e6);
        assert(collateralSeized <= 10e18);
        assert(recognizedBadDebt == 0);
        assert(pool.usdcDebt(ALICE) == 20_000e6 - repaidAmount);
        assert(pool.wethCollateral(ALICE) == 10e18 - collateralSeized);
        _assertAccountingIdentity();
    }

    function _assertAccountingIdentity() private view {
        assert(
            pool.availableUsdcLiquidity() + pool.totalPerformingUsdcDebt() + pool.badDebt() == pool.totalUsdcSupplies()
        );
        assert(usdc.balanceOf(address(pool)) == pool.availableUsdcLiquidity());
        assert(weth.balanceOf(address(pool)) == pool.totalWethCollateral());
    }

    /// @dev Deterministically maps any fuzz seed into the inclusive test range.
    function _bound(uint256 value, uint256 minimum, uint256 maximum) private pure returns (uint256) {
        if (minimum == maximum) return minimum;
        return minimum + value % (maximum - minimum + 1);
    }
}

// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(multi-contract-file)
