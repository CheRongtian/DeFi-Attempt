// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {SafeERC20} from "@openzeppelin/contracts/token/ERC20/utils/SafeERC20.sol";
import {ReentrancyGuard} from "@openzeppelin/contracts/utils/ReentrancyGuard.sol";
import {RiskManager} from "./RiskManager.sol";
import {MathLib} from "./libraries/MathLib.sol";

/// @title MVP Lending Pool
/// @notice Supports WETH collateral and USDC liquidity, borrowing, repayment and withdrawal.
contract LendingPool is ReentrancyGuard {
    using SafeERC20 for IERC20;

    uint256 public constant MINIMUM_USDC_BORROW = 1e6;

    RiskManager public immutable RISK_MANAGER;
    IERC20 public immutable WETH;
    IERC20 public immutable USDC;

    mapping(address user => uint256 amount) public wethCollateral;
    mapping(address user => uint256 amount) public usdcSupplies;
    mapping(address user => uint256 amount) public usdcDebt;

    uint256 public availableUsdcLiquidity;

    error InvalidAddress();
    error ZeroAmount();
    error UnsupportedAsset(address asset);
    error AssetNotBorrowable(address asset);
    error AssetNotRepayable(address asset);
    error InsufficientSupply(uint256 supplied, uint256 requested);
    error InsufficientLiquidity(uint256 available, uint256 requested);
    error BorrowBelowMinimum(uint256 amount, uint256 minimum);
    error BorrowCapacityExceeded(uint256 debtValueWad, uint256 maxBorrowValueWad);
    error UnhealthyPosition(uint256 healthFactor);
    error NoDebt();
    error RemainingDebtBelowMinimum(uint256 remainingDebt, uint256 minimum);

    event Supplied(address indexed user, address indexed asset, uint256 amount);
    event Withdrawn(address indexed user, address indexed asset, uint256 amount);
    event Borrowed(address indexed user, address indexed asset, uint256 amount);
    event Repaid(address indexed user, address indexed asset, uint256 amount, uint256 remainingDebt);

    constructor(RiskManager riskManager_) {
        if (address(riskManager_) == address(0)) revert InvalidAddress();

        RISK_MANAGER = riskManager_;
        WETH = IERC20(riskManager_.WETH());
        USDC = IERC20(riskManager_.USDC());
    }

    /// @notice Supplies USDC liquidity or WETH collateral for the caller.
    /// @dev Supply does not require an oracle price and remains available during stale prices.
    function supply(address asset, uint256 amount) external nonReentrant {
        if (amount == 0) revert ZeroAmount();

        if (asset == address(USDC)) {
            usdcSupplies[msg.sender] += amount;
            availableUsdcLiquidity += amount;
            USDC.safeTransferFrom(msg.sender, address(this), amount);
        } else if (asset == address(WETH)) {
            wethCollateral[msg.sender] += amount;
            WETH.safeTransferFrom(msg.sender, address(this), amount);
        } else {
            revert UnsupportedAsset(asset);
        }

        emit Supplied(msg.sender, asset, amount);
    }

    /// @notice Borrows USDC against the caller's WETH collateral.
    function borrow(address asset, uint256 amount) external nonReentrant {
        if (asset == address(WETH)) revert AssetNotBorrowable(asset);
        if (asset != address(USDC)) revert UnsupportedAsset(asset);
        if (amount == 0) revert ZeroAmount();
        if (amount < MINIMUM_USDC_BORROW) revert BorrowBelowMinimum(amount, MINIMUM_USDC_BORROW);

        uint256 newDebt = usdcDebt[msg.sender] + amount;
        uint256 maxBorrowValueWad = RISK_MANAGER.maxBorrow(wethCollateral[msg.sender]);
        uint256 newDebtValueWad = RISK_MANAGER.debtValue(newDebt);
        if (newDebtValueWad > maxBorrowValueWad) {
            revert BorrowCapacityExceeded(newDebtValueWad, maxBorrowValueWad);
        }

        uint256 available = availableUsdcLiquidity;
        if (amount > available) revert InsufficientLiquidity(available, amount);

        uint256 resultingHealthFactor = RISK_MANAGER.healthFactor(wethCollateral[msg.sender], newDebt);
        if (resultingHealthFactor < MathLib.WAD) revert UnhealthyPosition(resultingHealthFactor);

        usdcDebt[msg.sender] = newDebt;
        availableUsdcLiquidity = available - amount;
        USDC.safeTransfer(msg.sender, amount);

        emit Borrowed(msg.sender, asset, amount);
    }

    /// @notice Repays up to the caller's outstanding USDC debt.
    /// @dev Excess input is capped at the current debt. Repayment does not require oracle prices.
    function repay(address asset, uint256 amount) external nonReentrant {
        if (asset == address(WETH)) revert AssetNotRepayable(asset);
        if (asset != address(USDC)) revert UnsupportedAsset(asset);
        if (amount == 0) revert ZeroAmount();

        uint256 currentDebt = usdcDebt[msg.sender];
        if (currentDebt == 0) revert NoDebt();

        uint256 repaidAmount = amount > currentDebt ? currentDebt : amount;
        uint256 remainingDebt = currentDebt - repaidAmount;
        if (remainingDebt != 0 && remainingDebt < MINIMUM_USDC_BORROW) {
            revert RemainingDebtBelowMinimum(remainingDebt, MINIMUM_USDC_BORROW);
        }

        usdcDebt[msg.sender] = remainingDebt;
        availableUsdcLiquidity += repaidAmount;
        USDC.safeTransferFrom(msg.sender, address(this), repaidAmount);

        emit Repaid(msg.sender, asset, repaidAmount, remainingDebt);
    }

    /// @notice Withdraws the caller's supplied USDC or WETH collateral.
    function withdraw(address asset, uint256 amount) external nonReentrant {
        if (amount == 0) revert ZeroAmount();

        if (asset == address(USDC)) {
            _withdrawUsdc(amount);
        } else if (asset == address(WETH)) {
            _withdrawWeth(amount);
        } else {
            revert UnsupportedAsset(asset);
        }

        emit Withdrawn(msg.sender, asset, amount);
    }

    function _withdrawUsdc(uint256 amount) private {
        uint256 supplied = usdcSupplies[msg.sender];
        if (amount > supplied) revert InsufficientSupply(supplied, amount);

        uint256 available = availableUsdcLiquidity;
        if (amount > available) revert InsufficientLiquidity(available, amount);

        usdcSupplies[msg.sender] = supplied - amount;
        availableUsdcLiquidity = available - amount;
        USDC.safeTransfer(msg.sender, amount);
    }

    function _withdrawWeth(uint256 amount) private {
        uint256 collateral = wethCollateral[msg.sender];
        if (amount > collateral) revert InsufficientSupply(collateral, amount);

        uint256 remainingCollateral = collateral - amount;

        // Every collateral withdrawal requires a fresh WETH price, including zero-debt positions.
        // The call's validation is the intended effect; its calculated value is not needed here.
        // forge-lint: disable-next-line(unused-return)
        RISK_MANAGER.collateralValue(remainingCollateral);

        uint256 debt = usdcDebt[msg.sender];
        if (debt != 0) {
            uint256 resultingHealthFactor = RISK_MANAGER.healthFactor(remainingCollateral, debt);
            if (resultingHealthFactor < MathLib.WAD) revert UnhealthyPosition(resultingHealthFactor);
        }

        wethCollateral[msg.sender] = remainingCollateral;
        WETH.safeTransfer(msg.sender, amount);
    }
}
