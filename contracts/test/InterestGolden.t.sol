// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal JSON cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Golden vectors intentionally deploy isolated markets and call contracts inside loops.
// forge-lint: disable-start(calls-loop)
// A failing vector must stop its loop and report the case name.
// forge-lint: disable-start(require-revert-in-loop)
// Exact equality is required for deterministic Golden-vector results.
// forge-lint: disable-start(incorrect-strict-equality)
// Fixed amounts define the common 80% utilization fixture.
// forge-lint: disable-start(literal-instead-of-constant)

import {InterestRateModel} from "../src/InterestRateModel.sol";
import {LendingPool} from "../src/LendingPool.sol";
import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MathLib} from "../src/libraries/MathLib.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface IInterestGoldenVm {
    function projectRoot() external view returns (string memory);
    function readFile(string calldata path) external view returns (string memory);
    function parseJsonKeys(string calldata json, string calldata key) external pure returns (string[] memory);
    function parseJsonUint(string calldata json, string calldata key) external pure returns (uint256);
    function warp(uint256 timestamp) external;
    function prank(address sender) external;
}

contract InterestGoldenTest {
    IInterestGoldenVm private constant VM = IInterestGoldenVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    address private constant ALICE = address(0xA11CE);
    address private constant CHARLIE = address(0xC0FFEE);

    string private vectorsJson;

    function setUp() public {
        // forge-lint: disable-next-line(unsafe-cheatcode)
        vectorsJson = VM.readFile(string.concat(VM.projectRoot(), "/../tests/golden/risk_vectors.json"));
    }

    function testGoldenInterestRates() public {
        InterestRateModel model = new InterestRateModel();
        string[] memory names = VM.parseJsonKeys(vectorsJson, ".interest_rate");
        require(names.length > 0, "No interest rate vectors");

        for (uint256 i = 0; i < names.length; ++i) {
            string memory path = string.concat(".interest_rate.", names[i]);
            (uint256 utilizationRay, uint256 borrowRateRay, uint256 liquidityRateRay) = model.marketRates(
                _uint(string.concat(path, ".available_liquidity")),
                _uint(string.concat(path, ".total_debt")),
                _uint(string.concat(path, ".reserve_factor_bps"))
            );

            _assertEq(
                utilizationRay,
                _uint(string.concat(path, ".expected_utilization")),
                string.concat(names[i], ": utilization")
            );
            _assertEq(
                borrowRateRay,
                _uint(string.concat(path, ".expected_borrow_rate")),
                string.concat(names[i], ": borrow rate")
            );
            _assertEq(
                liquidityRateRay,
                _uint(string.concat(path, ".expected_liquidity_rate")),
                string.concat(names[i], ": liquidity rate")
            );
        }
    }

    function testGoldenInterestIndices() public {
        string[] memory names = VM.parseJsonKeys(vectorsJson, ".interest_index");
        require(names.length > 0, "No interest index vectors");

        for (uint256 i = 0; i < names.length; ++i) {
            uint256 startTime = 1_000_000 + i * 40_000_000;
            VM.warp(startTime);
            LendingPool pool = _deployEightyPercentMarket();
            string memory path = string.concat(".interest_index.", names[i]);
            VM.warp(startTime + _uint(string.concat(path, ".elapsed_seconds")));

            _assertEq(
                pool.normalizedBorrowIndex(),
                _uint(string.concat(path, ".expected_borrow_index")),
                string.concat(names[i], ": borrow index")
            );
            _assertEq(
                pool.normalizedLiquidityIndex(),
                _uint(string.concat(path, ".expected_liquidity_index")),
                string.concat(names[i], ": liquidity index")
            );
            _assertEq(pool.borrowIndex(), MathLib.RAY, string.concat(names[i], ": stored borrow index"));
            _assertEq(pool.liquidityIndex(), MathLib.RAY, string.concat(names[i], ": stored liquidity index"));
        }
    }

    function _deployEightyPercentMarket() private returns (LendingPool pool) {
        PriceOracle oracle = new PriceOracle();
        MockWETH weth = new MockWETH();
        MockUSDC usdc = new MockUSDC();
        oracle.registerAsset(address(weth), 400 days);
        oracle.registerAsset(address(usdc), 400 days);
        oracle.setPrice(address(weth), 3_000e8);
        oracle.setPrice(address(usdc), 1e8);

        RiskManager riskManager = new RiskManager(oracle, address(weth), address(usdc));
        pool = new LendingPool(riskManager);
        usdc.mint(CHARLIE, 100_000e6);
        weth.mint(ALICE, 100e18);

        VM.prank(CHARLIE);
        require(usdc.approve(address(pool), type(uint256).max), "Supplier approval failed");
        VM.prank(ALICE);
        require(weth.approve(address(pool), type(uint256).max), "Collateral approval failed");
        VM.prank(CHARLIE);
        pool.supply(address(usdc), 100_000e6);
        VM.prank(ALICE);
        pool.supply(address(weth), 100e18);
        VM.prank(ALICE);
        pool.borrow(address(usdc), 80_000e6);
    }

    function _uint(string memory path) private view returns (uint256) {
        return VM.parseJsonUint(vectorsJson, path);
    }

    function _assertEq(uint256 actual, uint256 expected, string memory message) private pure {
        require(actual == expected, message);
    }
}

// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(incorrect-strict-equality)
// forge-lint: disable-end(require-revert-in-loop)
// forge-lint: disable-end(calls-loop)
// forge-lint: disable-end(multi-contract-file)
