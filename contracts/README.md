# Solidity Protocol

The `contracts/` project contains the authoritative lending protocol and its Foundry test suite. The current market supports MockWETH collateral and MockUSDC liquidity/debt.

## Contract Graph

```text
PriceOracle ──> RiskManager ──> LendingPool
                                  │
                                  ├──> DepositToken
                                  ├──> DebtToken
                                  └──> InterestRateModel

LendingPool + RiskManager ──> LiquidationManager
```

## Contracts

| Contract | Responsibility |
|---|---|
| `MockWETH` | Unrestricted-mint, 18-decimal local collateral token |
| `MockUSDC` | Unrestricted-mint, 6-decimal local liquidity/debt token |
| `PriceOracle` | Asset registration, 8-decimal prices, freshness, publisher rounds |
| `RiskManager` | USD valuation, LTV, Health Factor, liquidation threshold |
| `InterestRateModel` | Kinked utilization, borrow-rate, and liquidity-rate calculation |
| `DepositToken` | Non-transferable scaled USDC supply claim |
| `DebtToken` | Non-transferable scaled USDC performing debt |
| `LendingPool` | Supply, borrow, repay, withdraw, interest, reserves, liquidation accounting |
| `LiquidationManager` | Unhealthy-position validation and capped liquidation calculation |

Financial parameters and rounding rules are documented in [Protocol](../docs/PROTOCOL.md).

## Roles

- `PriceOracle.DEFAULT_ADMIN_ROLE` registers assets and can set direct development prices.
- `PriceOracle.PUBLISHER_ROLE` authorizes monotonic aggregated publications.
- `LendingPool.LIQUIDATION_ROLE` authorizes liquidation accounting.
- Local deployment grants `LIQUIDATION_ROLE` to `LiquidationManager`.

## User Operations

```solidity
supply(address asset, uint256 amount)
borrow(address asset, uint256 amount)
repay(address asset, uint256 amount)
withdraw(address asset, uint256 amount)
```

Users approve MockWETH or MockUSDC in their wallet and call `LendingPool` directly. The C++ API does not relay these operations.

## Liquidation

Liquidators call:

```solidity
liquidate(
    address borrower,
    address debtAsset,
    address collateralAsset,
    uint256 requestedRepay,
    uint256 minCollateralOut
)
```

`LiquidationManager` revalidates the on-chain position, applies the close factor and bonus, and calls the role-protected pool execution path. Residual debt is recognized only when the final collateral is exhausted.

## Events

The Indexer reconstructs state from:

```text
AssetRegistered
PriceUpdated
Supplied
Withdrawn
Borrowed
Repaid
InterestAccrued
Liquidated
BadDebtRecognized
```

Changes to event signatures require matching changes in the C++ ABI registry, decoder, Indexer projection, and tests.

## Security Properties

- OpenZeppelin `AccessControl`, `SafeERC20`, and `ReentrancyGuard` are used at their relevant boundaries.
- Token transfers require exact balance deltas.
- Risk-increasing operations require fresh prices.
- Solidity makes the final borrow, withdrawal, and liquidation decision.
- Position tokens can only be minted or burned by the pool and cannot be transferred.

See [Security](../docs/SECURITY.md).

## Build and Test

```bash
cd contracts
forge build
forge test
forge fmt --check
```

The suite contains unit, integration, fuzz, invariant, golden-vector, rounding, reentrancy, and abnormal-token tests.

## Local Deployment

`scripts/deploy-local.sh` deploys the contract graph, registers both assets, initializes prices, grants operational roles, funds the local liquidator, and writes a protected environment file. It uses Anvil's public default development key and is suitable only for disposable local chains.
