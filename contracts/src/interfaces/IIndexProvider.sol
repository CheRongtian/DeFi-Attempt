// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

/// @title Lending Index Provider
/// @notice Supplies projected market indices to scaled position tokens.
interface IIndexProvider {
    function normalizedLiquidityIndex() external view returns (uint256);
    function normalizedBorrowIndex() external view returns (uint256);
}
