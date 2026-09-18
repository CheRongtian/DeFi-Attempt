# Observability

The project exposes Prometheus metrics from every C++ and Go service and provisions a Grafana dashboard for protocol, RPC, transaction, Oracle, and Kubernetes state.

## Components

```text
cpp/observability/                         shared C++ metrics server and RPC metrics
go/oracle-coordinator/internal/oracle/     Go Oracle metrics
observability/prometheus/prometheus.yml    scrape configuration
observability/grafana/                     datasource and dashboard provisioning
k8s/observability.yaml                     Prometheus, kube-state-metrics, Grafana
```

## Endpoints

Each application exposes:

```text
GET /health
GET /ready
GET /metrics
```

External kind endpoints:

| Service | URL |
|---|---|
| Prometheus | `http://127.0.0.1:19090` |
| Grafana | `http://127.0.0.1:13000/d/dlp-overview` |

## Metric Areas

The provisioned stack covers:

- service readiness;
- indexed and observed chain height;
- Indexer lag and processed blocks;
- RPC request counts, errors, latency, failover, broadcast success, and observed height;
- Risk Engine scans and liquidation candidates;
- outbox publication and pending rows;
- Liquidator claims, successes, and failures;
- Tx Manager pending, included, finalized, reorged, and failed jobs;
- Oracle leadership, fencing token, round, freshness, and failures;
- API position and liquidation totals;
- Kubernetes available replicas through kube-state-metrics.

## Grafana

Grafana is provisioned automatically with the Prometheus datasource and `DLP Overview` dashboard. No manual dashboard import is required in the kind environment.

The dashboard JSON is versioned at:

```text
observability/grafana/dashboards/dlp-overview.json
```

## Acceptance Test

Run against a fresh cluster:

```bash
./scripts/run-observability-tests.sh --clean
```

Reuse the current cluster:

```bash
./scripts/run-observability-tests.sh
```

The runner verifies:

1. API, Prometheus, and Grafana readiness.
2. Successful Prometheus scrapes for all required application and Kubernetes targets.
3. Presence of required protocol and infrastructure metrics.
4. Observable liquidation activity.
5. Oracle, RPC, transaction, and workload signals.

Artifacts are written to:

```text
artifacts/observability/<timestamp>/
```

`summary.log` is the concise result. `observability.log` and the captured snapshots retain the supporting diagnostics.

## Operational Notes

- A service can be alive while not ready; Kubernetes readiness and Prometheus `service_ready` expose that distinction.
- `indexer_lag_blocks` represents eventual consistency and can briefly be non-zero after a successful transaction.
- The local RPC proxies share one Anvil backend, so broadcast and failover metrics validate routing behavior rather than independent-node consensus.
- The acceptance artifact directories are ignored by Git and excluded from the Docker build context.
