// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the invariant handler alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Exact equality is required for the accounting identities under test.
// forge-lint: disable-start(incorrect-strict-equality)
// Fixed scenario amounts document the bounds of the stateful test environment.
// forge-lint: disable-start(literal-instead-of-constant)

import {LendingPool} from "../src/LendingPool.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MathLib} from "../src/libraries/MathLib.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface ILendingMvpInvariantVm {
    function warp(uint256 timestamp) external;
}

contract LendingMvpHandler {
    ILendingMvpInvariantVm private constant VM = ILendingMvpInvariantVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    LendingPool public immutable POOL;
    RiskManager public immutable RISK_MANAGER;
    MockWETH public immutable WETH;
    MockUSDC public immutable USDC;
    uint256 public lastObservedBorrowIndex = MathLib.RAY;
    uint256 public lastObservedLiquidityIndex = MathLib.RAY;
    bool public indicesNeverDecreased = true;

    constructor(LendingPool pool_, RiskManager riskManager_, MockWETH weth_, MockUSDC usdc_) {
        POOL = pool_;
        RISK_MANAGER = riskManager_;
        WETH = weth_;
        USDC = usdc_;

        require(WETH.approve(address(POOL), type(uint256).max), "WETH approval failed");
        require(USDC.approve(address(POOL), type(uint256).max), "USDC approval failed");
    }

    function supplyUsdc(uint96 seed) external {
        uint256 balance = USDC.balanceOf(address(this));
        uint256 minimum = MathLib.mulDivUp(1, POOL.normalizedLiquidityIndex(), MathLib.RAY);
        if (balance < minimum) return;
        uint256 amount = minimum + uint256(seed) % (balance - minimum + 1);

        POOL.supply(address(USDC), amount);
        _recordIndices();
    }

    function withdrawUsdc(uint96 seed) external {
        uint256 withdrawable = _min(POOL.usdcSupplies(address(this)), POOL.availableUsdcLiquidity());
        uint256 amount = uint256(seed) % (withdrawable + 1);
        if (amount == 0) return;

        POOL.withdraw(address(USDC), amount);
        _recordIndices();
    }

    function supplyWeth(uint96 seed) external {
        uint256 balance = WETH.balanceOf(address(this));
        uint256 amount = uint256(seed) % (balance + 1);
        if (amount == 0) return;

        POOL.supply(address(WETH), amount);
        _recordIndices();
    }

    function withdrawAllWethWithoutDebt() external {
        if (POOL.usdcDebt(address(this)) != 0) return;

        uint256 collateral = POOL.wethCollateral(address(this));
        if (collateral == 0) return;

        POOL.withdraw(address(WETH), collateral);
        _recordIndices();
    }

    function borrowUsdc(uint96 seed) external {
        uint256 collateral = POOL.wethCollateral(address(this));
        if (collateral == 0) return;

        // USDC is fixed at $1, so one native unit equals 1e12 USD WAD units.
        uint256 maximumDebt = RISK_MANAGER.maxBorrow(collateral) / 1e12;
        uint256 currentDebt = POOL.usdcDebt(address(this));
        if (maximumDebt <= currentDebt + 1) return;

        uint256 capacity = _min(maximumDebt - currentDebt - 1, POOL.availableUsdcLiquidity());
        uint256 minimum = POOL.MINIMUM_USDC_BORROW();
        if (capacity < minimum) return;

        uint256 amount = minimum + uint256(seed) % (capacity - minimum + 1);
        POOL.borrow(address(USDC), amount);
        _recordIndices();
    }

    function repayAllUsdc() external {
        uint256 debt = POOL.usdcDebt(address(this));
        if (debt == 0 || USDC.balanceOf(address(this)) < debt) return;

        POOL.repay(address(USDC), debt);
        _recordIndices();
    }

    function advanceTime(uint32 seed) external {
        uint256 elapsedSeconds = 1 + uint256(seed) % 30 days;
        VM.warp(block.timestamp + elapsedSeconds);
        POOL.accrueInterest();
        _recordIndices();
    }

    function _recordIndices() private {
        uint256 currentBorrowIndex = POOL.borrowIndex();
        uint256 currentLiquidityIndex = POOL.liquidityIndex();
        if (currentBorrowIndex < lastObservedBorrowIndex || currentLiquidityIndex < lastObservedLiquidityIndex) {
            indicesNeverDecreased = false;
        }
        lastObservedBorrowIndex = currentBorrowIndex;
        lastObservedLiquidityIndex = currentLiquidityIndex;
    }

    function _min(uint256 a, uint256 b) private pure returns (uint256) {
        return a < b ? a : b;
    }
}

contract LendingMvpInvariantTest {
    LendingPool private pool;
    LendingMvpHandler private handler;
    MockWETH private weth;
    MockUSDC private usdc;
    address[] private invariantTargets;

    function setUp() public {
        PriceOracle oracle = new PriceOracle();
        weth = new MockWETH();
        usdc = new MockUSDC();

        oracle.registerAsset(address(weth), 100 * 365 days);
        oracle.registerAsset(address(usdc), 100 * 365 days);
        oracle.setPrice(address(weth), 3_000e8);
        oracle.setPrice(address(usdc), 1e8);

        RiskManager riskManager = new RiskManager(oracle, address(weth), address(usdc));
        pool = new LendingPool(riskManager);
        handler = new LendingMvpHandler(pool, riskManager, weth, usdc);

        usdc.mint(address(handler), 1_000_000e6);
        weth.mint(address(handler), 1_000e18);

        invariantTargets.push(address(handler));
    }

    /// @notice Exposes the contracts Forge should call during stateful invariant runs.
    function targetContracts() external view returns (address[] memory) {
        return invariantTargets;
    }

    function testRegressionCapsSupplierInterestAtRoundingBoundary() public {
        handler.supplyUsdc(457_115_303);
        handler.supplyWeth(type(uint96).max);
        handler.borrowUsdc(3);
        handler.advanceTime(2_420);
        handler.withdrawUsdc(2_400);
        handler.withdrawUsdc(2_210_815_638);
        handler.withdrawUsdc(3_912);
        handler.supplyUsdc(443);
        handler.withdrawUsdc(2_174);
        handler.withdrawUsdc(97_043_819_060_657_669_655_084);
        handler.supplyUsdc(type(uint96).max);
        handler.withdrawUsdc(685_023_565_091);
        handler.withdrawUsdc(117_300_737);
        handler.repayAllUsdc();
        handler.withdrawAllWethWithoutDebt();
        handler.supplyUsdc(571_058_181_132);
        handler.supplyWeth(30_774_322_921_180_742);
        handler.borrowUsdc(346_147_262_542_509_919_389);
        handler.withdrawUsdc(2_796_547_478);
        handler.advanceTime(4);

        assert(
            pool.availableUsdcLiquidity() + pool.totalPerformingUsdcDebt() + pool.badDebt()
                == pool.totalUsdcSupplies() + pool.protocolReserve()
        );
    }

    function invariant_usdcAccountingIdentityHolds() public view {
        assert(
            pool.availableUsdcLiquidity() + pool.totalPerformingUsdcDebt() + pool.badDebt()
                == pool.totalUsdcSupplies() + pool.protocolReserve()
        );
    }

    function invariant_poolBalancesMatchInternalAccounting() public view {
        assert(usdc.balanceOf(address(pool)) == pool.availableUsdcLiquidity());
        assert(weth.balanceOf(address(pool)) == pool.totalWethCollateral());
    }

    function invariant_singleActorTotalsMatchPositions() public view {
        assert(pool.totalPerformingUsdcDebt() == pool.usdcDebt(address(handler)));
        assert(pool.totalWethCollateral() == pool.wethCollateral(address(handler)));
        assert(pool.totalUsdcSupplies() == pool.usdcSupplies(address(handler)));
        assert(pool.totalScaledDebt() == pool.DEBT_TOKEN().scaledBalanceOf(address(handler)));
        assert(pool.totalScaledSupply() == pool.DEPOSIT_TOKEN().scaledBalanceOf(address(handler)));
    }

    function invariant_interestIndicesNeverDecreaseBelowRay() public view {
        assert(handler.indicesNeverDecreased());
        assert(pool.borrowIndex() >= MathLib.RAY);
        assert(pool.liquidityIndex() >= MathLib.RAY);
    }
}

// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(multi-contract-file)
