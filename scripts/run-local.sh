#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DEPLOY_SCRIPT="$PROJECT_ROOT/scripts/deploy-local.sh"
ENV_FILE="$PROJECT_ROOT/.env.local"
INDEXER_EXECUTABLE="$PROJECT_ROOT/build/cpp/indexer/dlp_indexer"
LIQUIDATOR_EXECUTABLE="$PROJECT_ROOT/build/cpp/liquidator/dlp_liquidator"
API_EXECUTABLE="$PROJECT_ROOT/build/cpp/api-server/dlp_api_server"

ANVIL_PORT=${DLP_ANVIL_PORT:-8546}
POSTGRES_PORT=${DLP_POSTGRES_PORT:-5433}
POSTGRES_DB=${DLP_POSTGRES_DB:-dlp}
POSTGRES_USER=${DLP_POSTGRES_USER:-dlp}
POSTGRES_PASSWORD=${DLP_POSTGRES_PASSWORD:-dlp}
RPC_URL="http://127.0.0.1:$ANVIL_PORT"
DATABASE_URL="postgresql://$POSTGRES_USER:$POSTGRES_PASSWORD@127.0.0.1:$POSTGRES_PORT/$POSTGRES_DB"

ANVIL_PID=""
LIQUIDATOR_PID=""
API_PID=""
CLEAN_DATABASE=false

if [[ $# -gt 1 ]]
then
    printf 'Usage: %s [--clean]\n' "$0" >&2
    exit 2
fi

if [[ ${1:-} == --clean ]]
then
    CLEAN_DATABASE=true
elif [[ $# -eq 1 ]]
then
    printf 'Usage: %s [--clean]\n' "$0" >&2
    exit 2
fi

cleanup()
{
    if [[ -n $LIQUIDATOR_PID ]] && kill -0 "$LIQUIDATOR_PID" 2>/dev/null
    then
        kill "$LIQUIDATOR_PID"
        wait "$LIQUIDATOR_PID" 2>/dev/null || true
    fi
    if [[ -n $API_PID ]] && kill -0 "$API_PID" 2>/dev/null
    then
        kill "$API_PID"
        wait "$API_PID" 2>/dev/null || true
    fi
    if [[ -n $ANVIL_PID ]] && kill -0 "$ANVIL_PID" 2>/dev/null
    then
        kill "$ANVIL_PID"
        wait "$ANVIL_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

if [[ ! -x $INDEXER_EXECUTABLE || ! -x $LIQUIDATOR_EXECUTABLE || ! -x $API_EXECUTABLE ]]
then
    printf 'One or more C++ service executables are missing. Build the project before running local services.\n' >&2
    exit 1
fi

cd "$PROJECT_ROOT"

rpc_ready=false
if cast block-number --rpc-url "$RPC_URL" >/dev/null 2>&1
then
    rpc_ready=true
fi

deployment_ready=false
if [[ $rpc_ready == true && -f $ENV_FILE ]]
then
    source "$ENV_FILE"
    if [[ ${DLP_RPC_URL:-} == "$RPC_URL" \
        && -n ${DLP_POOL_ADDRESS:-} \
        && -n ${DLP_LIQUIDATION_MANAGER_ADDRESS:-} ]]
    then
        pool_code=$(cast code "$DLP_POOL_ADDRESS" --rpc-url "$RPC_URL" 2>/dev/null || true)
        liquidation_code=$(cast code "$DLP_LIQUIDATION_MANAGER_ADDRESS" --rpc-url "$RPC_URL" 2>/dev/null || true)
        if [[ $pool_code != 0x && -n $pool_code \
            && $liquidation_code != 0x && -n $liquidation_code ]]
        then
            deployment_ready=true
        fi
    fi
fi

if [[ $CLEAN_DATABASE == true ]]
then
    printf 'Removing the existing local PostgreSQL data...\n'
    DLP_POSTGRES_PORT="$POSTGRES_PORT" docker compose down -v
elif [[ $deployment_ready != true ]]
then
    printf 'Resetting PostgreSQL for a new local chain deployment...\n'
    DLP_POSTGRES_PORT="$POSTGRES_PORT" docker compose down -v
fi

printf 'Starting PostgreSQL on port %s...\n' "$POSTGRES_PORT"
DLP_POSTGRES_PORT="$POSTGRES_PORT" docker compose up -d postgres

postgres_ready=false
for ((attempt = 0; attempt < 30; ++attempt))
do
    if docker compose exec -T postgres \
        pg_isready -U "$POSTGRES_USER" -d "$POSTGRES_DB" >/dev/null 2>&1
    then
        postgres_ready=true
        break
    fi
    sleep 1
done

if [[ $postgres_ready != true ]]
then
    printf 'PostgreSQL did not become ready.\n' >&2
    exit 1
fi

for migration in "$PROJECT_ROOT"/database/migrations/*.sql
do
    docker compose exec -T postgres \
        psql -v ON_ERROR_STOP=1 -U "$POSTGRES_USER" -d "$POSTGRES_DB" < "$migration" >/dev/null
done

if [[ $rpc_ready == true ]]
then
    printf 'Using Ethereum RPC on port %s.\n' "$ANVIL_PORT"
else
    printf 'Starting Anvil on port %s...\n' "$ANVIL_PORT"
    anvil --port "$ANVIL_PORT" > "$PROJECT_ROOT/build/anvil.log" 2>&1 &
    ANVIL_PID=$!

    anvil_ready=false
    for ((attempt = 0; attempt < 30; ++attempt))
    do
        if cast block-number --rpc-url "$RPC_URL" >/dev/null 2>&1
        then
            anvil_ready=true
            break
        fi
        sleep 1
    done

    if [[ $anvil_ready != true ]]
    then
        printf 'Anvil did not become ready. See build/anvil.log.\n' >&2
        exit 1
    fi
fi

if [[ $deployment_ready != true ]]
then
    "$DEPLOY_SCRIPT" "$RPC_URL"
    source "$ENV_FILE"
else
    printf 'Using contracts from .env.local.\n'
fi

export DLP_DATABASE_URL="$DATABASE_URL"
export DLP_API_ADDRESS=${DLP_API_ADDRESS:-127.0.0.1}
export DLP_API_PORT=${DLP_API_PORT:-8081}

printf 'Starting local services...\n'
printf '  RPC:        %s\n' "$DLP_RPC_URL"
printf '  PostgreSQL: %s\n' "$DLP_DATABASE_URL"
printf '  API:        http://%s:%s\n' "$DLP_API_ADDRESS" "$DLP_API_PORT"

"$INDEXER_EXECUTABLE" --once
"$LIQUIDATOR_EXECUTABLE" &
LIQUIDATOR_PID=$!
"$API_EXECUTABLE" &
API_PID=$!
"$INDEXER_EXECUTABLE"
