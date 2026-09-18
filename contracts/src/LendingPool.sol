// SPDX-License-Identifier: MIT
pragma solidity ^0.8.36;

// Immutable pool-owned position tokens expose no callback path and all entry points are non-reentrant.
// forge-lint: disable-start(reentrancy-no-eth)
// Events intentionally follow completed token transfers so logs describe successful state transitions.
// forge-lint: disable-start(reentrancy-events)

import {AccessControl} from "@openzeppelin/contracts/access/AccessControl.sol";
import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {SafeERC20} from "@openzeppelin/contracts/token/ERC20/utils/SafeERC20.sol";
import {ReentrancyGuard} from "@openzeppelin/contracts/utils/ReentrancyGuard.sol";
import {InterestRateModel} from "./InterestRateModel.sol";
import {IIndexProvider} from "./interfaces/IIndexProvider.sol";
import {RiskManager} from "./RiskManager.sol";
import {MathLib} from "./libraries/MathLib.sol";
import {DebtToken} from "./tokens/DebtToken.sol";
import {DepositToken} from "./tokens/DepositToken.sol";

/// @title Indexed Lending Pool
/// @notice Supports WETH collateral and interest-bearing USDC lending with scaled position tokens.
contract LendingPool is ReentrancyGuard, AccessControl, IIndexProvider {
    using SafeERC20 for IERC20;

    uint256 public constant MINIMUM_USDC_BORROW = 1e6;
    uint256 public constant RESERVE_FACTOR_BPS = 1_000;
    uint256 public constant SECONDS_PER_YEAR = 365 days;
    bytes32 public constant LIQUIDATION_ROLE = keccak256("LIQUIDATION_ROLE");

    RiskManager public immutable RISK_MANAGER;
    InterestRateModel public immutable INTEREST_RATE_MODEL;
    DepositToken public immutable DEPOSIT_TOKEN;
    DebtToken public immutable DEBT_TOKEN;
    IERC20 public immutable WETH;
    IERC20 public immutable USDC;

    mapping(address user => uint256 amount) public wethCollateral;

    uint256 public availableUsdcLiquidity;
    uint256 public totalWethCollateral;
    uint256 public badDebt;
    uint256 public protocolReserve;
    uint256 public liquidityIndex;
    uint256 public borrowIndex;
    uint256 public lastUpdateTimestamp;

    error InvalidAddress();
    error ZeroAmount();
    error ScaledAmountTooSmall();
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
    error UnexpectedTokenTransfer(address asset, uint256 expected, uint256 debited, uint256 credited);

    event Supplied(address indexed user, address indexed asset, uint256 amount);
    event Withdrawn(address indexed user, address indexed asset, uint256 amount);
    event Borrowed(address indexed user, address indexed asset, uint256 amount);
    event Repaid(address indexed user, address indexed asset, uint256 amount, uint256 remainingDebt);
    event InterestAccrued(
        uint256 indexed timestamp, uint256 borrowIndex, uint256 liquidityIndex, uint256 reserveAccrued
    );
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
        INTEREST_RATE_MODEL = new InterestRateModel();

        liquidityIndex = MathLib.RAY;
        borrowIndex = MathLib.RAY;
        lastUpdateTimestamp = block.timestamp;

        DEPOSIT_TOKEN = new DepositToken(address(this));
        DEBT_TOKEN = new DebtToken(address(this));
        _grantRole(DEFAULT_ADMIN_ROLE, msg.sender);
    }

    /// @notice Returns a supplier's current USDC claim including projected interest.
    function usdcSupplies(address user) public view returns (uint256) {
        return DEPOSIT_TOKEN.balanceOf(user);
    }

    /// @notice Returns a borrower's current performing USDC debt including projected interest.
    function usdcDebt(address user) public view returns (uint256) {
        return DEBT_TOKEN.balanceOf(user);
    }

    function totalUsdcSupplies() public view returns (uint256) {
        return DEPOSIT_TOKEN.totalSupply();
    }

    function totalPerformingUsdcDebt() public view returns (uint256) {
        return DEBT_TOKEN.totalSupply();
    }

    function totalScaledSupply() public view returns (uint256) {
        return DEPOSIT_TOKEN.scaledTotalSupply();
    }

    function totalScaledDebt() public view returns (uint256) {
        return DEBT_TOKEN.scaledTotalSupply();
    }

    function reserveFactorBps() external pure returns (uint256) {
        return RESERVE_FACTOR_BPS;
    }

    /// @notice Returns the borrow index projected through the current timestamp.
    function normalizedBorrowIndex() public view override returns (uint256) {
        (uint256 projectedBorrowIndex,) = _projectedIndices();
        return projectedBorrowIndex;
    }

    /// @notice Returns the liquidity index projected through the current timestamp.
    function normalizedLiquidityIndex() public view override returns (uint256) {
        (, uint256 projectedLiquidityIndex) = _projectedIndices();
        return projectedLiquidityIndex;
    }

    /// @notice Stores interest accumulated since the previous market update.
    function accrueInterest() external nonReentrant {
        _accrueInterest();
    }

    /// @notice Supplies USDC liquidity or WETH collateral for the caller.
    /// @dev Supply does not require an oracle price and remains available during stale prices.
    function supply(address asset, uint256 amount) external nonReentrant {
        if (amount == 0) revert ZeroAmount();
        if (asset != address(USDC) && asset != address(WETH)) revert UnsupportedAsset(asset);

        _accrueInterest();

        if (asset == address(USDC)) {
            _supplyUsdc(amount);
        } else {
            wethCollateral[msg.sender] += amount;
            totalWethCollateral += amount;
            _pullExact(WETH, msg.sender, amount);
        }

        emit Supplied(msg.sender, asset, amount);
    }

    /// @notice Borrows USDC against the caller's WETH collateral.
    function borrow(address asset, uint256 amount) external nonReentrant {
        if (asset == address(WETH)) revert AssetNotBorrowable(asset);
        if (asset != address(USDC)) revert UnsupportedAsset(asset);
        if (amount == 0) revert ZeroAmount();
        if (amount < MINIMUM_USDC_BORROW) revert BorrowBelowMinimum(amount, MINIMUM_USDC_BORROW);

        _accrueInterest();

        uint256 available = availableUsdcLiquidity;
        if (amount > available) revert InsufficientLiquidity(available, amount);

        uint256 scaledBalance = DEBT_TOKEN.scaledBalanceOf(msg.sender);
        uint256 scaledMint = MathLib.mulDivUp(amount, MathLib.RAY, borrowIndex);
        if (scaledMint == 0) revert ScaledAmountTooSmall();

        uint256 currentDebt = _debtAtIndex(scaledBalance, borrowIndex);
        uint256 newDebt = _debtAtIndex(scaledBalance + scaledMint, borrowIndex);
        uint256 maxBorrowValueWad = RISK_MANAGER.maxBorrow(wethCollateral[msg.sender]);
        uint256 newDebtValueWad = RISK_MANAGER.debtValue(newDebt);
        if (newDebtValueWad > maxBorrowValueWad) {
            revert BorrowCapacityExceeded(newDebtValueWad, maxBorrowValueWad);
        }

        uint256 resultingHealthFactor = RISK_MANAGER.healthFactor(wethCollateral[msg.sender], newDebt);
        if (resultingHealthFactor < MathLib.WAD) revert UnhealthyPosition(resultingHealthFactor);

        uint256 debtIncrease = newDebt - currentDebt;
        if (debtIncrease > amount) protocolReserve += debtIncrease - amount;

        availableUsdcLiquidity = available - amount;
        DEBT_TOKEN.mintScaled(msg.sender, scaledMint);
        _pushExact(USDC, msg.sender, amount);

        emit Borrowed(msg.sender, asset, amount);
    }

    /// @notice Repays up to the caller's outstanding USDC debt.
    /// @dev Repayment does not require oracle prices and may include accrued interest.
    function repay(address asset, uint256 amount) external nonReentrant {
        if (asset == address(WETH)) revert AssetNotRepayable(asset);
        if (asset != address(USDC)) revert UnsupportedAsset(asset);
        if (amount == 0) revert ZeroAmount();

        _accrueInterest();

        uint256 scaledBalance = DEBT_TOKEN.scaledBalanceOf(msg.sender);
        if (scaledBalance == 0) revert NoDebt();

        uint256 currentDebt = _debtAtIndex(scaledBalance, borrowIndex);
        uint256 repaidAmount = amount > currentDebt ? currentDebt : amount;
        (uint256 scaledBurn, uint256 remainingDebt) = _scaledDebtBurn(scaledBalance, repaidAmount);

        if (remainingDebt != 0 && remainingDebt < MINIMUM_USDC_BORROW) {
            revert RemainingDebtBelowMinimum(remainingDebt, MINIMUM_USDC_BORROW);
        }

        uint256 debtReduction = currentDebt - remainingDebt;
        if (repaidAmount > debtReduction) protocolReserve += repaidAmount - debtReduction;

        DEBT_TOKEN.burnScaled(msg.sender, scaledBurn);
        availableUsdcLiquidity += repaidAmount;
        _pullExact(USDC, msg.sender, repaidAmount);

        emit Repaid(msg.sender, asset, repaidAmount, remainingDebt);
    }

    /// @notice Withdraws the caller's supplied USDC or WETH collateral.
    function withdraw(address asset, uint256 amount) external nonReentrant {
        if (amount == 0) revert ZeroAmount();
        if (asset != address(USDC) && asset != address(WETH)) revert UnsupportedAsset(asset);

        _accrueInterest();

        if (asset == address(USDC)) {
            _withdrawUsdc(amount);
        } else {
            _withdrawWeth(amount);
        }

        emit Withdrawn(msg.sender, asset, amount);
    }

    /// @notice Applies a liquidation calculated by an authorized liquidation manager.
    function executeLiquidation(address borrower, address liquidator, uint256 repaidAmount, uint256 collateralSeized)
        external
        nonReentrant
        onlyRole(LIQUIDATION_ROLE)
        returns (uint256 recognizedBadDebt)
    {
        if (borrower == address(0) || liquidator == address(0)) revert InvalidAddress();
        if (repaidAmount == 0 || collateralSeized == 0) revert ZeroAmount();

        _accrueInterest();

        uint256 scaledBalance = DEBT_TOKEN.scaledBalanceOf(borrower);
        uint256 currentDebt = _debtAtIndex(scaledBalance, borrowIndex);
        uint256 currentCollateral = wethCollateral[borrower];
        if (repaidAmount > currentDebt) revert LiquidationExceedsDebt(currentDebt, repaidAmount);
        if (collateralSeized > currentCollateral) {
            revert LiquidationExceedsCollateral(currentCollateral, collateralSeized);
        }

        (uint256 scaledBurn, uint256 remainingDebt) = _scaledDebtBurn(scaledBalance, repaidAmount);
        uint256 debtReduction = currentDebt - remainingDebt;
        if (repaidAmount > debtReduction) protocolReserve += repaidAmount - debtReduction;

        DEBT_TOKEN.burnScaled(borrower, scaledBurn);
        uint256 remainingCollateral = currentCollateral - collateralSeized;
        wethCollateral[borrower] = remainingCollateral;
        totalWethCollateral -= collateralSeized;
        availableUsdcLiquidity += repaidAmount;

        if (remainingCollateral == 0 && remainingDebt != 0) {
            recognizedBadDebt = remainingDebt;
            uint256 remainingScaledDebt = DEBT_TOKEN.scaledBalanceOf(borrower);
            DEBT_TOKEN.burnScaled(borrower, remainingScaledDebt);
            badDebt += recognizedBadDebt;
        }

        // The authorized manager supplies the initiating liquidator, who approved this pool directly.
        // forge-lint: disable-next-line(arbitrary-send-erc20)
        _pullExact(USDC, liquidator, repaidAmount);
        _pushExact(WETH, liquidator, collateralSeized);

        emit Liquidated(liquidator, borrower, address(USDC), address(WETH), repaidAmount, collateralSeized);
        if (recognizedBadDebt != 0) emit BadDebtRecognized(borrower, recognizedBadDebt);
    }

    function _supplyUsdc(uint256 amount) private {
        uint256 supplyBefore = _supplyAtIndex(DEPOSIT_TOKEN.scaledTotalSupply(), liquidityIndex);
        uint256 scaledMint = MathLib.mulDivDown(amount, MathLib.RAY, liquidityIndex);
        if (scaledMint == 0) revert ScaledAmountTooSmall();

        DEPOSIT_TOKEN.mintScaled(msg.sender, scaledMint);
        uint256 supplyAfter = _supplyAtIndex(DEPOSIT_TOKEN.scaledTotalSupply(), liquidityIndex);
        uint256 claimIncrease = supplyAfter - supplyBefore;
        if (amount > claimIncrease) protocolReserve += amount - claimIncrease;

        availableUsdcLiquidity += amount;
        _pullExact(USDC, msg.sender, amount);
    }

    function _withdrawUsdc(uint256 amount) private {
        uint256 scaledBalance = DEPOSIT_TOKEN.scaledBalanceOf(msg.sender);
        uint256 supplied = _supplyAtIndex(scaledBalance, liquidityIndex);
        if (amount > supplied) revert InsufficientSupply(supplied, amount);

        uint256 available = availableUsdcLiquidity;
        if (amount > available) revert InsufficientLiquidity(available, amount);

        uint256 scaledBurn = amount == supplied ? scaledBalance : MathLib.mulDivUp(amount, MathLib.RAY, liquidityIndex);
        if (scaledBurn == 0 || scaledBurn > scaledBalance) revert ScaledAmountTooSmall();

        uint256 supplyBefore = _supplyAtIndex(DEPOSIT_TOKEN.scaledTotalSupply(), liquidityIndex);
        DEPOSIT_TOKEN.burnScaled(msg.sender, scaledBurn);
        uint256 supplyAfter = _supplyAtIndex(DEPOSIT_TOKEN.scaledTotalSupply(), liquidityIndex);
        uint256 claimDecrease = supplyBefore - supplyAfter;
        if (claimDecrease > amount) protocolReserve += claimDecrease - amount;

        availableUsdcLiquidity = available - amount;
        _pushExact(USDC, msg.sender, amount);
    }

    function _withdrawWeth(uint256 amount) private {
        uint256 collateral = wethCollateral[msg.sender];
        if (amount > collateral) revert InsufficientSupply(collateral, amount);

        uint256 remainingCollateral = collateral - amount;

        // Every collateral withdrawal requires a fresh WETH price, including zero-debt positions.
        // forge-lint: disable-next-line(unused-return)
        RISK_MANAGER.collateralValue(remainingCollateral);

        uint256 debt = usdcDebt(msg.sender);
        if (debt != 0) {
            uint256 resultingHealthFactor = RISK_MANAGER.healthFactor(remainingCollateral, debt);
            if (resultingHealthFactor < MathLib.WAD) revert UnhealthyPosition(resultingHealthFactor);
        }

        wethCollateral[msg.sender] = remainingCollateral;
        totalWethCollateral -= amount;
        _pushExact(WETH, msg.sender, amount);
    }

    /// @dev The MVP supports exact-transfer assets only. SafeERC20 handles optional return values;
    ///      these balance checks reject fee-on-transfer, rebasing, or otherwise non-exact semantics.
    function _pullExact(IERC20 token, address from, uint256 amount) private {
        uint256 senderBalanceBefore = token.balanceOf(from);
        uint256 poolBalanceBefore = token.balanceOf(address(this));

        token.safeTransferFrom(from, address(this), amount);

        uint256 senderBalanceAfter = token.balanceOf(from);
        uint256 poolBalanceAfter = token.balanceOf(address(this));
        uint256 debited = senderBalanceBefore >= senderBalanceAfter ? senderBalanceBefore - senderBalanceAfter : 0;
        uint256 credited = poolBalanceAfter >= poolBalanceBefore ? poolBalanceAfter - poolBalanceBefore : 0;
        if (debited != amount || credited != amount) {
            revert UnexpectedTokenTransfer(address(token), amount, debited, credited);
        }
    }

    function _pushExact(IERC20 token, address to, uint256 amount) private {
        uint256 poolBalanceBefore = token.balanceOf(address(this));
        uint256 recipientBalanceBefore = token.balanceOf(to);

        token.safeTransfer(to, amount);

        uint256 poolBalanceAfter = token.balanceOf(address(this));
        uint256 recipientBalanceAfter = token.balanceOf(to);
        uint256 debited = poolBalanceBefore >= poolBalanceAfter ? poolBalanceBefore - poolBalanceAfter : 0;
        uint256 credited =
            recipientBalanceAfter >= recipientBalanceBefore ? recipientBalanceAfter - recipientBalanceBefore : 0;
        if (debited != amount || credited != amount) {
            revert UnexpectedTokenTransfer(address(token), amount, debited, credited);
        }
    }

    function _scaledDebtBurn(uint256 scaledBalance, uint256 repayment)
        private
        view
        returns (uint256 scaledBurn, uint256 remainingDebt)
    {
        uint256 currentDebt = _debtAtIndex(scaledBalance, borrowIndex);
        if (repayment >= currentDebt) return (scaledBalance, 0);

        scaledBurn = MathLib.mulDivDown(repayment, MathLib.RAY, borrowIndex);
        if (scaledBurn == 0) revert ScaledAmountTooSmall();
        remainingDebt = _debtAtIndex(scaledBalance - scaledBurn, borrowIndex);
    }

    function _accrueInterest() private {
        uint256 elapsedSeconds = block.timestamp - lastUpdateTimestamp;
        // Timestamp equality is the required guard against double accrual in the same block.
        // forge-lint: disable-next-line(block-timestamp)
        if (elapsedSeconds == 0) return;

        uint256 previousBorrowIndex = borrowIndex;
        uint256 previousLiquidityIndex = liquidityIndex;
        (uint256 nextBorrowIndex, uint256 nextLiquidityIndex) = _projectedIndices();

        uint256 scaledDebt = DEBT_TOKEN.scaledTotalSupply();
        uint256 scaledSupply = DEPOSIT_TOKEN.scaledTotalSupply();
        uint256 debtBefore = _debtAtIndex(scaledDebt, previousBorrowIndex);
        uint256 debtAfter = _debtAtIndex(scaledDebt, nextBorrowIndex);
        uint256 supplyBefore = _supplyAtIndex(scaledSupply, previousLiquidityIndex);
        uint256 supplyAfter = _supplyAtIndex(scaledSupply, nextLiquidityIndex);

        uint256 debtInterest = debtAfter - debtBefore;
        uint256 supplierInterest = supplyAfter - supplyBefore;
        uint256 reserveAccrued = debtInterest - supplierInterest;

        borrowIndex = nextBorrowIndex;
        liquidityIndex = nextLiquidityIndex;
        lastUpdateTimestamp = block.timestamp;
        protocolReserve += reserveAccrued;

        emit InterestAccrued(block.timestamp, nextBorrowIndex, nextLiquidityIndex, reserveAccrued);
    }

    function _projectedIndices() private view returns (uint256 projectedBorrowIndex, uint256 projectedLiquidityIndex) {
        projectedBorrowIndex = borrowIndex;
        projectedLiquidityIndex = liquidityIndex;

        uint256 elapsedSeconds = block.timestamp - lastUpdateTimestamp;
        uint256 scaledDebt = DEBT_TOKEN.scaledTotalSupply();
        // Projected views must leave the stored index unchanged at the same timestamp.
        // forge-lint: disable-next-line(block-timestamp)
        if (elapsedSeconds == 0 || scaledDebt == 0) return (projectedBorrowIndex, projectedLiquidityIndex);

        uint256 totalDebt = _debtAtIndex(scaledDebt, borrowIndex);
        uint256 utilizationRay = INTEREST_RATE_MODEL.utilization(availableUsdcLiquidity, totalDebt);
        uint256 borrowRateRay = INTEREST_RATE_MODEL.borrowRate(utilizationRay);
        uint256 liquidityRateRay = INTEREST_RATE_MODEL.liquidityRate(utilizationRay, borrowRateRay, RESERVE_FACTOR_BPS);

        uint256 borrowGrowth = MathLib.RAY + MathLib.mulDivDown(borrowRateRay, elapsedSeconds, SECONDS_PER_YEAR);
        uint256 liquidityGrowth = MathLib.RAY + MathLib.mulDivDown(liquidityRateRay, elapsedSeconds, SECONDS_PER_YEAR);

        projectedBorrowIndex = MathLib.mulDivDown(borrowIndex, borrowGrowth, MathLib.RAY);
        projectedLiquidityIndex = MathLib.mulDivDown(liquidityIndex, liquidityGrowth, MathLib.RAY);

        projectedLiquidityIndex = _cappedLiquidityIndex(scaledDebt, projectedBorrowIndex, projectedLiquidityIndex);
    }

    function _cappedLiquidityIndex(uint256 scaledDebt, uint256 projectedBorrowIndex, uint256 projectedLiquidityIndex)
        private
        view
        returns (uint256)
    {
        uint256 scaledSupply = DEPOSIT_TOKEN.scaledTotalSupply();
        if (scaledSupply == 0) return projectedLiquidityIndex;

        uint256 debtBefore = _debtAtIndex(scaledDebt, borrowIndex);
        uint256 debtAfter = _debtAtIndex(scaledDebt, projectedBorrowIndex);
        uint256 distributableInterest =
            MathLib.mulDivDown(debtAfter - debtBefore, MathLib.BPS - RESERVE_FACTOR_BPS, MathLib.BPS);
        uint256 supplyBefore = _supplyAtIndex(scaledSupply, liquidityIndex);
        uint256 supplyAfter = _supplyAtIndex(scaledSupply, projectedLiquidityIndex);

        if (supplyAfter - supplyBefore <= distributableInterest) return projectedLiquidityIndex;
        return _maximumLiquidityIndex(scaledSupply, supplyBefore + distributableInterest);
    }

    /// @dev Returns the greatest index whose rounded-down supplier claim does not exceed `maximumSupply`.
    function _maximumLiquidityIndex(uint256 scaledSupply, uint256 maximumSupply) private pure returns (uint256) {
        uint256 numeratorUnits = maximumSupply + 1;
        uint256 maximumIndex = MathLib.mulDivDown(numeratorUnits, MathLib.RAY, scaledSupply);

        // floor(scaledSupply * index / RAY) must remain strictly below maximumSupply + 1.
        if (mulmod(numeratorUnits, MathLib.RAY, scaledSupply) == 0) --maximumIndex;
        return maximumIndex;
    }

    function _debtAtIndex(uint256 scaledAmount, uint256 index) private pure returns (uint256) {
        return MathLib.mulDivUp(scaledAmount, index, MathLib.RAY);
    }

    function _supplyAtIndex(uint256 scaledAmount, uint256 index) private pure returns (uint256) {
        return MathLib.mulDivDown(scaledAmount, index, MathLib.RAY);
    }
}

// forge-lint: disable-end(reentrancy-events)
// forge-lint: disable-end(reentrancy-no-eth)
