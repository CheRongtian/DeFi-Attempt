# Architecture

## System Boundaries

The project separates financial authority, indexed state, and operational state.

| Layer | Responsibility | Authority |
|---|---|---|
| Solidity protocol | Balances, debt, interest, collateral checks, liquidation, bad debt | Authoritative |
| Ethereum RPC | Canonical blocks, receipts, and logs | Observed chain interface |
| PostgreSQL | Indexed protocol state and operational jobs | Rebuildable / operational |
| C++ and Go services | Indexing, risk discovery, job coordination, signing, APIs | Non-authoritative |
| Frontend | Queries, formatting, wallet interaction | Non-authoritative |

An off-chain calculation can discover or propose an action. The Solidity contracts make the final decision when a transaction executes.

## Component Topology

```text
                                      ┌─────────────────────┐
                                      │      Frontend       │
                                      └───────┬───────┬─────┘
                                              │       │
                                        query │       │ wallet transaction
                                              ▼       ▼
                                       ┌──────────┐  User Wallet
                                       │ C++ API  │       │
                                       └────┬─────┘       ▼
                                            │       Solidity Protocol
                                     ┌──────▼──────┐       ▲
                                     │ PostgreSQL  │       │
                                     └──────▲──────┘       │
                                            │              │
Ethereum RPC ──> Indexer ───────────────────┘              │
       ▲                                                   │
       │           PostgreSQL ──> Risk Engine ──> Outbox   │
       │                                          │        │
       │                                          ▼        │
       │                                   NATS JetStream  │
       │                                          │        │
       │                                          ▼        │
       └── Tx Manager <── tx_jobs <── Liquidator workers ──┘
                ▲
                └── tx_jobs <── Go Oracle Coordinator
```

## On-Chain Components

- `PriceOracle` registers supported assets, stores 8-decimal prices, enforces freshness, and accepts monotonic publisher rounds.
- `RiskManager` converts WETH collateral and USDC debt to USD WAD and applies LTV and liquidation thresholds.
- `LendingPool` owns protocol accounting, indexed deposit/debt tokens, liquidity, reserves, collateral, and bad debt.
- `LiquidationManager` revalidates unhealthy positions and calculates close-factor, bonus, collateral, and bad-debt outcomes.
- `DepositToken` and `DebtToken` store scaled, non-transferable balances owned by the pool.

See [Protocol](PROTOCOL.md) and [contracts/README.md](../contracts/README.md).

## Off-Chain Components

### Indexer

The Indexer reads blocks and registered protocol logs, stores raw canonical history, and applies events to derived market, position, price, and liquidation tables. A parent-hash mismatch triggers common-ancestor discovery and canonical replay.

### Risk Engine

The Risk Engine reads indexed positions and market data, reproduces contract-compatible integer risk calculations, and writes liquidation candidates through the transactional outbox. It discovers candidates; it cannot liquidate a healthy position on chain.

### Messaging and Liquidator

The Outbox Publisher moves committed database events to JetStream. Liquidator workers consume events idempotently, revalidate candidates against current chain data, acquire fenced database jobs, and create approved transaction jobs.

### Transaction Manager

The Tx Manager is the only C++ service that receives the operational private key. It allocates nonces, signs EIP-1559 transactions, broadcasts the same signed payload to configured RPC endpoints, replaces stale submissions, and tracks inclusion, finality, and reorg state. Its approval policy limits destinations and calldata to Oracle publication and liquidation.

### Oracle Coordinator

Three Go replicas use a Kubernetes Lease to elect one active coordinator. The active replica takes the median of configurable demo prices, acquires a database fencing token, assigns a monotonic round, and inserts an idempotent transaction job. It does not receive the private key.

### API Server

The API reads indexed data from PostgreSQL, uses the deterministic C++ risk library for simulation, and observes chain-head state through read RPC endpoints. It never signs a transaction.

## Frontend Paths

Query path:

```text
Frontend → C++ API → PostgreSQL / deterministic Risk Engine / read RPC
```

User transaction path:

```text
Frontend → injected wallet → Solidity protocol
```

The Frontend distinguishes a confirmed receipt from later Indexer synchronization. Index lag is displayed as freshness information and does not change the chain transaction result.

## Deployment Modes

| Mode | Purpose | Environment file |
|---|---|---|
| Native C++ services | Fast local development | `.env.local` |
| Docker Compose | Container integration | `.env.containers` |
| kind | Distributed system and acceptance testing | `.env.kind` |
| Sepolia RPC check | Read-only endpoint validation | `.env.sepolia` |

Generated environment files are local artifacts and are excluded from version control.

## Optional Metal Service

The Apple Metal option pricer is a host-native macOS analytics service. It has no wallet, RPC, database, protocol, or signing authority. The Frontend calls it only when the user enables the optional Risk-page switch.
