# Protocol and Financial Rules

## Market

The local MVP contains one market with two unrestricted-mint test tokens:

| Token | Role | Decimals |
|---|---|---:|
| `MockWETH` (`mWETH`) | Supplied collateral | 18 |
| `MockUSDC` (`mUSDC`) | Supplied liquidity and borrowed debt | 6 |

MockWETH does not wrap or unwrap native ETH. Both assets are development-only contracts.

## Fixed-Point Units

| Unit | Scale | Use |
|---|---:|---|
| WAD | `1e18` | USD values and Health Factor |
| RAY | `1e27` | Interest rates and indices |
| BPS | `1e4` | Percent parameters |
| Oracle price | `1e8` | USD price per whole token |

Collateral valuation rounds down. Debt valuation and scaled debt minting use conservative upward rounding where required. `MathLib` uses full-precision multiplication and division for values that require a reduced intermediate product.

## Risk Parameters

| Parameter | Contract value |
|---|---:|
| WETH LTV | 7,500 BPS (75%) |
| WETH liquidation threshold | 8,000 BPS (80%) |
| Liquidation close factor | 5,000 BPS (50%) |
| Liquidation bonus | 500 BPS (5%) |
| Reserve factor | 1,000 BPS (10%) |
| Minimum USDC debt | `1e6` base units (1 USDC) |

Maximum borrow value:

```text
maxBorrow = collateralValue × 75%
```

Health Factor:

```text
healthFactor = collateralValue × 80% / debtValue
```

A zero-debt position has the maximum `uint256` Health Factor. A position becomes liquidatable below one WAD.

## Oracle Rules

`PriceOracle` stores an 8-decimal price, update timestamp, maximum age, registration state, and latest publication round for each asset.

- Unregistered assets are rejected.
- Zero prices are rejected.
- Risk-sensitive reads reject missing or stale prices.
- Administrative `setPrice` is restricted to the default admin role.
- Aggregated `publishPrice` requires `PUBLISHER_ROLE`, a valid report timestamp, and a strictly increasing round ID.
- Supply and repayment remain available without a fresh price because they do not increase protocol risk.

The local deployment registers MockWETH and MockUSDC with a one-day maximum price age and initializes them to `$3,000` and `$1`.

## Supply

`supply(asset, amount)` accepts MockWETH collateral or MockUSDC liquidity.

- WETH increases the caller's collateral and total collateral.
- USDC mints a scaled `DepositToken` balance at the current liquidity index.
- Zero amounts and unsupported assets revert.
- Exact before/after token balance deltas are required.

The scaled token balance remains constant between state changes while its reported balance grows with the projected liquidity index.

## Borrow

`borrow(MockUSDC, amount)`:

1. Accrues market interest.
2. Checks the one-USDC minimum.
3. Checks available liquidity.
4. Converts the requested amount to scaled debt with upward rounding.
5. Enforces LTV and Health Factor using fresh prices.
6. Mints scaled `DebtToken` units and transfers exact USDC.

WETH cannot be borrowed in the current market.

## Repay

`repay(MockUSDC, amount)` caps repayment at the caller's current debt. It burns scaled debt, increases available liquidity, and accounts for rounding differences in the protocol reserve.

Repayment does not require fresh prices. A partial repayment cannot leave a non-zero debt below one USDC.

## Withdraw

USDC suppliers can withdraw up to their indexed claim and the pool's available liquidity. WETH collateral withdrawal is allowed only when the remaining collateral keeps all outstanding debt healthy. The contracts perform the final validation regardless of Frontend estimates.

## Interest Model

The kink model uses annualized RAY rates:

| Parameter | Value |
|---|---:|
| Optimal utilization | 80% |
| Base borrow rate | 2% |
| Slope below kink | 8% |
| Slope above kink | 100% |

```text
utilization = debt / (available liquidity + debt)
liquidityRate = borrowRate × utilization × (1 - reserveFactor)
```

`LendingPool` accrues linear index growth before market mutations. Borrower interest is split between suppliers and protocol reserves, with integer rounding constrained so pool accounting remains consistent.

## Liquidation

`LiquidationManager` re-reads debt, collateral, prices, and Health Factor on chain.

- Healthy or debt-free positions are rejected.
- Repayment is capped by the caller request, the 50% close factor, and available collateral.
- Seized collateral includes the 5% bonus.
- `minCollateralOut` protects the liquidator from an unacceptable result.
- Debt dust below one USDC is resolved without leaving an invalid residual position.
- A collateral-limited liquidation consumes the final collateral rounding remainder.

Only the configured `LiquidationManager` has the pool's `LIQUIDATION_ROLE`. Off-chain candidates cannot bypass these checks.

## Bad Debt

When liquidation exhausts a borrower's collateral, `LendingPool` burns the remaining scaled debt and records the residual amount in `badDebt`. The position has no performing debt afterward, so the same residual cannot be recognized repeatedly.

Bad debt remains explicit protocol accounting; the MVP does not add an insurance fund or socialized-loss mechanism.

## Position Tokens

`DepositToken` and `DebtToken` are pool-owned accounting tokens:

- Only `LendingPool` can mint or burn scaled balances.
- User-visible balances apply the latest projected index.
- Transfers, approvals, and `transferFrom` are disabled.
- Both tokens use six display decimals to match USDC.

## Token Compatibility

OpenZeppelin `SafeERC20` handles tokens that omit a Boolean return value. The pool additionally checks exact sender, receiver, and pool balance deltas. Fee-on-transfer, rebasing, and other non-exact transfer semantics are intentionally unsupported.
