#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CLEAN=false
METAL_PID=

cleanup()
{
    if [[ -n $METAL_PID ]] && kill -0 "$METAL_PID" 2>/dev/null
    then
        kill "$METAL_PID" 2>/dev/null || true
        wait "$METAL_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

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

printf 'Building Apple Metal option pricer...\n'
"$PROJECT_ROOT/scripts/metal-option-pricer.sh" build

printf 'Starting Apple Metal option pricer on http://%s:%s...\n' \
    "${DLP_OPTION_PRICER_HOST:-127.0.0.1}" \
    "${DLP_OPTION_PRICER_PORT:-18081}"
"$PROJECT_ROOT/scripts/metal-option-pricer.sh" serve &
METAL_PID=$!

printf '\nFinal local demo is ready.\n'
printf '  1. Charlie supplies 50,000 USDC.\n'
printf '  2. Alice supplies 10 WETH and borrows 10,000 USDC.\n'
printf '  3. Run: ./scripts/create-liquidation-scenario.sh .env.kind\n'
printf '  4. Complete repay and withdraw in the Frontend.\n\n'

"$PROJECT_ROOT/scripts/frontend.sh" "$PROJECT_ROOT/.env.kind"
