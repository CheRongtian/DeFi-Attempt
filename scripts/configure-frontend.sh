#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SOURCE_ENV=${1:-"$PROJECT_ROOT/.env.local"}
FRONTEND_ENV="$PROJECT_ROOT/frontend/.env.local"

if [[ ! -f $SOURCE_ENV ]]
then
    printf 'Protocol environment not found: %s\n' "$SOURCE_ENV" >&2
    exit 1
fi

source "$SOURCE_ENV"

: "${DLP_RPC_URL:?DLP_RPC_URL is required}"
: "${DLP_POOL_ADDRESS:?DLP_POOL_ADDRESS is required}"
: "${DLP_ORACLE_ADDRESS:?DLP_ORACLE_ADDRESS is required}"
: "${DLP_RISK_MANAGER_ADDRESS:?DLP_RISK_MANAGER_ADDRESS is required}"
: "${DLP_WETH_ADDRESS:?DLP_WETH_ADDRESS is required}"
: "${DLP_USDC_ADDRESS:?DLP_USDC_ADDRESS is required}"

API_URL=${DLP_FRONTEND_API_URL:-/api}
API_PROXY_TARGET=${DLP_API_URL:-http://127.0.0.1:${DLP_API_PORT:-18080}}
CHAIN_ID=$(cast chain-id --rpc-url "$DLP_RPC_URL")
CHAIN_NAME=${DLP_CHAIN_NAME:-Anvil Local}

{
    printf 'VITE_API_BASE_URL=%s\n' "$API_URL"
    printf 'VITE_API_PROXY_TARGET=%s\n' "$API_PROXY_TARGET"
    printf 'VITE_RPC_URL=%s\n' "$DLP_RPC_URL"
    printf 'VITE_CHAIN_ID=%s\n' "$CHAIN_ID"
    printf 'VITE_CHAIN_NAME=%s\n' "$CHAIN_NAME"
    printf 'VITE_LENDING_POOL_ADDRESS=%s\n' "$DLP_POOL_ADDRESS"
    printf 'VITE_PRICE_ORACLE_ADDRESS=%s\n' "$DLP_ORACLE_ADDRESS"
    printf 'VITE_RISK_MANAGER_ADDRESS=%s\n' "$DLP_RISK_MANAGER_ADDRESS"
    printf 'VITE_WETH_ADDRESS=%s\n' "$DLP_WETH_ADDRESS"
    printf 'VITE_USDC_ADDRESS=%s\n' "$DLP_USDC_ADDRESS"
} > "$FRONTEND_ENV"

printf 'Frontend environment written to %s\n' "$FRONTEND_ENV"
