// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal JSON cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Golden vectors intentionally deploy isolated markets and call contracts inside a loop.
// forge-lint: disable-start(calls-loop)
// A failing vector must stop its loop and report the case name.
// forge-lint: disable-start(require-revert-in-loop)
// Fixed fixture amounts keep each vector focused on liquidation outputs.
// forge-lint: disable-start(literal-instead-of-constant)

import {LendingPool} from "../src/LendingPool.sol";
import {LiquidationManager} from "../src/LiquidationManager.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface ILiquidationGoldenVm {
    function projectRoot() external view returns (string memory);
    function readFile(string calldata path) external view returns (string memory);
    function parseJsonKeys(string calldata json, string calldata key) external pure returns (string[] memory);
    function parseJsonUint(string calldata json, string calldata key) external pure returns (uint256);
    function prank(address sender) external;
}

contract LiquidationGoldenTest {
    ILiquidationGoldenVm private constant VM = ILiquidationGoldenVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    address private constant ALICE = address(0xA11CE);
    address private constant CHARLIE = address(0xC0FFEE);
    address private constant LIQUIDATOR = address(0x1A11CE);

    string private vectorsJson;

    function setUp() public {
        // forge-lint: disable-next-line(unsafe-cheatcode)
        vectorsJson = VM.readFile(string.concat(VM.projectRoot(), "/../tests/golden/risk_vectors.json"));
    }

    function testGoldenLiquidations() public {
        string[] memory names = VM.parseJsonKeys(vectorsJson, ".liquidation");
        require(names.length > 0, "No liquidation vectors");

        for (uint256 i = 0; i < names.length; ++i) {
            _runVector(names[i]);
        }
    }

    function _runVector(string memory name) private {
        string memory path = string.concat(".liquidation.", name);
        (LendingPool pool, LiquidationManager manager, MockWETH weth, MockUSDC usdc) = _deployPosition(
            _uint(string.concat(path, ".collateral_amount")),
            _uint(string.concat(path, ".debt_amount")),
            _uint(string.concat(path, ".liquidation_weth_price"))
        );

        VM.prank(LIQUIDATOR);
        (uint256 repaid, uint256 seized, uint256 recognized) =
            manager.liquidate(ALICE, address(usdc), address(weth), _uint(string.concat(path, ".requested_repay")), 0);

        _assertEq(repaid, _uint(string.concat(path, ".expected_repaid")), string.concat(name, ": repaid"));
        _assertEq(
            seized,
            _uint(string.concat(path, ".expected_collateral_seized")),
            string.concat(name, ": collateral seized")
        );
        _assertEq(
            pool.usdcDebt(ALICE),
            _uint(string.concat(path, ".expected_remaining_debt")),
            string.concat(name, ": remaining debt")
        );
        _assertEq(
            pool.wethCollateral(ALICE),
            _uint(string.concat(path, ".expected_remaining_collateral")),
            string.concat(name, ": remaining collateral")
        );
        _assertEq(recognized, _uint(string.concat(path, ".expected_bad_debt")), string.concat(name, ": recognized"));
        _assertEq(pool.badDebt(), _uint(string.concat(path, ".expected_bad_debt")), string.concat(name, ": bad debt"));
    }

    function _deployPosition(uint256 collateral, uint256 debt, uint256 liquidationWethPrice)
        private
        returns (LendingPool pool, LiquidationManager manager, MockWETH weth, MockUSDC usdc)
    {
        PriceOracle oracle = new PriceOracle();
        weth = new MockWETH();
        usdc = new MockUSDC();
        oracle.registerAsset(address(weth), 1 days);
        oracle.registerAsset(address(usdc), 1 days);
        oracle.setPrice(address(weth), 3_000e8);
        oracle.setPrice(address(usdc), 1e8);

        RiskManager riskManager = new RiskManager(oracle, address(weth), address(usdc));
        pool = new LendingPool(riskManager);
        manager = new LiquidationManager(pool);
        pool.grantRole(pool.LIQUIDATION_ROLE(), address(manager));

        usdc.mint(CHARLIE, 50_000e6);
        usdc.mint(LIQUIDATOR, 50_000e6);
        weth.mint(ALICE, collateral);
        VM.prank(CHARLIE);
        require(usdc.approve(address(pool), type(uint256).max), "Supplier approval failed");
        VM.prank(LIQUIDATOR);
        require(usdc.approve(address(pool), type(uint256).max), "Liquidator approval failed");
        VM.prank(ALICE);
        require(weth.approve(address(pool), type(uint256).max), "Collateral approval failed");

        VM.prank(CHARLIE);
        pool.supply(address(usdc), 50_000e6);
        VM.prank(ALICE);
        pool.supply(address(weth), collateral);
        VM.prank(ALICE);
        pool.borrow(address(usdc), debt);
        oracle.setPrice(address(weth), liquidationWethPrice);
    }

    function _uint(string memory path) private view returns (uint256) {
        return VM.parseJsonUint(vectorsJson, path);
    }

    function _assertEq(uint256 actual, uint256 expected, string memory message) private pure {
        require(actual == expected, message);
    }
}

// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(require-revert-in-loop)
// forge-lint: disable-end(calls-loop)
// forge-lint: disable-end(multi-contract-file)
