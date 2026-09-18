# Go Oracle Coordinator

The Oracle Coordinator turns configurable demo price observations into fenced, idempotent Oracle publication jobs. It coordinates publication; the C++ Tx Manager owns the operational signer and sends the transaction.

## Workflow

```text
three configured demo providers
→ median price
→ Kubernetes leader election
→ PostgreSQL lease and fencing token
→ monotonic round ID
→ tx_jobs publication request
→ C++ Tx Manager
→ Solidity PriceOracle.publishPrice
```

The current providers are `StaticProvider` instances configured with environment values. They exercise aggregation and failover semantics without claiming production market-data connectivity.

## High Availability

In kind, three replicas compete for the `oracle-coordinator` Kubernetes Lease. Only the active leader runs publication work.

Before creating a publication, the leader acquires a PostgreSQL fence for the chain and Oracle address. A new owner increments the fencing token. The store accepts a publication only while the caller still owns the unexpired fence.

`oracle_rounds` assigns a strictly increasing round, and `oracle_publications` links each publication to one unique Tx Manager job. The Solidity Oracle independently checks publisher permission, timestamp, and round monotonicity.

## Configuration

Required deployment values:

```text
DLP_ORACLE_ADDRESS
DLP_ORACLE_ASSET_ADDRESS
DLP_OPERATOR_ADDRESS
```

Operational values:

| Variable | Default | Meaning |
|---|---:|---|
| `DLP_CHAIN_ID` | `31337` | Target chain |
| `DLP_DATABASE_URL` | local `dlp` database | PostgreSQL connection |
| `DLP_ORACLE_PROVIDER_A_PRICE` | `299500000000` | 8-decimal demo price |
| `DLP_ORACLE_PROVIDER_B_PRICE` | `300700000000` | 8-decimal demo price |
| `DLP_ORACLE_PROVIDER_C_PRICE` | `300100000000` | 8-decimal demo price |
| `DLP_ORACLE_PUBLISH_SECONDS` | `30` | Publication interval |
| `DLP_ORACLE_DATABASE_LEASE_SECONDS` | `45` | Database fence duration |
| `DLP_ORACLE_LEADER_ELECTION` | `false` | Enable Kubernetes Lease election |
| `DLP_ORACLE_LEASE_NAME` | `oracle-coordinator` | Lease resource name |
| `DLP_METRICS_PORT` | `9107` | Health and metrics server |

Kubernetes supplies `POD_NAME` and `POD_NAMESPACE`. The Coordinator requires the operator address for job creation but never receives the operator private key.

## Run and Test

```bash
go mod download
go test ./...
go run ./cmd/oracle-coordinator
```

Database-backed tests run when `DLP_TEST_DATABASE_URL` is set and otherwise skip. See [Testing](../../docs/TESTING.md).

## Metrics

The service exposes `/health`, `/ready`, and `/metrics`. Metrics include leadership state, fencing token, current round, last publication timestamp, and failure counts.

## Production Boundary

Production use would require authenticated, independently operated market-data providers, source freshness and deviation policy, stronger key custody, and public-network deployment controls. Those additions are outside this prototype.
