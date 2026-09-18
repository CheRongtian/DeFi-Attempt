# Distributed DeFi Lending Protocol

[简体中文](README.zh-CN.md)

A distributed overcollateralized lending protocol built with Solidity, C++20, Go, PostgreSQL, NATS JetStream, Kubernetes, Prometheus, Grafana, and React.

The project combines an authoritative on-chain lending protocol with fault-tolerant off-chain infrastructure for indexing, deterministic risk analysis, automated liquidation, transaction management, Oracle coordination, recovery, and observability.

> Portfolio prototype demonstrated locally with Anvil and kind. Sepolia support is limited to read-only RPC connectivity. The protocol has not been audited and must not be used with real funds.

## Architecture

```text
Frontend ── queries ──> C++ API ── reads ──> PostgreSQL
    │                         │                    ▲
    │                         └── chain head ──> Ethereum RPC
    │                                              ▲
    └── User Wallet ── transactions ──> Solidity Protocol
                                              │
                                              └── logs/blocks ──> Ethereum RPC
                                                                     │
                                                                     ▼
                                                                  Indexer
                                                                     │
                     PostgreSQL <────────────────────────────────────┘
                          │
                          └──> Risk Engine ──> Outbox ──> NATS JetStream
                                                                  │
                                                                  ▼
Liquidator ── leased job ──> tx_jobs ──> Tx Manager ──> Ethereum RPC

Go Oracle Coordinator ── fenced publication ──> tx_jobs ──> Tx Manager
```

User transactions always follow:

```text
Frontend → User Wallet → Solidity
```

Solidity is the authoritative financial state. PostgreSQL contains rebuildable indexed and operational state; off-chain services cannot authorize an invalid borrow, withdrawal, or liquidation.

## Highlights

- Overcollateralized MockWETH / MockUSDC lending with WETH/USDC-compatible decimals
- Supply, borrow, repay, withdraw, indexed interest accrual, liquidation, and bad-debt accounting
- Fixed-point WAD, RAY, and BPS math with explicit rounding
- Solidity and C++ deterministic risk calculations checked against shared golden vectors
- Reorg-aware C++20 blockchain indexer and deterministic state reconstruction
- Persistent EIP-1559 Transaction Manager with nonce recovery and replacement
- Transactional Outbox and NATS JetStream at-least-once delivery
- Idempotent consumers, database leases, and fencing tokens
- Horizontally scalable Liquidator workers
- Go Oracle coordination with median aggregation over configurable demo providers
- Kubernetes leader election, RPC failover, and signed-transaction broadcast fan-out
- Prometheus metrics, Grafana dashboards, and automated recovery tests
- React wallet-based protocol console
- Optional host-native Apple Metal European option analytics

## Protocol

The local MVP market uses `MockWETH` and `MockUSDC`:

| Asset | Supply | Collateral | Borrow | Decimals |
|---|---:|---:|---:|---:|
| MockWETH | Yes | Yes | No | 18 |
| MockUSDC | Yes | No | Yes | 6 |

Core parameters are fixed in the contracts:

| Parameter | Value |
|---|---:|
| WETH loan-to-value | 75% |
| WETH liquidation threshold | 80% |
| Liquidation close factor | 50% |
| Liquidation bonus | 5% |
| Reserve factor | 10% |
| Minimum USDC debt | 1 USDC |

Interest-bearing USDC supply and debt are represented by non-transferable scaled tokens. Borrow and liquidity indices accrue lazily before market state changes. If liquidation exhausts all collateral, the remaining debt is explicitly recognized as bad debt.

## Distributed System

The off-chain system handles failures without creating a second source of financial truth:

```text
Ethereum reorg detection and common-ancestor recovery
Canonical log replay and state reconstruction
Transactional Outbox and JetStream delivery
Idempotent event consumption
Liquidation job leases and fencing tokens
Oracle leader election and database fencing
Persistent nonce allocation and transaction replacement
RPC read failover and broadcast fan-out
Crash recovery and Kubernetes reconciliation
```

Ethereum provides consensus. C++ and Go provide fault-tolerant operational infrastructure around the Solidity state machine.

## Demo

The complete local environment runs with kind and Anvil. A clean automated liquidation scenario follows:

```text
USDC liquidity is supplied
→ Alice supplies MockWETH
→ Alice borrows MockUSDC
→ the WETH price falls
→ the Risk Engine emits a liquidation candidate
→ a Liquidator claims the fenced job
→ the Tx Manager signs and broadcasts the approved call
→ Solidity validates and executes the liquidation
→ the Indexer rebuilds the derived state
→ the API, Frontend, Prometheus, and Grafana reflect the result
```

Wallet-based supply, borrow, repay, and withdraw can also be demonstrated interactively in the Frontend. Recovery tooling covers Pod crashes, PostgreSQL and JetStream restarts, RPC failure, leader and lease takeover, transaction reorgs, and deep chain reorganization.

## Quick Start

The complete launcher currently targets macOS because it starts the optional Apple Metal service. Install the prerequisites from [Development](docs/DEVELOPMENT.md), then create the read-only Sepolia RPC configuration:

```bash
cp .env.sepolia.example .env.sepolia
chmod 600 .env.sepolia
```

Fill the three endpoint values in `.env.sepolia`, then start a fresh local demonstration:

```bash
./scripts/run-final-demo.sh --clean
```

Reuse the existing cluster on later runs:

```bash
./scripts/run-final-demo.sh
```

The launcher validates Sepolia RPC connectivity, starts the local kind/Anvil system, prepares demo accounts, starts the Metal service, and serves the Frontend. It does not deploy to Sepolia or send a Sepolia transaction.

| Service | Local URL |
|---|---|
| Frontend | `http://127.0.0.1:4173` |
| API | `http://127.0.0.1:18080` |
| Metal health | `http://127.0.0.1:18081/health` |
| Prometheus | `http://127.0.0.1:19090` |
| Grafana | `http://127.0.0.1:13000/d/dlp-overview` |

See [Operations](docs/OPERATIONS.md) for native, Docker Compose, kind, Frontend-only, and cleanup workflows.

## Validation

The repository includes:

- Foundry unit, integration, fuzz, and stateful invariant tests
- Solidity/C++ shared golden-vector tests for risk, liquidation, and interest calculations
- C++20 unit, PostgreSQL integration, RPC integration, and reorg tests
- Go aggregation, publication, idempotency, and fencing tests
- React component, configuration, wallet-state, and API-state tests
- Automated Kubernetes recovery and observability acceptance suites

Commands and required services are documented in [Testing](docs/TESTING.md).

## Documentation

- [Architecture](docs/ARCHITECTURE.md)
- [Protocol and financial rules](docs/PROTOCOL.md)
- [Distributed system design](docs/DISTRIBUTED_SYSTEM.md)
- [Development environment](docs/DEVELOPMENT.md)
- [Operations](docs/OPERATIONS.md)
- [Testing](docs/TESTING.md)
- [Security boundaries](docs/SECURITY.md)
- [Demo guide](docs/DEMO.md)

Module documentation:

- [Solidity contracts](contracts/README.md)
- [C++20 services](cpp/README.md)
- [Go Oracle Coordinator](go/oracle-coordinator/README.md)
- [Frontend](frontend/README.md)
- [Kubernetes](k8s/README.md)
- [Observability](observability/README.md)
- [Apple Metal option pricer](tools/metal-option-pricer/README.md)

## Repository Layout

```text
contracts/       Solidity protocol and Foundry tests
cpp/             C++20 common libraries and off-chain services
go/              Go Oracle Coordinator
frontend/        React protocol console
database/        PostgreSQL migrations
infrastructure/  Local RPC proxy configuration
k8s/             kind and Kubernetes manifests
observability/   Prometheus and Grafana configuration
scripts/         Build, launch, demo, and acceptance automation
tests/           Shared cross-language golden vectors
tools/           Optional host-native analytics
docs/            Detailed project documentation
```

## Security Boundaries

- Solidity performs the final risk, withdrawal, interest, liquidation, and bad-debt checks.
- Frontend and API services do not hold user private keys or sign user transactions.
- The Tx Manager signs only zero-value, approved Oracle and Liquidation Manager calls.
- The operational signer is isolated from non-signing services.
- PostgreSQL is indexed and rebuildable, never authoritative financial state.
- The pool requires exact-transfer token semantics; fee-on-transfer and rebasing tokens are unsupported.
- Generated environment files and secrets are excluded from version control.
- Checked-in Anvil accounts and local database credentials are disposable development values.

See [Security](docs/SECURITY.md) for the complete trust model and production limitations.

## Scope

This project demonstrates protocol accounting, blockchain indexing, deterministic risk analysis, event-driven processing, transaction management, reorg recovery, leases, fencing, leader election, Kubernetes operations, and observability.

It does not claim mainnet readiness, an external audit, formal verification, production Oracle guarantees, institutional key custody, or real-fund safety.
