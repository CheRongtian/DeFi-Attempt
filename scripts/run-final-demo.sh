#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CLEAN=false

if [[ $# -gt 1 ]]
then
    printf 'Usage: %s [--clean]\n' "$0" >&2
    exit 2
fi

if [[ ${1:-} == --clean ]]
then
    CLEAN=true
elif [[ $# -eq 1 ]]
then
    printf 'Usage: %s [--clean]\n' "$0" >&2
    exit 2
fi

"$PROJECT_ROOT/scripts/check-sepolia-rpc.sh"

if [[ $CLEAN == true ]]
then
    "$PROJECT_ROOT/scripts/run-kind.sh" --clean
else
    "$PROJECT_ROOT/scripts/run-kind.sh"
fi
"$PROJECT_ROOT/scripts/prepare-frontend-demo.sh" "$PROJECT_ROOT/.env.kind"

printf '\nFinal local demo is ready.\n'
printf '  1. Charlie supplies 50,000 USDC.\n'
printf '  2. Alice supplies 10 WETH and borrows 10,000 USDC.\n'
printf '  3. Run: ./scripts/create-liquidation-scenario.sh .env.kind\n'
printf '  4. Complete repay and withdraw in the Frontend.\n\n'

exec "$PROJECT_ROOT/scripts/frontend.sh" "$PROJECT_ROOT/.env.kind"
