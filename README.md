# Distributed DeFi Lending Protocol

[简体中文](README.zh-CN.md)

A Solidity-based overcollateralized lending protocol with C++20 services for indexing, risk scanning, transaction management, automated liquidation, and a PostgreSQL-backed REST API. A Go Oracle Coordinator publishes aggregated prices with leader election and database fencing. The complete stack runs through Docker Compose or a local kind cluster with purpose-specific RPC failover.

## Project Structure

```text
DeFi/
├── contracts/
│   ├── foundry.toml
│   ├── lib/
│   │   └── openzeppelin-contracts/
│   ├── src/
│   │   ├── InterestRateModel.sol
│   │   ├── LendingPool.sol
│   │   ├── LiquidationManager.sol
│   │   ├── PriceOracle.sol
│   │   ├── RiskManager.sol
│   │   ├── interfaces/
│   │   │   └── IIndexProvider.sol
│   │   ├── libraries/
│   │   │   └── MathLib.sol
│   │   ├── mocks/
│   │   │   ├── MockUSDC.sol
│   │   │   └── MockWETH.sol
│   │   └── tokens/
│   │       ├── DebtToken.sol
│   │       └── DepositToken.sol
│   └── test/
│       ├── DebtToken.t.sol
│       ├── DepositToken.t.sol
│       ├── InterestAccounting.t.sol
│       ├── InterestFuzz.t.sol
│       ├── InterestGolden.t.sol
│       ├── InterestRateModel.t.sol
│       ├── LendingPool.t.sol
│       ├── LendingPoolIntegration.t.sol
│       ├── LendingMvpFuzz.t.sol
│       ├── LendingMvpInvariant.t.sol
│       ├── LiquidationGolden.t.sol
│       ├── LiquidationManager.t.sol
│       ├── MathLibGolden.t.sol
│       ├── MathLib.t.sol
│       ├── MockUSDC.t.sol
│       ├── MockWETH.t.sol
│       ├── PriceOracle.t.sol
│       ├── RiskManagerGolden.t.sol
│       ├── RiskManager.t.sol
│       └── Smoke.t.sol
├── cpp/
│   ├── common/
│   │   ├── include/dlp/ethereum/
│   │   │   ├── Abi.hpp
│   │   │   ├── Address.hpp
│   │   │   ├── Hex.hpp
│   │   │   ├── Keccak.hpp
│   │   │   ├── ProtocolAbi.hpp
│   │   │   ├── RpcClient.hpp
│   │   │   ├── RpcEndpoints.hpp
│   │   │   ├── Uint256.hpp
│   │   │   └── Uint256Math.hpp
│   │   ├── smoke/
│   │   │   └── Smoke.cpp
│   │   ├── src/
│   │   │   ├── Abi.cpp
│   │   │   ├── Address.cpp
│   │   │   ├── Hex.cpp
│   │   │   ├── Keccak.cpp
│   │   │   ├── ProtocolAbi.cpp
│   │   │   ├── RpcClient.cpp
│   │   │   ├── RpcEndpoints.cpp
│   │   │   ├── Uint256.cpp
│   │   │   └── Uint256Math.cpp
│   │   ├── tests/
│   │   │   ├── AbiTests.cpp
│   │   │   ├── AddressTests.cpp
│   │   │   ├── HexTests.cpp
│   │   │   ├── KeccakTests.cpp
│   │   │   ├── ProtocolAbiTests.cpp
│   │   │   ├── RpcClientIntegrationTests.cpp
│   │   │   ├── RpcEndpointsTests.cpp
│   │   │   ├── Uint256Tests.cpp
│   │   │   └── Uint256MathTests.cpp
│   │   └── CMakeLists.txt
│   ├── messaging/
│   │   ├── include/dlp/messaging/
│   │   ├── src/
│   │   └── tests/
│   ├── indexer/
│   │   ├── include/dlp/indexer/
│   │   ├── src/
│   │   ├── tests/
│   │   └── CMakeLists.txt
│   ├── risk-engine/
│   │   ├── include/dlp/risk/
│   │   ├── src/
│   │   └── tests/
│   ├── tx-manager/
│   │   ├── include/dlp/tx/
│   │   ├── src/
│   │   └── tests/
│   ├── liquidator/
│   │   ├── include/dlp/liquidator/
│   │   ├── src/
│   │   └── tests/
│   └── api-server/
│       ├── include/dlp/api/
│       ├── src/
│       └── tests/
├── database/
│   └── migrations/
│       ├── 001_create_blocks.sql
│       ├── 002_create_raw_logs.sql
│       ├── 003_create_sync_state.sql
│       ├── 004_create_positions.sql
│       ├── 005_create_markets.sql
│       ├── 006_create_liquidations.sql
│       ├── 007_create_tx_jobs.sql
│       ├── 008_create_outbox_events.sql
│       ├── 009_create_liquidation_jobs.sql
│       └── 010_create_oracle_publications.sql
├── go/
│   └── oracle-coordinator/
│       ├── cmd/oracle-coordinator/
│       ├── internal/oracle/
│       ├── go.mod
│       └── go.sum
├── infrastructure/
│   └── rpc-proxy/
├── k8s/
│   ├── applications.yaml
│   ├── infrastructure.yaml
│   ├── kind-config.yaml
│   └── migrations-job.yaml
├── scripts/
│   ├── create-liquidation-scenario.sh
│   ├── deploy-local.sh
│   ├── run-containers.sh
│   ├── run-kind.sh
│   ├── run-local.sh
│   └── scale-kind.sh
├── tests/
│   └── golden/
│       └── risk_vectors.json
├── .gitignore
├── CMakeLists.txt
├── Dockerfile
├── compose.apps.yaml
├── compose.yaml
├── README.md
└── README.zh-CN.md
```

## Prerequisites

- macOS or Linux
- Bash or Zsh
- `curl`
- A C++20 compiler and CMake 3.20 or newer
- Go 1.25 or newer
- Boost 1.74 or newer, nlohmann/json 3.10 or newer, GoogleTest, libpq, and libpqxx 8
- Docker with Docker Compose
- `kubectl` and kind for the Kubernetes deployment
- Foundry with Anvil for local deployment and RPC integration
- An internet connection for installing Foundry, downloading Solc, and fetching the pinned Ethereum Keccak dependency on the first CMake configuration

## Install Foundry

```bash
curl -L https://getfoundry.sh/install | bash
export PATH="$PATH:$HOME/.foundry/bin" # forge: command not found
foundryup

# Add Foundry to the PATH for future Zsh sessions:
echo 'export PATH="$PATH:$HOME/.foundry/bin"' >> ~/.zshrc
source ~/.zshrc # To make it persistent, add the same line to `~/.zshrc`, then reload the configuration
```

## Install C++ Dependencies

On macOS with Homebrew:

```bash
brew install cmake boost nlohmann-json googletest libpq libpqxx
```

## Verify the Toolchain

```bash
# Verify the Forge installation: 
forge --version
# Verify the C++ toolchain:
cmake --version
c++ --version
# Verify Anvil before running the RPC integration test:
anvil --version
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

### Solidity

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

### C++

From the repository root:

```bash
# Configure and build the C++ targets:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
# Run the local C++ tests:
ctest --test-dir build --output-on-failure
```

The PostgreSQL integration test requires the local database:

```bash
DLP_POSTGRES_PORT=5433 docker compose up -d postgres

DLP_TEST_DATABASE_URL=postgresql://dlp:dlp@127.0.0.1:5433/dlp \
ctest --test-dir build --output-on-failure \
-R PostgresStoreIntegrationTests
```

The protocol RPC integration test requires Anvil and deployed contracts. After running the local stack, load the generated environment in another terminal:

```bash
source .env.local

ctest --test-dir build --output-on-failure \
-R RpcChainClientIntegrationTests
```

### Go

```bash
cd go/oracle-coordinator
go mod tidy
go test ./...
```

## Run Locally

After building the C++ targets, start PostgreSQL, Anvil, deploy the contracts, and run the Indexer, Liquidator, and API Server with one command:

```bash
./scripts/run-local.sh
```

The local runner uses PostgreSQL port `5433`, Anvil port `8546`, and API port `18080` by default. Contract addresses are written to the ignored `.env.local` file. When the current RPC and deployed contracts can be reused, their indexed database state is preserved. A new chain deployment automatically removes the old PostgreSQL volume so stale indexed state and transaction nonces cannot leak into the new chain.

To force removal of the local PostgreSQL volume before startup:

```bash
./scripts/run-local.sh --clean
```

Keep the runner open and create a complete liquidation scenario from another terminal:

```bash
./scripts/create-liquidation-scenario.sh

curl -sS http://127.0.0.1:18080/markets
curl -sS http://127.0.0.1:18080/liquidations
curl -sS http://127.0.0.1:18080/protocol/stats
```

After the Indexer catches up, the scenario produces five partial liquidations, exhausts the borrower's collateral, and records the remaining debt as bad debt. Press `Ctrl+C` to stop the C++ services and the Anvil process started by the runner. The PostgreSQL container remains available until the next reset.

## Run with kind

Build the service images, create a fresh kind cluster, deploy the contracts, run the database migrations, and start the complete stack:

```bash
./scripts/run-kind.sh --clean
```

The API is available at `http://127.0.0.1:18080`. A run interrupted by a slow image pull can continue without deleting the cluster:

```bash
./scripts/run-kind.sh
```

Inspect the deployment and the elected Oracle leader with:

```bash
kubectl --context kind-dlp get pods -n dlp
kubectl --context kind-dlp get lease oracle-coordinator -n dlp
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

An 8-decimal price oracle with asset registration, stale-price validation, administrator updates, and role-controlled aggregated publications. Published reports must use a fresh timestamp and a strictly increasing round ID.

Files:

```text
contracts/src/PriceOracle.sol
contracts/test/PriceOracle.t.sol
```

### Fixed-Point Math

Shared WAD, RAY, and BPS math with full-precision multiplication and division, explicit rounding, and USD valuation for 6- and 18-decimal tokens.

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

Supports USDC liquidity supply, WETH collateral, USDC borrowing and repayment, and safe withdrawals. The pool enforces available liquidity, borrowing capacity, health factor, minimum debt, and stale-price rules while maintaining indexed supply, debt, and reserve accounting.

An integration test covers the complete supply, borrow, repay, and collateral withdrawal lifecycle.

Files:

```text
contracts/src/LendingPool.sol
contracts/test/LendingPool.t.sol
contracts/test/LendingPoolIntegration.t.sol
```

### Interest and Indexed Positions

USDC deposits and debts are represented by non-transferable, pool-controlled scaled tokens. Interest is accrued lazily before market state changes through borrow and liquidity indices.

The kinked rate model uses an 80% optimal utilization rate, a 2% base rate, an 8% first slope, a 100% second slope, and a 10% reserve factor. Supplier interest is capped at the borrower interest available for distribution so integer rounding cannot break the pool's accounting identity.

Golden vectors, fuzz tests, stateful invariants, and a rounding-boundary regression test cover rates, indices, reserves, and interest-bearing operations.

Files:

```text
contracts/src/InterestRateModel.sol
contracts/src/interfaces/IIndexProvider.sol
contracts/src/tokens/DepositToken.sol
contracts/src/tokens/DebtToken.sol
contracts/test/DepositToken.t.sol
contracts/test/DebtToken.t.sol
contracts/test/InterestRateModel.t.sol
contracts/test/InterestAccounting.t.sol
contracts/test/InterestFuzz.t.sol
contracts/test/InterestGolden.t.sol
contracts/test/LendingMvpInvariant.t.sol
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

### C++ Ethereum Common Layer

The modern C++20 common layer provides strongly typed Ethereum addresses and checked 256-bit integers, strict hexadecimal conversion, Ethereum-compatible Keccak-256, and fixed-type ABI encoding and event decoding for the current protocol.

Its synchronous HTTP JSON-RPC client uses Boost.Asio and Boost.Beast with structured errors and typed responses. It supports `eth_chainId`, `eth_blockNumber`, `eth_getBlockByNumber`, and `eth_getLogs`, with the complete path verified against Anvil.

Files:

```text
CMakeLists.txt
cpp/common/CMakeLists.txt
cpp/common/include/dlp/ethereum/
cpp/common/src/
cpp/common/smoke/
cpp/common/tests/
```

### PostgreSQL and Reorg-Aware Indexer

The C++20 Indexer polls Ethereum blocks and protocol logs, decodes the registered ABI events, and reconstructs positions, market accounting, prices, liquidations, and bad debt in PostgreSQL. Each block, its raw logs, the derived state, and the sync cursor are committed atomically.

Restart recovery validates the stored canonical block hash. Parent-hash mismatches trigger common-ancestor discovery, orphan marking, and deterministic state reconstruction from canonical logs while retaining orphaned history.

Files:

```text
compose.yaml
database/migrations/
cpp/indexer/
scripts/deploy-local.sh
scripts/run-local.sh
```

### C++ Risk Engine

The Risk Engine reads indexed PostgreSQL positions and market state, reproduces the Solidity collateral, debt, health-factor, and liquidation calculations with checked integer arithmetic, and scans large position sets for actionable candidates.

Files:

```text
cpp/risk-engine/
tests/golden/risk_vectors.json
```

### Transactional Messaging and Leased Liquidation

PostgreSQL transactional outbox records are published to NATS JetStream and consumed idempotently. Liquidation jobs use database leases and fencing tokens so multiple Liquidator replicas can safely claim work. The persistent Transaction Manager signs EIP-1559 transactions, recovers nonces, replaces stale submissions, and records finality or reorgs.

Files:

```text
cpp/tx-manager/
cpp/liquidator/
cpp/messaging/
database/migrations/007_create_tx_jobs.sql
database/migrations/008_create_outbox_events.sql
database/migrations/009_create_liquidation_jobs.sql
scripts/create-liquidation-scenario.sh
```

### REST API

The Boost.Beast API Server exposes markets, positions, health factors, liquidation history, protocol statistics, and deterministic risk simulation. Read responses include the indexed block, observed chain head, and index lag.

Endpoints:

```text
GET  /markets
GET  /positions/:address
GET  /positions/:address/health
GET  /liquidations
GET  /protocol/stats
POST /risk/simulate
```

Files:

```text
cpp/api-server/
```

### Containers and Kubernetes

Multi-stage images package the C++ and Go services. Docker Compose provides the containerized local stack, while kind runs PostgreSQL, JetStream, Anvil, two RPC proxy endpoints, all application services, and replicated workers through Kubernetes manifests.

Files:

```text
Dockerfile
compose.apps.yaml
k8s/
scripts/run-containers.sh
scripts/run-kind.sh
scripts/scale-kind.sh
```

### Highly Available Oracle and RPC Routing

Three simulated price providers are aggregated by median. Three Go Oracle Coordinator replicas use a Kubernetes Lease to elect one publisher, while PostgreSQL fencing, monotonic round IDs, and idempotent transaction jobs protect publication. The private key remains isolated in the C++ Transaction Manager.

RPC routing follows request semantics: the Indexer uses an active/failover pair with chain and canonical-block verification, the API round-robins read requests, and the Transaction Manager obtains pending nonces from the preferred primary while broadcasting the same signed payload to both endpoints.

Files:

```text
go/oracle-coordinator/
database/migrations/010_create_oracle_publications.sql
infrastructure/rpc-proxy/
cpp/common/include/dlp/ethereum/RpcEndpoints.hpp
```
