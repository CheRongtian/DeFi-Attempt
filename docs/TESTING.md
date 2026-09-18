# Testing

## Test Layers

The project validates the same protocol from several boundaries:

| Layer | Coverage |
|---|---|
| Solidity | Unit, integration, fuzz, invariant, security edge cases |
| Shared vectors | Solidity/C++ integer risk, interest, and liquidation parity |
| C++20 | Ethereum types, ABI, RPC, Indexer, Risk Engine, messaging, Tx Manager, Liquidator, API |
| PostgreSQL | Migrations, persistence, jobs, leases, fencing, state reconstruction |
| Go | Median aggregation, ABI encoding, publication idempotency and fencing |
| Frontend | Rendering, loading/errors, wallet/network state, transaction state, risk results |
| Kubernetes | Recovery, failover, reorg, leader and lease takeover |
| Observability | Scrape targets, required metrics, dashboards, and workload health |

## Solidity

```bash
cd contracts
forge build
forge test
forge fmt --check
```

Important suites cover:

- WETH and USDC mock behavior;
- Oracle registration, publisher rounds, missing/stale prices, and permissions;
- fixed-point rounding and golden vectors;
- borrow capacity, Health Factor, and zero debt;
- supply, borrow, repay, withdraw, and exact-transfer semantics;
- interest indices, reserves, fuzzed operations, and accounting invariants;
- liquidation bounds, collateral exhaustion, debt dust, and one-time bad debt;
- fee-on-transfer rejection and reentrancy boundaries.

## C++20

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Tests that depend on live services skip when their environment is absent.

### PostgreSQL Integration

```bash
DLP_POSTGRES_PORT=5433 docker compose up -d postgres

for migration in database/migrations/*.sql; do
  docker compose exec -T postgres \
    psql -v ON_ERROR_STOP=1 -U dlp -d dlp < "$migration"
done

DLP_TEST_DATABASE_URL='postgresql://dlp:dlp@127.0.0.1:5433/dlp' \
ctest --test-dir build --output-on-failure \
  -R '(PostgresStoreIntegrationTests|LiquidationJobStoreTests)'
```

### RPC Integration

After a local stack has deployed contracts:

```bash
source .env.local

ctest --test-dir build --output-on-failure \
  -R '(RpcClientIntegrationTests|RpcChainClientIntegrationTests)'
```

### Focused Security Tests

```bash
cmake --build build --target dlp_tx_manager_tests dlp_api_server_tests

ctest --test-dir build --output-on-failure \
  -R '(TxManagerTests|ApiServiceTests)'
```

These include the transaction approval policy, native-value rejection, bounded API behavior, and non-disclosure of internal errors.

## Shared Golden Vectors

`tests/golden/risk_vectors.json` is consumed by Foundry tests and the C++ Risk Engine tests. Values are integers with explicit rounding; the suites require exact equality instead of floating-point tolerances.

The vectors cover position risk, projected borrow indices, and liquidation calculations.

## Go Oracle Coordinator

Unit tests:

```bash
cd go/oracle-coordinator
go test ./...
```

PostgreSQL integration tests can run against a local forwarded database:

```bash
DLP_TEST_DATABASE_URL='postgresql://dlp:dlp@127.0.0.1:15433/dlp' \
go test -v ./...
```

When testing a kind database, establish the forwarding session in another terminal:

```bash
kubectl --context kind-dlp port-forward -n dlp pod/postgres-0 15433:5432
```

## Frontend

```bash
cd frontend
npm install
npm run build
npm test
```

The Frontend test suite uses mocked API and wallet boundaries. It covers configuration errors, disconnected and wrong-network states, market/position rendering, transaction progress, system freshness, API failures, and deterministic risk output.

## Recovery Acceptance

For a fresh cluster:

```bash
./scripts/run-recovery-tests.sh --clean
```

If the cluster and images are already present:

```bash
./scripts/run-recovery-tests.sh
```

The suite records `recovery.log`, `summary.log`, workload snapshots, database state, Kubernetes events, and service logs under:

```text
artifacts/recovery/<timestamp>/
```

Passing cases include baseline readiness, Pod recreation, PostgreSQL reconnect, JetStream recovery, RPC failover, fenced Liquidator takeover, Oracle failover, deep reorg rebuild, included-transaction reorg, and final invariants.

## Observability Acceptance

```bash
./scripts/run-observability-tests.sh --clean
```

Reuse an existing cluster by omitting `--clean`. Results are stored under:

```text
artifacts/observability/<timestamp>/
```

The suite waits for the required Prometheus targets, checks protocol and infrastructure metric names, creates a liquidation scenario, verifies recorded activity, and checks the provisioned Grafana service.

## Generated Artifacts

Build outputs, Foundry outputs, coverage, and timestamped acceptance logs are ignored by Git. Test fixtures and shared golden vectors remain versioned.
