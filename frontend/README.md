# Frontend Protocol Console

The Frontend is a React and TypeScript dashboard built with Vite, wagmi, viem, and TanStack Query.

## Trust Boundary

Queries follow:

```text
Frontend → C++ API → indexed PostgreSQL state / deterministic risk logic
```

User transactions follow:

```text
Frontend → injected wallet → Solidity LendingPool
```

The Frontend does not hold private keys, sign operational transactions, or ask the API to transact for a user.

## Pages

- **Market**: WETH/USDC prices, liquidity, LTV, liquidation threshold, and market configuration.
- **Position**: connected address, WETH collateral, USDC debt, USDC supply, and Health Factor.
- **Transactions**: MockUSDC/MockWETH supply, USDC borrow/repay, and USDC/WETH withdrawal.
- **Liquidations**: indexed borrower, repayment, collateral seized, block, and transaction hash.
- **System**: indexed block/hash, observed chain head, and index lag.
- **Risk**: deterministic C++ risk simulation and optional Apple Metal option analytics.

## Configuration

`frontend/.env.example` documents every Vite setting:

```text
VITE_API_BASE_URL
VITE_API_PROXY_TARGET
VITE_OPTION_PRICER_BASE_URL
VITE_OPTION_PRICER_PROXY_TARGET
VITE_RPC_URL
VITE_CHAIN_ID
VITE_CHAIN_NAME
VITE_LENDING_POOL_ADDRESS
VITE_PRICE_ORACLE_ADDRESS
VITE_RISK_MANAGER_ADDRESS
VITE_WETH_ADDRESS
VITE_USDC_ADDRESS
```

The application reports missing settings, invalid contract addresses, API/market address mismatches, and wrong wallet networks. Every `VITE_` value is public.

Generate `frontend/.env.local` from the active backend:

```bash
./scripts/configure-frontend.sh .env.local
./scripts/configure-frontend.sh .env.containers
./scripts/configure-frontend.sh .env.kind
```

Choose exactly one environment matching the backend currently running.

## Install, Build, and Test

```bash
cd frontend
npm install
npm run build
npm test
```

Development server:

```bash
npm run dev
```

One-command production preview from the repository root:

```bash
./scripts/frontend.sh .env.kind
```

The preview is available at `http://127.0.0.1:4173`.

## Wallet Transactions

For token supply and repayment, the transaction hook requests ERC-20 approval when needed. It then asks the wallet to call `LendingPool`, displays the transaction hash, waits for the receipt, and refreshes market and position queries.

A confirmed receipt and indexed API state are separate milestones. The interface continues polling when the chain transaction has succeeded but the Indexer has not reached its block.

Solidity remains responsible for final amount, liquidity, price-freshness, Health Factor, and withdrawal validation.

## Optional Metal Analytics

The Risk page includes a disabled-by-default switch. When enabled, requests are proxied to the host service at `VITE_OPTION_PRICER_PROXY_TARGET`.

The option result is an independent European call/put analysis. It does not affect lending Health Factor, liquidation, or any wallet transaction.

## Accessibility and Layout

The interface provides responsive layouts, light/dark themes, keyboard focus, reduced-motion handling, and explicit loading, error, unavailable, stale, wallet-disconnected, and wrong-network states.
