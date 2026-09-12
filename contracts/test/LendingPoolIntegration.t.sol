// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal test-only cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Exact equality is intentional for the end-to-end accounting checks.
// forge-lint: disable-start(incorrect-strict-equality)
// Repeated scenario amounts document the complete lifecycle directly.
// forge-lint: disable-start(literal-instead-of-constant)

import {LendingPool} from "../src/LendingPool.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface ILendingPoolIntegrationVm {
    function prank(address sender) external;
}

contract LendingPoolIntegrationTest {
    ILendingPoolIntegrationVm private constant VM =
        ILendingPoolIntegrationVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    address private constant ALICE = address(0xA11CE);
    address private constant CHARLIE = address(0xC0FFEE);

    function testCompleteSupplyBorrowRepayWithdrawLifecycle() public {
        MockWETH weth = new MockWETH();
        MockUSDC usdc = new MockUSDC();
        PriceOracle oracle = new PriceOracle();

        oracle.registerAsset(address(weth), 1 hours);
        oracle.registerAsset(address(usdc), 1 hours);
        oracle.setPrice(address(weth), 3_000e8);
        oracle.setPrice(address(usdc), 1e8);

        RiskManager riskManager = new RiskManager(oracle, address(weth), address(usdc));
        LendingPool pool = new LendingPool(riskManager);

        usdc.mint(CHARLIE, 50_000e6);
        weth.mint(ALICE, 10e18);

        VM.prank(CHARLIE);
        require(usdc.approve(address(pool), 50_000e6), "Charlie USDC approval failed");
        VM.prank(ALICE);
        require(weth.approve(address(pool), 10e18), "Alice WETH approval failed");
        VM.prank(ALICE);
        require(usdc.approve(address(pool), 10_000e6), "Alice USDC approval failed");

        VM.prank(CHARLIE);
        pool.supply(address(usdc), 50_000e6);

        VM.prank(ALICE);
        pool.supply(address(weth), 10e18);

        VM.prank(ALICE);
        pool.borrow(address(usdc), 10_000e6);

        VM.prank(ALICE);
        pool.repay(address(usdc), 10_000e6);

        VM.prank(ALICE);
        pool.withdraw(address(weth), 10e18);

        assert(pool.usdcSupplies(CHARLIE) == 50_000e6);
        assert(pool.availableUsdcLiquidity() == 50_000e6);
        assert(pool.usdcDebt(ALICE) == 0);
        assert(pool.wethCollateral(ALICE) == 0);
        assert(usdc.balanceOf(address(pool)) == 50_000e6);
        assert(weth.balanceOf(address(pool)) == 0);
        assert(usdc.balanceOf(ALICE) == 0);
        assert(weth.balanceOf(ALICE) == 10e18);
    }
}

// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(multi-contract-file)
