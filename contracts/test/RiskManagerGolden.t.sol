// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal JSON cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)
// Golden vectors intentionally decode fields and update oracle prices inside a loop.
// forge-lint: disable-start(calls-loop)
// A failing vector must stop the loop and report its case name.
// forge-lint: disable-start(require-revert-in-loop)
// Fixed setup literals describe the shared Golden-vector environment.
// forge-lint: disable-start(literal-instead-of-constant)

import {PriceOracle} from "../src/PriceOracle.sol";
import {RiskManager} from "../src/RiskManager.sol";
import {MockUSDC} from "../src/mocks/MockUSDC.sol";
import {MockWETH} from "../src/mocks/MockWETH.sol";

interface IRiskManagerGoldenVm {
    function projectRoot() external view returns (string memory);
    function readFile(string calldata path) external view returns (string memory);
    function parseJsonKeys(string calldata json, string calldata key) external pure returns (string[] memory);
    function parseJsonUint(string calldata json, string calldata key) external pure returns (uint256);
}

contract RiskManagerGoldenTest {
    IRiskManagerGoldenVm private constant VM = IRiskManagerGoldenVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    string private vectorsJson;
    PriceOracle private oracle;
    RiskManager private riskManager;
    MockWETH private weth;
    MockUSDC private usdc;

    function setUp() public {
        // Read only the committed fixture; Foundry's configured filesystem permissions still apply.
        // forge-lint: disable-next-line(unsafe-cheatcode)
        vectorsJson = VM.readFile(string.concat(VM.projectRoot(), "/../tests/golden/risk_vectors.json"));

        oracle = new PriceOracle();
        weth = new MockWETH();
        usdc = new MockUSDC();
        oracle.registerAsset(address(weth), 1 days);
        oracle.registerAsset(address(usdc), 1 days);
        riskManager = new RiskManager(oracle, address(weth), address(usdc));
    }

    function testGoldenRiskPositions() public {
        string[] memory names = VM.parseJsonKeys(vectorsJson, ".risk_position");
        require(names.length > 0, "No risk position vectors");

        for (uint256 i = 0; i < names.length; ++i) {
            string memory path = string.concat(".risk_position.", names[i]);
            uint256 collateralAmount = _uint(string.concat(path, ".collateral_amount"));
            uint256 debtAmount = _uint(string.concat(path, ".debt_amount"));

            oracle.setPrice(address(weth), _uint(string.concat(path, ".weth_price")));
            oracle.setPrice(address(usdc), _uint(string.concat(path, ".usdc_price")));

            _assertEq(
                riskManager.collateralValue(collateralAmount),
                _uint(string.concat(path, ".expected_collateral_value")),
                string.concat(names[i], ": collateral value")
            );
            _assertEq(
                riskManager.debtValue(debtAmount),
                _uint(string.concat(path, ".expected_debt_value")),
                string.concat(names[i], ": debt value")
            );
            _assertEq(
                riskManager.maxBorrow(collateralAmount),
                _uint(string.concat(path, ".expected_max_borrow")),
                string.concat(names[i], ": maximum borrow")
            );
            _assertEq(
                riskManager.healthFactor(collateralAmount, debtAmount),
                _uint(string.concat(path, ".expected_health_factor")),
                string.concat(names[i], ": health factor")
            );
        }
    }

    function _uint(string memory path) private view returns (uint256) {
        return VM.parseJsonUint(vectorsJson, path);
    }

    function _assertEq(uint256 actual, uint256 expected, string memory message) private pure {
        require(actual == expected, message);
    }
}

// forge-lint: disable-end(require-revert-in-loop)
// forge-lint: disable-end(literal-instead-of-constant)
// forge-lint: disable-end(calls-loop)
// forge-lint: disable-end(multi-contract-file)
