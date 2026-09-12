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
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

contract LendingMvpHandler {
    LendingPool public immutable POOL;
    RiskManager public immutable RISK_MANAGER;
    MockWETH public immutable WETH;
    MockUSDC public immutable USDC;

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
        uint256 amount = uint256(seed) % (balance + 1);
        if (amount == 0) return;

        POOL.supply(address(USDC), amount);
    }

    function withdrawUsdc(uint96 seed) external {
        uint256 withdrawable = _min(POOL.usdcSupplies(address(this)), POOL.availableUsdcLiquidity());
        uint256 amount = uint256(seed) % (withdrawable + 1);
        if (amount == 0) return;

        POOL.withdraw(address(USDC), amount);
    }

    function supplyWeth(uint96 seed) external {
        uint256 balance = WETH.balanceOf(address(this));
        uint256 amount = uint256(seed) % (balance + 1);
        if (amount == 0) return;

        POOL.supply(address(WETH), amount);
    }

    function withdrawAllWethWithoutDebt() external {
        if (POOL.usdcDebt(address(this)) != 0) return;

        uint256 collateral = POOL.wethCollateral(address(this));
        if (collateral == 0) return;

        POOL.withdraw(address(WETH), collateral);
    }

    function borrowUsdc(uint96 seed) external {
        uint256 collateral = POOL.wethCollateral(address(this));
        if (collateral == 0) return;

        // USDC is fixed at $1, so one native unit equals 1e12 USD WAD units.
        uint256 maximumDebt = RISK_MANAGER.maxBorrow(collateral) / 1e12;
        uint256 currentDebt = POOL.usdcDebt(address(this));
        if (maximumDebt <= currentDebt) return;

        uint256 capacity = _min(maximumDebt - currentDebt, POOL.availableUsdcLiquidity());
        uint256 minimum = POOL.MINIMUM_USDC_BORROW();
        if (capacity < minimum) return;

        uint256 amount = minimum + uint256(seed) % (capacity - minimum + 1);
        POOL.borrow(address(USDC), amount);
    }

    function repayAllUsdc() external {
        uint256 debt = POOL.usdcDebt(address(this));
        if (debt == 0 || USDC.balanceOf(address(this)) < debt) return;

        POOL.repay(address(USDC), debt);
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

        oracle.registerAsset(address(weth), 1 days);
        oracle.registerAsset(address(usdc), 1 days);
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

    function invariant_usdcAccountingIdentityHolds() public view {
        assert(
            pool.availableUsdcLiquidity() + pool.totalPerformingUsdcDebt() + pool.badDebt() == pool.totalUsdcSupplies()
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
    }
}

// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(multi-contract-file)
