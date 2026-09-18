# Distributed System Design

## Design Goal

The off-chain system discovers, schedules, signs, and observes operational work while Solidity remains the final financial authority. PostgreSQL can be rebuilt from canonical logs; JetStream messages and worker processes can be replayed or replaced.

## Indexing and Canonical State

The C++ Indexer stores:

- block number, hash, parent hash, and canonical flag;
- raw protocol logs with canonical status;
- a chain sync cursor;
- projected market, position, price, liquidation, reserve, and bad-debt state.

Each indexed block commits raw logs, derived state, and the cursor atomically. On restart, the Indexer compares its stored tip with the RPC chain. A parent mismatch triggers backward common-ancestor discovery, orphan marking, and replay of canonical logs.

The database retains orphaned block/log history for diagnosis while API and risk queries use reconstructed canonical state.

## Transactional Outbox

Risk events are inserted into `outbox_events` in the same PostgreSQL transaction as the scan result. The Outbox Publisher reads unpublished rows, publishes an event envelope to JetStream, and records `published_at`.

Delivery is at least once. Duplicate delivery is expected and consumers use stable event IDs plus `processed_events` or unique job keys for idempotency.

## Risk Processing

The Risk Engine reads a bounded set of indexed positions and the current indexed market. Its integer calculations match Solidity through shared JSON golden vectors.

When a position is liquidatable, the worker writes a deterministic event containing the canonical block identity and version. A chain reorganization changes the canonical version and allows downstream work to reject obsolete candidates.

## Liquidation Jobs

Liquidator workers consume risk events and persist one job per source event. Workers compete through PostgreSQL:

1. An available or expired job is claimed with a lease.
2. Every takeover increments a fencing token.
3. The worker revalidates the candidate against the current chain.
4. A valid, profitable job creates a `tx_jobs` row.
5. Submission and completion updates require the current worker, unexpired lease, and fencing token.

A worker that resumes after losing its lease cannot overwrite the new owner's result.

## Transaction Manager

`tx_jobs` is the persistent operational queue. The Tx Manager:

- recovers active jobs after restart;
- allocates a nonce after the highest active or recovered nonce;
- signs EIP-1559 payloads with the operational key;
- broadcasts identical raw transactions to the primary and additional RPC endpoints;
- tracks `Pending`, `Submitted`, `Included`, `Finalized`, `Replaced`, `Reorged`, and `Failed` states;
- replaces stale submissions with the same nonce and increased fees;
- returns orphaned inclusions to the reorg path.

Before signing, the approval policy checks zero native value, destination address, function selector, and exact calldata length. Current approved calls are Oracle `publishPrice` and Liquidation Manager `liquidate`.

## Oracle Coordination

The Go Oracle Coordinator runs three replicas in kind.

```text
Kubernetes Lease
→ one active replica
→ median of three configurable static demo prices
→ PostgreSQL fencing token
→ monotonic round ID
→ idempotent tx job
→ C++ Tx Manager
→ Solidity PriceOracle
```

The Kubernetes Lease reduces duplicate active work. Database fencing protects publication if an old leader continues briefly after losing leadership. The Solidity Oracle independently rejects stale timestamps and non-increasing rounds.

The current providers are configurable demo values supplied through environment variables. They do not claim production market-data availability.

## RPC High Availability

The local distributed environment exposes `rpc-a` and `rpc-b`, both proxying the same Anvil chain.

- Indexer and API reads use ordered endpoints and validate chain identity.
- The Tx Manager obtains pending nonce and fee data from its preferred endpoint.
- Signed raw transactions are broadcast to both endpoints.
- A duplicate `already imported` response can occur because both proxies lead to the same development node.

This setup validates routing and failover mechanics; it does not simulate independent Ethereum consensus nodes.

## Recovery Properties

The automated recovery suite verifies:

- Kubernetes recreation after Indexer, Risk Engine, Tx Manager, and Liquidator Pod deletion;
- PostgreSQL disconnect and reconnect;
- JetStream restart and event recovery;
- primary RPC failure and failover;
- Liquidator lease expiry and fenced takeover;
- Oracle leader loss and fenced failover;
- deep chain reorganization and canonical state rebuild;
- reorganization of an included transaction;
- final database and chain invariants.

See [Testing](TESTING.md) for execution commands.

## Consistency Model

| Data | Consistency |
|---|---|
| Solidity state | Canonical after Ethereum confirmation assumptions |
| Indexed PostgreSQL state | Eventually consistent with the observed canonical chain |
| JetStream events | At-least-once |
| Liquidation and Oracle jobs | Idempotent with lease/fencing coordination |
| Frontend query results | Indexed state plus explicit chain-head/index-lag metadata |

The Frontend may show a successful receipt before the Indexer reaches the transaction block. That lag is an expected state transition.
