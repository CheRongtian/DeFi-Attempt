// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Keep the minimal JSON cheatcode interface alongside its only consumer.
// forge-lint: disable-start(multi-contract-file)

import {SafeCast} from "@openzeppelin/contracts/utils/math/SafeCast.sol";
import {MathLib} from "../src/libraries/MathLib.sol";

interface IMathLibGoldenVm {
    function projectRoot() external view returns (string memory);
    function readFile(string calldata path) external view returns (string memory);
    function parseJsonKeys(string calldata json, string calldata key) external pure returns (string[] memory);
    function parseJsonUint(string calldata json, string calldata key) external pure returns (uint256);
}

/// @dev Before running, grant read access under [profile.default] in contracts/foundry.toml:
/// fs_permissions = [{ access = "read", path = "../tests/golden" }]
contract MathLibGoldenTest {
    IMathLibGoldenVm private constant VM = IMathLibGoldenVm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    string private vectorsJson;

    function setUp() public {
        // Read only the committed fixture; Foundry's configured filesystem permissions still apply.
        // forge-lint: disable-next-line(unsafe-cheatcode)
        vectorsJson = VM.readFile(string.concat(VM.projectRoot(), "/../tests/golden/risk_vectors.json"));
    }

    function testGoldenScales() public view {
        _assertEq(_uint(".units.wad"), MathLib.WAD, "WAD scale");
        _assertEq(_uint(".units.bps"), MathLib.BPS, "BPS scale");
        _assertEq(_uint(".units.oracle_price_scale"), MathLib.ORACLE_PRICE_SCALE, "Oracle price scale");
    }

    function testGoldenMulDivVectors() public view {
        string[] memory names = VM.parseJsonKeys(vectorsJson, ".mul_div");
        require(names.length > 0, "No mulDiv vectors");

        for (uint256 i = 0; i < names.length; ++i) {
            string memory path = string.concat(".mul_div.", names[i]);
            uint256 x = _uint(string.concat(path, ".x"));
            uint256 y = _uint(string.concat(path, ".y"));
            uint256 denominator = _uint(string.concat(path, ".denominator"));

            _assertEq(
                MathLib.mulDivDown(x, y, denominator),
                _uint(string.concat(path, ".expected_down")),
                string.concat(names[i], ": round down")
            );
            _assertEq(
                MathLib.mulDivUp(x, y, denominator),
                _uint(string.concat(path, ".expected_up")),
                string.concat(names[i], ": round up")
            );
        }
    }

    function testGoldenUsdValueVectors() public view {
        string[] memory names = VM.parseJsonKeys(vectorsJson, ".usd_value");
        require(names.length > 0, "No USD value vectors");

        for (uint256 i = 0; i < names.length; ++i) {
            string memory path = string.concat(".usd_value.", names[i]);
            uint256 amount = _uint(string.concat(path, ".amount"));
            uint256 price = _uint(string.concat(path, ".price"));
            uint8 decimals = SafeCast.toUint8(_uint(string.concat(path, ".token_decimals")));

            _assertEq(
                MathLib.toUsdWadDown(amount, price, decimals),
                _uint(string.concat(path, ".expected_down")),
                string.concat(names[i], ": round down")
            );
            _assertEq(
                MathLib.toUsdWadUp(amount, price, decimals),
                _uint(string.concat(path, ".expected_up")),
                string.concat(names[i], ": round up")
            );
        }
    }

    function _uint(string memory path) private view returns (uint256) {
        // Decimal strings preserve all 256 bits when other languages consume the same JSON.
        // Each vector field is decoded by a pure Foundry cheatcode, with no application contract call.
        // forge-lint: disable-next-line(calls-loop)
        return VM.parseJsonUint(vectorsJson, path);
    }

    function _assertEq(uint256 actual, uint256 expected, string memory message) private pure {
        // Stop at the failing vector and report its name, even when called from a loop.
        // forge-lint: disable-next-line(require-revert-in-loop)
        require(actual == expected, message);
    }
}

// forge-lint: disable-end(multi-contract-file)
