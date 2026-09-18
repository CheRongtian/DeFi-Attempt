# Security Boundaries

## Scope

This is a portfolio prototype for disposable local environments. It has not received an external audit or formal verification and is not suitable for real funds. Security controls here demonstrate explicit trust boundaries and reject unsafe behavior within the supported WETH/USDC-style market.

## State Authority

Solidity is the final authority for:

- collateral and debt balances;
- borrow capacity and Health Factor;
- interest indices and reserves;
- withdrawal eligibility;
- liquidation eligibility and output;
- bad-debt recognition.

PostgreSQL, the Indexer, Risk Engine, Liquidator, API, and Frontend hold derived or operational state. Deleting and rebuilding PostgreSQL cannot change on-chain financial state.

## User Signing Boundary

User actions follow:

```text
Frontend → injected user wallet → Solidity protocol
```

The C++ API provides query and simulation responses only. It does not receive user private keys, sign user transactions, or relay supply, borrow, repay, or withdraw on a user's behalf.

Every `VITE_` value is public because Vite embeds it in browser assets. Private keys, seed phrases, and private provider credentials must never be placed in Frontend environment variables.

## Operational Signer

The operational private key is supplied only to the Tx Manager:

- native host runner removes it from non-signing process environments;
- Docker Compose supplies it only to the Tx Manager container;
- Kubernetes stores it in `dlp-operator` and injects it only into the Tx Manager Pod.

Before signing, the Tx Manager requires:

- zero native token value;
- the configured PriceOracle or LiquidationManager destination;
- an approved function selector;
- the exact expected calldata size.

Rejected and accepted lifecycle actions produce structured transaction audit logs. The signer is intended only for local Oracle publication and liquidation operations.

## Solidity Controls

### Token Transfers

`LendingPool` uses OpenZeppelin `SafeERC20` and checks exact sender, receiver, and pool balance deltas. The checks explicitly reject fee-on-transfer, rebasing, and other non-exact token semantics rather than silently corrupting accounting.

### Reentrancy and Interaction Order

State-changing pool entry points use `ReentrancyGuard`. Position and accounting effects are established around controlled token transfers, and pool-owned position tokens expose no external callback surface.

### Roles

- `PriceOracle` administration and direct price updates require `DEFAULT_ADMIN_ROLE`.
- Aggregated price publication requires `PUBLISHER_ROLE`.
- Pool liquidation execution requires `LIQUIDATION_ROLE`.
- The local deployment grants the liquidation role only to `LiquidationManager`.

### Oracle Validation

The Oracle rejects unsupported assets, zero prices, missing values, stale values, invalid report timestamps, stale reports, and non-increasing rounds. Risk-increasing actions require fresh WETH and USDC prices. Supply and repayment remain available when prices are stale.

### Liquidation and Bad Debt

`LiquidationManager` revalidates debt, collateral, prices, and Health Factor on chain. Close-factor, collateral, caller-request, minimum-output, and debt-dust rules are enforced before pool execution. The pool burns residual scaled debt when final collateral is exhausted, preventing repeated recognition of the same bad debt.

### Financial Rounding

Collateral uses downward valuation and debt uses conservative valuation. Scaled debt minting rounds upward. Shared Solidity/C++ vectors and invariants check indices, reserves, debt, collateral, and the pool's USDC accounting identity.

## API Boundary

The Boost.Beast HTTP server applies:

- a 16 KiB request-body limit;
- a process-wide 100 requests-per-second window;
- five-second socket read/write timeouts;
- strict Ethereum address and integer parsing;
- parameterized PostgreSQL queries;
- generic client-facing internal errors with structured server-side failure logs.

These are local prototype controls. Internet-facing deployment would additionally require a reverse proxy, TLS, authentication where appropriate, network-level limits, and production traffic policy.

## Database and Messaging

- SQL inputs use `pqxx::params` binding.
- Raw logs retain block identity and canonical status.
- Reorg recovery rebuilds derived state from canonical logs.
- Outbox events and transaction jobs use stable IDs and uniqueness constraints.
- JetStream consumers are idempotent under at-least-once delivery.
- Liquidation and Oracle ownership changes use fencing tokens.

## Secrets and Environment Files

The following generated files are excluded by `.gitignore`:

```text
.env.local
.env.containers
.env.kind
.env.sepolia
frontend/.env.local
```

Generation scripts use a restrictive umask and `chmod 600`. `.env.sepolia.example` contains placeholders only and instructs the user to keep private keys out.

The checked-in kind database password and Anvil keys are public local-development values. They must be replaced or removed from any non-disposable deployment and must never receive public-network funds.

## Kubernetes

- Database connection data is injected through `dlp-postgres`.
- The operator key is injected through `dlp-operator` only into Tx Manager.
- Oracle leader-election permissions are limited to coordination Leases in the `dlp` namespace.
- Application Pods use readiness and liveness endpoints.

Kubernetes Secrets provide configuration separation; they are not an external secret manager. Production work would require encrypted secret storage, managed key custody, restricted namespaces, network policies, image provenance, and access auditing.

## Supported and Unsupported Assumptions

Supported project assumptions:

- one MockWETH collateral market and one MockUSDC debt market;
- exact-transfer ERC-20 semantics;
- cooperating local operator;
- disposable Anvil and kind infrastructure;
- read-only Sepolia RPC verification.

Future production work, intentionally outside this project:

- external audit and formal verification;
- hardware or managed signing service;
- production Oracle providers and manipulation-resistant market-data policy;
- upgrade and emergency governance design;
- production database and JetStream high availability;
- TLS, authentication, WAF, and external rate limiting;
- mainnet or public-testnet deployment controls;
- incident response and operational key rotation.
