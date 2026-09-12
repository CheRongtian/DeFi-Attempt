# Distributed DeFi Lending Protocol

[简体中文](README.zh-CN.md)

A Solidity-based overcollateralized lending protocol. The smart contracts are built and tested with Foundry.

## Project Structure

```text
DeFi/
├── contracts/
│   ├── foundry.toml
│   ├── lib/
│   │   └── openzeppelin-contracts/
│   ├── src/
│   │   ├── LendingPool.sol
│   │   ├── LiquidationManager.sol
│   │   ├── PriceOracle.sol
│   │   ├── RiskManager.sol
│   │   ├── libraries/
│   │   │   └── MathLib.sol
│   │   └── mocks/
│   │       ├── MockUSDC.sol
│   │       └── MockWETH.sol
│   └── test/
│       ├── LendingPool.t.sol
│       ├── LendingPoolIntegration.t.sol
│       ├── LendingMvpFuzz.t.sol
│       ├── LendingMvpInvariant.t.sol
│       ├── LiquidationManager.t.sol
│       ├── MathLibGolden.t.sol
│       ├── MathLib.t.sol
│       ├── MockUSDC.t.sol
│       ├── MockWETH.t.sol
│       ├── PriceOracle.t.sol
│       ├── RiskManagerGolden.t.sol
│       ├── RiskManager.t.sol
│       └── Smoke.t.sol
├── tests/
│   └── golden/
│       └── risk_vectors.json
├── .gitignore
├── README.md
└── README.zh-CN.md
```

## Prerequisites

- macOS or Linux
- Bash or Zsh
- `curl`
- An internet connection for installing Foundry and downloading the required Solidity compiler on the first run

## Install Foundry

```bash
curl -L https://getfoundry.sh/install | bash
export PATH="$PATH:$HOME/.foundry/bin" # forge: command not found
foundryup

# Add Foundry to the PATH for future Zsh sessions:
echo 'export PATH="$PATH:$HOME/.foundry/bin"' >> ~/.zshrc
source ~/.zshrc # To make it persistent, add the same line to `~/.zshrc`, then reload the configuration
```

## Verify the Toolchain

```bash
# Verify the Forge installation: 
forge --version
```

## Verify the Setup

```bash
# Enter the Solidity project directory:
cd contracts
# Verify the local Foundry setup:
forge test --match-contract SmokeTest
```

Current expected result:

```text
1 test passed
0 failed
0 skipped
```

On the first run, Foundry may automatically download Solc 0.8.36 for `Smoke.t.sol`. Compilation and testing continue after the download finishes.

## Build and Test

```bash
# Enter the Solidity project directory:
cd contracts
# Build the contracts:
forge build
# Run the tests:
forge test
```

```bash
# Check Solidity formatting:
forge fmt --check
# Remove Foundry-generated `out/` and `cache/` artifacts:
forge clean
```

## Implemented Features

### Mock USDC

A test-only ERC-20 token that simulates USDC using OpenZeppelin Contracts.

```text
Name: Mock USDC
Symbol: mUSDC
Decimals: 6
Minting: unrestricted for local and testnet use
```

Files:

```text
contracts/src/mocks/MockUSDC.sol
contracts/test/MockUSDC.t.sol
```

### Mock WETH

A test-only ERC-20 token used as WETH collateral. It does not implement ETH wrapping or unwrapping.

```text
Name: Mock WETH
Symbol: mWETH
Decimals: 18
Minting: unrestricted for local and testnet use
```

Files:

```text
contracts/src/mocks/MockWETH.sol
contracts/test/MockWETH.t.sol
```

### Price Oracle

An administrator-managed price oracle with 8-decimal prices, asset registration, update timestamps, and stale-price validation.

Files:

```text
contracts/src/PriceOracle.sol
contracts/test/PriceOracle.t.sol
```

### Fixed-Point Math

Shared WAD and BPS math with full-precision multiplication and division, explicit rounding, and USD valuation for 6- and 18-decimal tokens.

Golden vectors provide reusable expected results for Solidity and future off-chain implementations.

Files:

```text
contracts/src/libraries/MathLib.sol
contracts/test/MathLib.t.sol
contracts/test/MathLibGolden.t.sol
tests/golden/risk_vectors.json
```

### Risk Management

Calculates WETH collateral and USDC debt values using a 75% LTV and an 80% liquidation threshold. Health factors use conservative rounding and treat positions without debt as healthy.

Files:

```text
contracts/src/RiskManager.sol
contracts/test/RiskManager.t.sol
```

### Lending Pool

Supports USDC liquidity supply, WETH collateral, USDC borrowing and repayment, and safe withdrawals. The pool enforces available liquidity, borrowing capacity, health factor, minimum debt, and stale-price rules.

An integration test covers the complete supply, borrow, repay, and collateral withdrawal lifecycle.

Files:

```text
contracts/src/LendingPool.sol
contracts/test/LendingPool.t.sol
contracts/test/LendingPoolIntegration.t.sol
```

### Liquidation and Bad Debt

Unhealthy positions with a health factor below 1 can be partially liquidated with a 50% close factor and a 5% liquidation bonus. Repayment caps, minimum collateral output, rounding, debt dust, collateral exhaustion, and one-time bad debt recognition are handled explicitly.

Fuzz tests and stateful invariants verify borrowing and liquidation bounds, token balances, and the pool's USDC accounting identity.

Files:

```text
contracts/src/LiquidationManager.sol
contracts/test/LiquidationManager.t.sol
contracts/test/LendingMvpFuzz.t.sol
contracts/test/LendingMvpInvariant.t.sol
contracts/test/RiskManagerGolden.t.sol
```
