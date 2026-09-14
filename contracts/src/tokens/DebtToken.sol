// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {ERC20} from "@openzeppelin/contracts/token/ERC20/ERC20.sol";
import {IIndexProvider} from "../interfaces/IIndexProvider.sol";
import {MathLib} from "../libraries/MathLib.sol";

/// @title USDC Debt Token
/// @notice Non-transferable scaled representation of performing USDC debt.
contract DebtToken is ERC20 {
    address public immutable POOL;
    IIndexProvider public immutable INDEX_PROVIDER;

    error InvalidAddress();
    error OnlyPool(address caller);
    error NonTransferable();

    modifier onlyPool() {
        _checkPool();
        _;
    }

    constructor(address pool_) ERC20("USDC Debt Token", "debtUSDC") {
        if (pool_ == address(0)) revert InvalidAddress();
        POOL = pool_;
        INDEX_PROVIDER = IIndexProvider(pool_);
    }

    function decimals() public pure override returns (uint8) {
        return 6;
    }

    /// @notice Returns current debt rounded up at the latest projected borrow index.
    function balanceOf(address account) public view override returns (uint256) {
        return MathLib.mulDivUp(super.balanceOf(account), INDEX_PROVIDER.normalizedBorrowIndex(), MathLib.RAY);
    }

    /// @notice Returns total performing debt rounded up at the projected borrow index.
    function totalSupply() public view override returns (uint256) {
        return MathLib.mulDivUp(super.totalSupply(), INDEX_PROVIDER.normalizedBorrowIndex(), MathLib.RAY);
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
