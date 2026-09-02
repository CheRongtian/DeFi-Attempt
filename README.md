# Distributed DeFi Lending Protocol

A Solidity-based overcollateralized lending protocol. The smart contracts are built and tested with Foundry.

## Prerequisites

- macOS or Linux
- Bash or Zsh
- `curl`
- An internet connection for installing Foundry and downloading the required Solidity compiler on the first run

## Install Foundry

```bash
curl -L https://getfoundry.sh/install | bash
export PATH="$PATH:$HOME/.foundry/bin"
foundryup
```

Add Foundry to the PATH for future Zsh sessions:

```bash
echo 'export PATH="$PATH:$HOME/.foundry/bin"' >> ~/.zshrc
source ~/.zshrc
```

## Verify the Toolchain

Verify the Forge installation:

```bash
forge --version
```

## Current Solidity Project Layout

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

Enter the Solidity project directory:

```bash
cd contracts
```

Build the contracts:

```bash
forge build
```

Run the tests:

```bash
forge test
```

Current expected result:

```text
1 test passed
0 failed
0 skipped
```

On the first run, Foundry may automatically download Solc 0.8.36 for `Smoke.t.sol`. Compilation and testing continue after the download finishes.

Check Solidity formatting:

```bash
forge fmt --check
```

Remove Foundry-generated `out/` and `cache/` artifacts:

```bash
forge clean
```

## Troubleshooting

### `forge: command not found`

The current shell has not loaded the Foundry path. Run:

```bash
export PATH="$PATH:$HOME/.foundry/bin"
```

To make it persistent, add the same line to `~/.zshrc`, then reload the configuration:

```bash
source ~/.zshrc
```

### `foundry.toml` reports `expected a map`

Make sure the configuration fields are under `[profile.default]`:

```toml
[profile.default]
src = "src"
test = "test"
out = "out"
libs = ["lib"]
```

### `No tests found in project`

Forge discovers test functions whose names begin with `test`, for example:

```solidity
function testSmoke() public pure {
    assert(1 + 1 == 2);
}
```

## Implemented

```text
Foundry project configuration
Solidity compilation setup
SmokeTest basic test
```
