#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
ENV_FILE="$PROJECT_ROOT/.env.local"

source "$ENV_FILE"

BORROWER_PRIVATE_KEY=${DLP_TEST_BORROWER_PRIVATE_KEY:-0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d}
BORROWER_ADDRESS=$(cast wallet address --private-key "$BORROWER_PRIVATE_KEY")
MAX_UINT256=115792089237316195423570985008687907853269984665640564039457584007913129639935

cast send "$DLP_POOL_ADDRESS" "supply(address,uint256)" "$DLP_USDC_ADDRESS" 50000000000 \
    --rpc-url "$DLP_RPC_URL" --private-key "$DLP_OPERATOR_PRIVATE_KEY" >/dev/null

cast send "$DLP_WETH_ADDRESS" "mint(address,uint256)" "$BORROWER_ADDRESS" 10000000000000000000 \
    --rpc-url "$DLP_RPC_URL" --private-key "$DLP_OPERATOR_PRIVATE_KEY" >/dev/null
cast send "$DLP_WETH_ADDRESS" "approve(address,uint256)" "$DLP_POOL_ADDRESS" "$MAX_UINT256" \
    --rpc-url "$DLP_RPC_URL" --private-key "$BORROWER_PRIVATE_KEY" >/dev/null
cast send "$DLP_POOL_ADDRESS" "supply(address,uint256)" "$DLP_WETH_ADDRESS" 10000000000000000000 \
    --rpc-url "$DLP_RPC_URL" --private-key "$BORROWER_PRIVATE_KEY" >/dev/null
cast send "$DLP_POOL_ADDRESS" "borrow(address,uint256)" "$DLP_USDC_ADDRESS" 20000000000 \
    --rpc-url "$DLP_RPC_URL" --private-key "$BORROWER_PRIVATE_KEY" >/dev/null

cast send "$DLP_ORACLE_ADDRESS" "setPrice(address,uint256)" "$DLP_WETH_ADDRESS" 200000000000 \
    --rpc-url "$DLP_RPC_URL" --private-key "$DLP_OPERATOR_PRIVATE_KEY" >/dev/null

printf 'Liquidation scenario created for %s.\n' "$BORROWER_ADDRESS"
printf 'The indexed position becomes liquidatable after the WETH price update is processed.\n'
