// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {ERC20} from "@openzeppelin/contracts/token/ERC20/ERC20.sol";
import {IIndexProvider} from "../interfaces/IIndexProvider.sol";
import {MathLib} from "../libraries/MathLib.sol";

/// @title USDC Deposit Token
/// @notice Non-transferable scaled claim for USDC supplied to the lending pool.
contract DepositToken is ERC20 {
    address public immutable POOL;
    IIndexProvider public immutable INDEX_PROVIDER;

    error InvalidAddress();
    error OnlyPool(address caller);
    error NonTransferable();

    modifier onlyPool() {
        _checkPool();
        _;
    }

    constructor(address pool_) ERC20("USDC Deposit Token", "dUSDC") {
        if (pool_ == address(0)) revert InvalidAddress();
        POOL = pool_;
        INDEX_PROVIDER = IIndexProvider(pool_);
    }

    function decimals() public pure override returns (uint8) {
        return 6;
    }

    /// @notice Returns the supplier claim at the latest projected liquidity index.
    function balanceOf(address account) public view override returns (uint256) {
        return MathLib.mulDivDown(super.balanceOf(account), INDEX_PROVIDER.normalizedLiquidityIndex(), MathLib.RAY);
    }

    /// @notice Returns total supplier claims at the latest projected liquidity index.
    function totalSupply() public view override returns (uint256) {
        return MathLib.mulDivDown(super.totalSupply(), INDEX_PROVIDER.normalizedLiquidityIndex(), MathLib.RAY);
    }

    function scaledBalanceOf(address account) external view returns (uint256) {
        return super.balanceOf(account);
    }

    function scaledTotalSupply() external view returns (uint256) {
        return super.totalSupply();
    }

    function mintScaled(address account, uint256 scaledAmount) external onlyPool {
        _mint(account, scaledAmount);
    }

    function burnScaled(address account, uint256 scaledAmount) external onlyPool {
        _burn(account, scaledAmount);
    }

    function transfer(address, uint256) public pure override returns (bool) {
        revert NonTransferable();
    }

    function approve(address, uint256) public pure override returns (bool) {
        revert NonTransferable();
    }

    function transferFrom(address, address, uint256) public pure override returns (bool) {
        revert NonTransferable();
    }

    function _checkPool() private view {
        if (msg.sender != POOL) revert OnlyPool(msg.sender);
    }
}
