#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CONTRACTS_DIR="$PROJECT_ROOT/contracts"
ENV_FILE="$PROJECT_ROOT/.env.local"

RPC_URL=${1:-${DLP_RPC_URL:-http://127.0.0.1:8545}}
ANVIL_PRIVATE_KEY_VALUE=${ANVIL_PRIVATE_KEY:-0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80}

deploy_contract()
{
    local name=$1
    local contract=$2
    shift 2

    printf 'Deploying %s...\n' "$name" >&2

    local output
    output=$(forge create "$contract" \
        --rpc-url "$RPC_URL" \
        --private-key "$ANVIL_PRIVATE_KEY_VALUE" \
        --broadcast \
        "$@")

    local address
    address=$(printf '%s\n' "$output" | awk '/Deployed to:/ {print $3; exit}')
    if [[ ! $address =~ ^0x[0-9a-fA-F]{40}$ ]]
    then
        printf '%s\n' "$output" >&2
        printf 'Failed to read the %s deployment address.\n' "$name" >&2
        return 1
    fi

    printf '%s' "$address"
}

cd "$CONTRACTS_DIR"

WETH_ADDRESS=$(deploy_contract MockWETH src/mocks/MockWETH.sol:MockWETH)
USDC_ADDRESS=$(deploy_contract MockUSDC src/mocks/MockUSDC.sol:MockUSDC)
ORACLE_ADDRESS=$(deploy_contract PriceOracle src/PriceOracle.sol:PriceOracle)
RISK_MANAGER_ADDRESS=$(deploy_contract \
    RiskManager \
    src/RiskManager.sol:RiskManager \
    --constructor-args "$ORACLE_ADDRESS" "$WETH_ADDRESS" "$USDC_ADDRESS")
POOL_ADDRESS=$(deploy_contract \
    LendingPool \
    src/LendingPool.sol:LendingPool \
    --constructor-args "$RISK_MANAGER_ADDRESS")

{
    printf 'export DLP_RPC_URL=%q\n' "$RPC_URL"
    printf 'export DLP_WETH_ADDRESS=%q\n' "$WETH_ADDRESS"
    printf 'export DLP_USDC_ADDRESS=%q\n' "$USDC_ADDRESS"
    printf 'export DLP_ORACLE_ADDRESS=%q\n' "$ORACLE_ADDRESS"
    printf 'export DLP_RISK_MANAGER_ADDRESS=%q\n' "$RISK_MANAGER_ADDRESS"
    printf 'export DLP_POOL_ADDRESS=%q\n' "$POOL_ADDRESS"
} > "$ENV_FILE"

printf '\nLocal deployment complete:\n'
printf '  WETH:         %s\n' "$WETH_ADDRESS"
printf '  USDC:         %s\n' "$USDC_ADDRESS"
printf '  Oracle:       %s\n' "$ORACLE_ADDRESS"
printf '  RiskManager:  %s\n' "$RISK_MANAGER_ADDRESS"
printf '  LendingPool:  %s\n' "$POOL_ADDRESS"
printf '\nEnvironment written to %s\n' "$ENV_FILE"
