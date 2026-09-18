# Kubernetes Deployment

The `k8s/` manifests run the full local distributed system in a single-node kind cluster named `dlp`.

## Files

| File | Contents |
|---|---|
| `kind-config.yaml` | Cluster and host port mappings |
| `namespace.yaml` | `dlp` namespace |
| `infrastructure.yaml` | PostgreSQL, NATS JetStream, Anvil, `rpc-a`, and `rpc-b` |
| `migrations-job.yaml` | Ordered PostgreSQL migration Job |
| `applications.yaml` | C++ services, Go Oracle replicas, ConfigMaps, RBAC, and API Service |
| `observability.yaml` | Prometheus, kube-state-metrics, Grafana, Services, and RBAC |

## Topology

Stateful infrastructure:

```text
postgres-0        PostgreSQL with a PersistentVolumeClaim
nats-0            NATS JetStream with persistent storage
anvil             local Ethereum development chain
rpc-a / rpc-b     two request-routing endpoints over the same Anvil chain
```

Application Deployments:

```text
indexer
outbox-publisher
risk-engine
tx-manager
liquidator
api-server
oracle-coordinator (3 replicas)
```

Observability Deployments:

```text
prometheus
kube-state-metrics
grafana
```

## Start

From the repository root:

```bash
./scripts/run-kind.sh --clean
```

The script builds and loads local images before applying manifests. Omit `--clean` to reuse the cluster, persistent state, and image cache:

```bash
./scripts/run-kind.sh
```

Slow first-time image downloads may continue after a rollout timeout. Once infrastructure Pods are running, rerunning without `--clean` resumes the deployment.

## Inspect

```bash
kubectl --context kind-dlp get pods -n dlp
kubectl --context kind-dlp get deployments,statefulsets,services -n dlp
kubectl --context kind-dlp get events -n dlp --sort-by=.lastTimestamp
kubectl --context kind-dlp get lease oracle-coordinator -n dlp
```

Service logs:

```bash
kubectl --context kind-dlp logs -n dlp deployment/indexer
kubectl --context kind-dlp logs -n dlp deployment/tx-manager
kubectl --context kind-dlp logs -n dlp deployment/oracle-coordinator
```

## Scaling

```bash
./scripts/scale-kind.sh
```

The script scales API Server to three replicas, Risk Engine to two, and Liquidator to three. Liquidator jobs remain coordinated through PostgreSQL leases and fencing tokens.

## Configuration and Secrets

`run-kind.sh` creates:

- `dlp-contracts`: deployed addresses and operator address;
- `dlp-postgres`: local database settings and connection URL;
- `dlp-operator`: the local operational private key;
- migration and observability ConfigMaps from repository files.

Only Tx Manager consumes `dlp-operator`. The Oracle Coordinator receives the public operator address and creates transaction jobs without signing them.

The checked-in database values and generated Anvil key are disposable local credentials. Kubernetes Secrets in this project demonstrate isolation from ordinary ConfigMaps; they do not replace a production secret manager.

## Health Probes

C++ and Go services expose `/health` and `/ready` on their metrics ports. Kubernetes uses these for liveness and readiness. PostgreSQL uses `pg_isready`; NATS and Anvil have service-specific probes.

## Host Ports

| Host port | Service |
|---:|---|
| `8546` | Anvil RPC |
| `18080` | API Server |
| `19090` | Prometheus |
| `13000` | Grafana |

## Reset

Delete all kind state:

```bash
kind delete cluster --name dlp
```

The next `run-kind.sh --clean` recreates the cluster, PVCs, deployed contracts, and application state.
