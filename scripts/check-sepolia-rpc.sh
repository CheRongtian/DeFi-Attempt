#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ENV_FILE=${1:-"$PROJECT_ROOT/.env.sepolia"}
SEPOLIA_CHAIN_ID=11155111

if [[ ! -f $ENV_FILE ]]
then
    printf 'Sepolia configuration not found: %s\n' "$ENV_FILE" >&2
    exit 1
fi

source "$ENV_FILE"

: "${DLP_SEPOLIA_RPC_URL:?DLP_SEPOLIA_RPC_URL is required}"
: "${DLP_SEPOLIA_RPC_FAILOVER_URL:?DLP_SEPOLIA_RPC_FAILOVER_URL is required}"
: "${DLP_SEPOLIA_FRONTEND_RPC_URL:?DLP_SEPOLIA_FRONTEND_RPC_URL is required}"

check_endpoint()
{
    local name=$1
    local url=$2
    local chain_id

    chain_id=$(cast chain-id --rpc-url "$url")
    if [[ $chain_id != "$SEPOLIA_CHAIN_ID" ]]
    then
        printf '%s returned chain ID %s; expected %s.\n' \
            "$name" "$chain_id" "$SEPOLIA_CHAIN_ID" >&2
        exit 1
    fi

    printf '%-12s connected to Sepolia (chain ID %s)\n' "$name" "$chain_id"
}

check_endpoint "Primary RPC" "$DLP_SEPOLIA_RPC_URL"
check_endpoint "Failover RPC" "$DLP_SEPOLIA_RPC_FAILOVER_URL"
check_endpoint "Frontend RPC" "$DLP_SEPOLIA_FRONTEND_RPC_URL"

printf 'Sepolia RPC integration is ready. No transaction was sent.\n'
