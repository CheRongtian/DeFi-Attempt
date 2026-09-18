# C++20 Services

The C++20 layer provides strongly typed Ethereum primitives and the off-chain services around the Solidity protocol. It discovers and executes operational work without becoming a second financial authority.

## Modules

| Directory | Main output | Responsibility |
|---|---|---|
| `common/` | `dlp_ethereum_common` | Address/uint256 types, hex, Keccak, RLP, ABI, EIP-1559 signing, JSON-RPC |
| `observability/` | `dlp_observability` | Health, readiness, service and RPC metrics |
| `messaging/` | `dlp_outbox_publisher` | Event envelopes, PostgreSQL outbox, JetStream publishing/consumption |
| `indexer/` | `dlp_indexer` | Canonical block/log indexing and reorg-aware state projection |
| `risk-engine/` | `dlp_risk_worker` | Deterministic risk calculations and liquidation candidate events |
| `tx-manager/` | `dlp_tx_manager` | Approved operational signing, nonce recovery, replacement, finality |
| `liquidator/` | `dlp_liquidator` | Idempotent consumption, leased jobs, revalidation, tx job creation |
| `api-server/` | `dlp_api_server` | Indexed query API and deterministic risk simulation |

## Data Flow

```text
RPC → Indexer → PostgreSQL
PostgreSQL → Risk Worker → Outbox → JetStream
JetStream → Liquidator → liquidation_jobs → tx_jobs
tx_jobs → Tx Manager → RPC → Solidity
PostgreSQL + read RPC → API → Frontend
```

The Oracle Coordinator also writes approved Oracle publication jobs into `tx_jobs`.

## Build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

Required libraries include Boost.System, nlohmann/json, libpq/libpqxx 8, libsecp256k1, and GoogleTest. CMake fetches pinned ethash/Keccak, nats.c, and prometheus-cpp sources.

## Test

```bash
ctest --test-dir build --output-on-failure
```

PostgreSQL and RPC integration tests require their corresponding local services. See [Testing](../docs/TESTING.md).

## API

Default address: `http://127.0.0.1:18080`

| Method | Route | Purpose |
|---|---|---|
| GET | `/health` | API process health |
| GET | `/ready` | API readiness |
| GET | `/markets` | Indexed market and configuration |
| GET | `/positions/:address` | Indexed position and USDC supply |
| GET | `/positions/:address/health` | Deterministic indexed risk result |
| GET | `/liquidations` | Indexed liquidation history |
| GET | `/protocol/stats` | Indexed protocol totals |
| POST | `/risk/simulate` | Deterministic hypothetical risk result |

Responses derived from indexed state include chain-head and index-lag metadata. The server bounds request body size, request rate, and socket time and returns generic internal errors.

## Configuration

Common variables:

```text
DLP_DATABASE_URL
DLP_CHAIN_ID
DLP_RPC_URL
DLP_RPC_PRIMARY_URL
DLP_RPC_FAILOVER_URLS
DLP_READ_RPC_URLS
DLP_RPC_BROADCAST_URLS
DLP_NATS_URL
DLP_WETH_ADDRESS
DLP_USDC_ADDRESS
DLP_ORACLE_ADDRESS
DLP_POOL_ADDRESS
DLP_LIQUIDATION_MANAGER_ADDRESS
DLP_OPERATOR_ADDRESS
```

Only Tx Manager receives `DLP_OPERATOR_PRIVATE_KEY`. Metrics bind through service-specific `DLP_METRICS_PORT` values.

## Numeric and ABI Rules

- Financial values use checked 256-bit integer types.
- Ethereum quantities use strict hexadecimal parsing and canonical output.
- Risk calculations use exact integer equality against Solidity golden vectors.
- Protocol events are registered centrally in `common/ProtocolAbi`.
- Database queries use bound `pqxx::params` values.

## Operations

The services can run as host processes, Docker Compose containers, or Kubernetes Deployments. See [Operations](../docs/OPERATIONS.md) and [Distributed System Design](../docs/DISTRIBUTED_SYSTEM.md).
