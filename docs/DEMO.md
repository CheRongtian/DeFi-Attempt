# Demo Guide

## What the Demo Shows

The demonstration combines:

- the authoritative Solidity lending protocol;
- C++20 Indexer, Risk Engine, Tx Manager, Liquidator, messaging, and API services;
- PostgreSQL and NATS JetStream;
- a three-replica Go Oracle Coordinator;
- Kubernetes recovery and RPC failover;
- Prometheus and Grafana;
- a React wallet interface;
- optional Apple Metal option analytics on macOS;
- read-only Sepolia RPC connectivity.

All financial transactions in the full protocol demonstration execute on local Anvil. Sepolia is not used for deployment or transactions.

## Start the Complete Environment

Create `.env.sepolia` once:

```bash
cp .env.sepolia.example .env.sepolia
chmod 600 .env.sepolia
```

Replace all three URL placeholders with Sepolia endpoints, then run:

```bash
./scripts/run-final-demo.sh --clean
```

Use the existing cluster later:

```bash
./scripts/run-final-demo.sh
```

Keep that terminal open. The Frontend is served at `http://127.0.0.1:4173`.

## Demo Accounts

`prepare-frontend-demo.sh` mints:

- 20 MockWETH to Alice: `0x70997970C51812dc3A010C7d01b50e0d17dc79C8`;
- 100,000 MockUSDC to Charlie: `0x90F79bf6EB2c4f870365E785982E1f101E93b906`.

These are public Anvil development accounts. Import their standard Anvil development keys only into a wallet profile used for disposable local chains. Never fund or reuse them on a public network.

```text
Alice:   0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d
Charlie: 0x7c852118294b873bd7ebd81f49d5e1ac554b1f4a4392e31f5eac68e54b70
```

## Interactive Wallet Flow

Use the Frontend to demonstrate the user signing boundary:

1. Add the local network to the wallet: RPC `http://127.0.0.1:8546`, chain ID `31337`.
2. Connect Charlie and approve/supply MockUSDC liquidity.
3. Connect Alice and approve/supply MockWETH collateral.
4. Borrow MockUSDC as Alice.
5. Observe the wallet address, collateral, debt, supply position, and Health Factor.
6. Repay some or all debt through the wallet.
7. Withdraw eligible MockUSDC supply or MockWETH collateral.

The Frontend reports wallet confirmation, transaction hash, receipt confirmation, and subsequent Indexer synchronization as separate states.

## Automated Liquidation Flow

Use this flow on a clean deployment before manually changing Alice's position:

```bash
./scripts/create-liquidation-scenario.sh .env.kind
```

The script supplies 50,000 USDC of liquidity, supplies 10 WETH for Alice, borrows 20,000 USDC, and lowers WETH from `$3,000` to `$2,000`. The indexed position becomes unhealthy.

Observe:

```bash
curl -sS http://127.0.0.1:18080/markets
curl -sS http://127.0.0.1:18080/liquidations
curl -sS http://127.0.0.1:18080/protocol/stats
```

Expected system path:

```text
PriceUpdated log
→ Indexer updates canonical market state
→ Risk Engine writes an outbox candidate
→ Outbox Publisher sends it to JetStream
→ Liquidator claims and revalidates the job
→ Tx Manager signs and broadcasts the approved transaction
→ LiquidationManager and LendingPool validate on chain
→ Indexer records liquidation and accounting events
→ API and Frontend show the indexed result
```

The worker can execute multiple close-factor liquidations until collateral or debt is exhausted. The final state may include explicit bad debt, so the automated liquidation scenario should remain separate from the earlier manual repay/withdraw walkthrough.

## System Status

The Frontend System page and API freshness objects show:

```text
indexedBlock
indexedBlockHash
chainHead
indexLag
```

Prometheus is available at `http://127.0.0.1:19090`. The provisioned Grafana dashboard is available at `http://127.0.0.1:13000/d/dlp-overview`.

## Risk Simulation

The Frontend Risk page calls `POST /risk/simulate` and displays deterministic C++ results for hypothetical collateral, debt, and price inputs. This request does not submit a transaction.

On macOS, enable `Apple Metal option analytics` to call the host service. The tool prices European calls and puts and remains independent from lending risk and protocol state.

## Failure and Recovery Demo

Run the automated suite:

```bash
./scripts/run-recovery-tests.sh
```

Use `--clean` when a fresh cluster is required. The timestamped `summary.log` provides a concise pass/fail record, while the run directory retains workload, database, event, and service logs.

The suite demonstrates Pod recreation, database and JetStream recovery, RPC failover, Liquidator fencing takeover, Oracle leader failover, deep chain reorganization, and included-transaction reorg handling.

## Observability Demo

```bash
./scripts/run-observability-tests.sh
```

The script verifies scrape targets, required metrics, workload availability, protocol activity, and Grafana access, then writes timestamped artifacts below `artifacts/observability/`.

## Sepolia Boundary

```bash
./scripts/check-sepolia-rpc.sh
```

The primary, failover, and Frontend endpoints must all report Sepolia chain ID `11155111`. A successful result proves RPC connectivity only; it does not prove contract deployment or transaction execution on Sepolia.
