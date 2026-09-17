#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ENV_FILE=${1:-"$PROJECT_ROOT/.env.local"}
ALICE_ADDRESS=${DLP_DEMO_ALICE_ADDRESS:-0x70997970C51812dc3A010C7d01b50e0d17dc79C8}
CHARLIE_ADDRESS=${DLP_DEMO_CHARLIE_ADDRESS:-0x90F79bf6EB2c4f870365E785982E1f101E93b906}

if [[ ! -f $ENV_FILE ]]
then
    printf 'Protocol environment not found: %s\n' "$ENV_FILE" >&2
    exit 1
fi

source "$ENV_FILE"

: "${DLP_RPC_URL:?DLP_RPC_URL is required}"
: "${DLP_WETH_ADDRESS:?DLP_WETH_ADDRESS is required}"
: "${DLP_USDC_ADDRESS:?DLP_USDC_ADDRESS is required}"
: "${DLP_OPERATOR_PRIVATE_KEY:?DLP_OPERATOR_PRIVATE_KEY is required}"

cast send "$DLP_WETH_ADDRESS" "mint(address,uint256)" "$ALICE_ADDRESS" 20000000000000000000 \
    --rpc-url "$DLP_RPC_URL" --private-key "$DLP_OPERATOR_PRIVATE_KEY" >/dev/null
cast send "$DLP_USDC_ADDRESS" "mint(address,uint256)" "$CHARLIE_ADDRESS" 100000000000 \
    --rpc-url "$DLP_RPC_URL" --private-key "$DLP_OPERATOR_PRIVATE_KEY" >/dev/null

"$PROJECT_ROOT/scripts/configure-frontend.sh" "$ENV_FILE"

printf 'Frontend demo accounts funded:\n'
printf '  Alice:   %s (20 WETH)\n' "$ALICE_ADDRESS"
printf '  Charlie: %s (100,000 USDC)\n' "$CHARLIE_ADDRESS"
