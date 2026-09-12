// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {Math} from "@openzeppelin/contracts/utils/math/Math.sol";

/// @title Lending Fixed-Point Math
/// @notice Explicit rounding and USD valuation for the MVP's 6- and 18-decimal tokens.
library MathLib {
    uint256 internal constant WAD = 1e18;
    uint256 internal constant BPS = 1e4;
    uint256 internal constant ORACLE_PRICE_SCALE = 1e8;

    error UnsupportedDecimals(uint8 tokenDecimals);

    /// @notice Returns floor(x * y / denominator), using a full-precision intermediate product.
    /// @dev Reverts if denominator is zero or the final result does not fit in uint256.
    function mulDivDown(uint256 x, uint256 y, uint256 denominator) internal pure returns (uint256) {
        return Math.mulDiv(x, y, denominator, Math.Rounding.Floor);
    }

    /// @notice Returns ceil(x * y / denominator), using a full-precision intermediate product.
    /// @dev Reverts if denominator is zero or the rounded result does not fit in uint256.
    function mulDivUp(uint256 x, uint256 y, uint256 denominator) internal pure returns (uint256) {
        return Math.mulDiv(x, y, denominator, Math.Rounding.Ceil);
    }

    /// @notice Converts a token amount to USD WAD, rounding down (for collateral valuation).
    /// @param amount Amount in the token's smallest units.
    /// @param oraclePrice USD price per whole token, scaled by 1e8.
    /// @param tokenDecimals Must be 6 (USDC) or 18 (WETH).
    /// @dev Price validity is the caller/oracle's responsibility; zero inputs yield zero.
    function toUsdWadDown(uint256 amount, uint256 oraclePrice, uint8 tokenDecimals) internal pure returns (uint256) {
        return _toUsdWad(amount, oraclePrice, tokenDecimals, Math.Rounding.Floor);
    }

    /// @notice Converts a token amount to USD WAD, rounding up (for debt valuation).
    /// @param amount Amount in the token's smallest units.
    /// @param oraclePrice USD price per whole token, scaled by 1e8.
    /// @param tokenDecimals Must be 6 (USDC) or 18 (WETH).
    /// @dev Price validity is the caller/oracle's responsibility; zero inputs yield zero.
    function toUsdWadUp(uint256 amount, uint256 oraclePrice, uint8 tokenDecimals) internal pure returns (uint256) {
        return _toUsdWad(amount, oraclePrice, tokenDecimals, Math.Rounding.Ceil);
    }

    function _toUsdWad(uint256 amount, uint256 oraclePrice, uint8 tokenDecimals, Math.Rounding rounding)
        private
        pure
        returns (uint256)
    {
        // Cancel WAD against the token scale before multiplication to avoid needless overflow.
        if (tokenDecimals == 18) {
            return Math.mulDiv(amount, oraclePrice, ORACLE_PRICE_SCALE, rounding);
        }

        if (tokenDecimals == 6) {
            // WAD / 1e6 / 1e8 = 1e4: the result is exact in either rounding direction.
            // Checked multiplication is sufficient: an overflowing product cannot be reduced here.
            // Both divisions are exact constant divisions; no runtime precision is lost.
            // forge-lint: disable-next-line(divide-before-multiply)
            return amount * oraclePrice * (WAD / 1e6 / ORACLE_PRICE_SCALE);
        }

        // Each conversion must validate its decimals, including calls from a batch of test vectors.
        // forge-lint: disable-next-line(require-revert-in-loop)
        revert UnsupportedDecimals(tokenDecimals);
    }
}
