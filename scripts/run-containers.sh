#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BASE_COMPOSE="$PROJECT_ROOT/compose.yaml"
APP_COMPOSE="$PROJECT_ROOT/compose.apps.yaml"
ENV_FILE="$PROJECT_ROOT/.env.containers"

export DLP_ANVIL_PORT=${DLP_ANVIL_PORT:-8546}
export DLP_POSTGRES_PORT=${DLP_POSTGRES_PORT:-5433}
export DLP_NATS_PORT=${DLP_NATS_PORT:-4222}
export DLP_API_PORT=${DLP_API_PORT:-18080}

HOST_RPC_URL="http://127.0.0.1:$DLP_ANVIL_PORT"
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

compose()
{
    docker compose -f "$BASE_COMPOSE" -f "$APP_COMPOSE" "$@"
}

start_infrastructure()
{
    compose up -d --wait postgres nats anvil rpc-a rpc-b
}

cd "$PROJECT_ROOT"

if [[ $CLEAN == true ]]
then
    compose down -v --remove-orphans
fi

start_infrastructure

deployment_ready=false
if [[ -f $ENV_FILE ]]
then
    source "$ENV_FILE"
    if [[ ${DLP_RPC_URL:-} == "$HOST_RPC_URL" \
        && -n ${DLP_POOL_ADDRESS:-} \
        && -n ${DLP_LIQUIDATION_MANAGER_ADDRESS:-} \
        && -n ${DLP_OPERATOR_ADDRESS:-} ]]
    then
        pool_code=$(cast code "$DLP_POOL_ADDRESS" --rpc-url "$HOST_RPC_URL")
        liquidation_code=$(cast code "$DLP_LIQUIDATION_MANAGER_ADDRESS" --rpc-url "$HOST_RPC_URL")
        if [[ $pool_code != 0x && $liquidation_code != 0x ]]
        then
            deployment_ready=true
        fi
    fi
fi

if [[ $deployment_ready != true ]]
then
    if [[ $CLEAN != true ]]
    then
        compose down -v --remove-orphans
        start_infrastructure
    fi
    "$PROJECT_ROOT/scripts/deploy-local.sh" "$HOST_RPC_URL" "$ENV_FILE"
else
    printf 'Using contracts from .env.containers.\n'
fi

for migration in "$PROJECT_ROOT"/database/migrations/*.sql
do
    compose exec -T postgres \
        psql -v ON_ERROR_STOP=1 -U "${DLP_POSTGRES_USER:-dlp}" \
        -d "${DLP_POSTGRES_DB:-dlp}" < "$migration" >/dev/null
done

source "$ENV_FILE"

compose up -d --build \
    indexer \
    outbox-publisher \
    risk-engine \
    tx-manager \
    oracle-coordinator \
    liquidator \
    api-server

printf '\nContainer services are running.\n'
printf '  API: http://127.0.0.1:%s\n' "$DLP_API_PORT"
printf '  Logs: docker compose -f compose.yaml -f compose.apps.yaml logs -f\n'
