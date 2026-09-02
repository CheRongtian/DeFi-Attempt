# Distributed DeFi Lending Protocol

A Solidity-based overcollateralized lending protocol. The smart contracts are built and tested with Foundry.

## Project Structure

```text
DeFi/
├── contracts/
│   ├── foundry.toml
│   ├── lib/
│   │   └── openzeppelin-contracts/
│   ├── src/
│   │   └── mocks/
│   │       └── MockUSDC.sol
│   └── test/
│       ├── MockUSDC.t.sol
│       └── Smoke.t.sol
├── .gitignore
└── README.md
```

## 0. Prerequisites
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
## Prerequisities Structure
```text
contracts/
├── foundry.toml
├── src/
└── test/
    └── Smoke.t.sol
```

The Foundry configuration is located at `contracts/foundry.toml`:

```toml
[profile.default]
src = "src"
test = "test"
out = "out"
libs = ["lib"]
```

## Build and Test

```bash
# Enter the Solidity project directory:
cd contracts
# Build the contracts:
forge build
# Run the tests:
forge test
```

Current expected result:

```text
1 test passed
0 failed
0 skipped
```

On the first run, Foundry may automatically download Solc 0.8.36 for `Smoke.t.sol`. Compilation and testing continue after the download finishes.

```bash
# Check Solidity formatting:
forge fmt --check
# Remove Foundry-generated `out/` and `cache/` artifacts:
forge clean
```

## Test
Use Example:

```solidity
function testSmoke() public pure {
    assert(1 + 1 == 2);
}
```

## 1. Mock USDC

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

Build and test:

```bash
cd contracts
forge build
forge test --match-contract MockUSDCTest -vv
```
