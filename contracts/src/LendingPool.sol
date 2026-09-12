// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

import {AccessControl} from "@openzeppelin/contracts/access/AccessControl.sol";
import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {SafeERC20} from "@openzeppelin/contracts/token/ERC20/utils/SafeERC20.sol";
import {ReentrancyGuard} from "@openzeppelin/contracts/utils/ReentrancyGuard.sol";
import {RiskManager} from "./RiskManager.sol";
import {MathLib} from "./libraries/MathLib.sol";

/// @title MVP Lending Pool
/// @notice Supports WETH collateral and USDC liquidity, borrowing, repayment and withdrawal.
contract LendingPool is ReentrancyGuard, AccessControl {
    using SafeERC20 for IERC20;

    uint256 public constant MINIMUM_USDC_BORROW = 1e6;
    bytes32 public constant LIQUIDATION_ROLE = keccak256("LIQUIDATION_ROLE");

    RiskManager public immutable RISK_MANAGER;
    IERC20 public immutable WETH;
    IERC20 public immutable USDC;

    mapping(address user => uint256 amount) public wethCollateral;
    mapping(address user => uint256 amount) public usdcSupplies;
    mapping(address user => uint256 amount) public usdcDebt;

    uint256 public availableUsdcLiquidity;
    uint256 public totalUsdcSupplies;
    uint256 public totalWethCollateral;
    uint256 public totalPerformingUsdcDebt;
    uint256 public badDebt;

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
    error LiquidationExceedsDebt(uint256 debt, uint256 repayment);
    error LiquidationExceedsCollateral(uint256 collateral, uint256 seized);

    event Supplied(address indexed user, address indexed asset, uint256 amount);
    event Withdrawn(address indexed user, address indexed asset, uint256 amount);
    event Borrowed(address indexed user, address indexed asset, uint256 amount);
    event Repaid(address indexed user, address indexed asset, uint256 amount, uint256 remainingDebt);
    event Liquidated(
        address indexed liquidator,
        address indexed borrower,
        address indexed debtAsset,
        address collateralAsset,
        uint256 repaidAmount,
        uint256 collateralSeized
    );
    event BadDebtRecognized(address indexed borrower, uint256 amount);

    constructor(RiskManager riskManager_) {
        if (address(riskManager_) == address(0)) revert InvalidAddress();

        RISK_MANAGER = riskManager_;
        WETH = IERC20(riskManager_.WETH());
        USDC = IERC20(riskManager_.USDC());
        _grantRole(DEFAULT_ADMIN_ROLE, msg.sender);
    }

    /// @notice Supplies USDC liquidity or WETH collateral for the caller.
    /// @dev Supply does not require an oracle price and remains available during stale prices.
    function supply(address asset, uint256 amount) external nonReentrant {
        if (amount == 0) revert ZeroAmount();

        if (asset == address(USDC)) {
            usdcSupplies[msg.sender] += amount;
            availableUsdcLiquidity += amount;
            totalUsdcSupplies += amount;
            USDC.safeTransferFrom(msg.sender, address(this), amount);
        } else if (asset == address(WETH)) {
            wethCollateral[msg.sender] += amount;
            totalWethCollateral += amount;
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
        totalPerformingUsdcDebt += amount;
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
        totalPerformingUsdcDebt -= repaidAmount;
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
        totalUsdcSupplies -= amount;
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
        totalWethCollateral -= amount;
        WETH.safeTransfer(msg.sender, amount);
    }

    /// @notice Applies a liquidation calculated by an authorized liquidation manager.
    /// @dev Pulls USDC from the liquidator and sends seized WETH after updating all accounting.
    function executeLiquidation(address borrower, address liquidator, uint256 repaidAmount, uint256 collateralSeized)
        external
        nonReentrant
        onlyRole(LIQUIDATION_ROLE)
        returns (uint256 recognizedBadDebt)
    {
        if (borrower == address(0) || liquidator == address(0)) revert InvalidAddress();
        if (repaidAmount == 0 || collateralSeized == 0) revert ZeroAmount();

        uint256 currentDebt = usdcDebt[borrower];
        uint256 currentCollateral = wethCollateral[borrower];
        if (repaidAmount > currentDebt) revert LiquidationExceedsDebt(currentDebt, repaidAmount);
        if (collateralSeized > currentCollateral) {
            revert LiquidationExceedsCollateral(currentCollateral, collateralSeized);
        }

        uint256 remainingDebt = currentDebt - repaidAmount;
        uint256 remainingCollateral = currentCollateral - collateralSeized;

        usdcDebt[borrower] = remainingDebt;
        wethCollateral[borrower] = remainingCollateral;
        totalPerformingUsdcDebt -= repaidAmount;
        totalWethCollateral -= collateralSeized;
        availableUsdcLiquidity += repaidAmount;

        if (remainingCollateral == 0 && remainingDebt != 0) {
            recognizedBadDebt = remainingDebt;
            usdcDebt[borrower] = 0;
            remainingDebt = 0;
            totalPerformingUsdcDebt -= recognizedBadDebt;
            badDebt += recognizedBadDebt;
        }

        // The trusted liquidation manager supplies the initiating liquidator, who approved this pool directly.
        // forge-lint: disable-next-line(arbitrary-send-erc20)
        USDC.safeTransferFrom(liquidator, address(this), repaidAmount);
        WETH.safeTransfer(liquidator, collateralSeized);

        emit Liquidated(liquidator, borrower, address(USDC), address(WETH), repaidAmount, collateralSeized);
        if (recognizedBadDebt != 0) emit BadDebtRecognized(borrower, recognizedBadDebt);
    }
}
