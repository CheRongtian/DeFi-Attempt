# Operations

## Runtime Modes

| Mode | Launcher | Main use |
|---|---|---|
| Native services | `scripts/run-local.sh` | Fast C++ iteration |
| Docker Compose | `scripts/run-containers.sh` | Container integration |
| kind | `scripts/run-kind.sh` | Kubernetes and distributed-system validation |
| Complete macOS demo | `scripts/run-final-demo.sh` | kind, Frontend, Metal, and Sepolia RPC check |

All commands below run from the repository root.

## Native Services

Build the C++ targets first, then run:

```bash
./scripts/run-local.sh
```

The script starts PostgreSQL and NATS with Docker Compose, starts or reuses Anvil, deploys the contracts, applies migrations, and runs the C++ services as host processes. It writes contract and operator values to `.env.local` with owner-only permissions.

Default endpoints:

| Component | Endpoint |
|---|---|
| PostgreSQL | `127.0.0.1:5433` |
| NATS | `127.0.0.1:4222` |
| Anvil RPC | `http://127.0.0.1:8546` |
| API | `http://127.0.0.1:18080` |

Reset the PostgreSQL and NATS volumes before startup:

```bash
./scripts/run-local.sh --clean
```

`Ctrl+C` stops the host services and the Anvil process created by the runner. Docker infrastructure remains available until it is stopped with Docker Compose.

## Docker Compose

```bash
./scripts/run-containers.sh --clean
```

This combines `compose.yaml` and `compose.apps.yaml`, deploys contracts through the host Foundry tools, applies migrations, builds service images, and starts the containerized application stack. Generated deployment values are stored in `.env.containers`.

Later starts can reuse the existing state:

```bash
./scripts/run-containers.sh
```

Inspect logs:

```bash
docker compose -f compose.yaml -f compose.apps.yaml logs -f
```

Stop containers while preserving volumes:

```bash
docker compose -f compose.yaml -f compose.apps.yaml down
```

## kind

Create a fresh cluster and full distributed environment:

```bash
./scripts/run-kind.sh --clean
```

The runner:

1. Creates the `dlp` kind cluster.
2. Builds and loads the local application images.
3. Starts PostgreSQL, NATS JetStream, Anvil, and two RPC proxies.
4. applies all database migrations;
5. deploys the Solidity contracts and creates Kubernetes ConfigMaps/Secrets;
6. starts the Indexer, Outbox Publisher, Risk Engine, Tx Manager, Liquidator, API, and three Oracle Coordinator replicas;
7. provisions Prometheus, kube-state-metrics, and Grafana.

Use the existing cluster and image cache after an interrupted image pull or during normal iteration:

```bash
./scripts/run-kind.sh
```

Inspect workloads and the elected Oracle leader:

```bash
kubectl --context kind-dlp get pods -n dlp
kubectl --context kind-dlp get deployments,statefulsets -n dlp
kubectl --context kind-dlp get lease oracle-coordinator -n dlp
```

Scale the stateless demonstration workloads:

```bash
./scripts/scale-kind.sh
```

This scales the API to three replicas, Risk Engine to two replicas, and Liquidator to three replicas.

Delete the cluster:

```bash
kind delete cluster --name dlp
```

## Frontend

Generate Frontend configuration from the environment belonging to the active backend:

```text
run-local.sh       → ./scripts/frontend.sh .env.local
run-containers.sh  → ./scripts/frontend.sh .env.containers
run-kind.sh        → ./scripts/frontend.sh .env.kind
```

`frontend.sh` regenerates `frontend/.env.local`, builds the production bundle, and serves it at `http://127.0.0.1:4173`.

Using an environment file from another chain or deployment causes the Frontend to disable transactions because the API market addresses do not match the configured contracts.

## Complete Demo Launcher

The complete launcher currently targets macOS because it builds and starts the Apple Metal option service:

```bash
cp .env.sepolia.example .env.sepolia
chmod 600 .env.sepolia
# Replace the three endpoint placeholders.

./scripts/run-final-demo.sh --clean
```

On later runs:

```bash
./scripts/run-final-demo.sh
```

The command remains active while the Frontend preview server runs. Press `Ctrl+C` to stop the Frontend and host Metal process. Kubernetes services remain in the cluster.

## Service URLs

| Service | URL |
|---|---|
| Anvil RPC | `http://127.0.0.1:8546` |
| API | `http://127.0.0.1:18080` |
| Frontend preview | `http://127.0.0.1:4173` |
| Metal health | `http://127.0.0.1:18081/health` |
| Prometheus | `http://127.0.0.1:19090` |
| Grafana | `http://127.0.0.1:13000/d/dlp-overview` |

The Metal service root has no HTML route; `GET /` returns `route not found` by design.

## API Checks

```bash
curl -sS http://127.0.0.1:18080/markets
curl -sS http://127.0.0.1:18080/liquidations
curl -sS http://127.0.0.1:18080/protocol/stats
```

Responses include `indexedBlock`, `indexedBlockHash`, `chainHead`, and `indexLag` freshness information.

## Sepolia RPC Connectivity

`.env.sepolia` contains only three RPC URLs:

```text
DLP_SEPOLIA_RPC_URL
DLP_SEPOLIA_RPC_FAILOVER_URL
DLP_SEPOLIA_FRONTEND_RPC_URL
```

Validate them with:

```bash
./scripts/check-sepolia-rpc.sh
```

All endpoints must report chain ID `11155111`. The script does not load a wallet, deploy a contract, or send a transaction.

## Generated State

- `.env.local`, `.env.containers`, `.env.kind`, `.env.sepolia`, and `frontend/.env.local` are ignored.
- C++ and Metal build outputs are written below `build/`.
- Foundry writes generated outputs below `contracts/out/` and `contracts/cache/`.
- Recovery and observability runs write timestamped diagnostics below `artifacts/`.
