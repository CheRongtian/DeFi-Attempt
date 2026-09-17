#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
PROMETHEUS_URL=http://127.0.0.1:19090
GRAFANA_URL=http://127.0.0.1:13000
API_URL=http://127.0.0.1:18080
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

RUN_ID=$(date '+%Y%m%d-%H%M%S')
RUN_DIR="$PROJECT_ROOT/artifacts/observability/$RUN_ID"
RUN_LOG="$RUN_DIR/observability.log"
SUMMARY_LOG="$RUN_DIR/summary.log"
mkdir -p "$RUN_DIR"
: > "$SUMMARY_LOG"
exec > >(tee -a "$RUN_LOG") 2>&1

FAILED_CASES=0

record_result()
{
    local status=$1
    local name=$2
    printf '%-5s %s\n' "$status" "$name" | tee -a "$SUMMARY_LOG"
}

check()
{
    local name=$1
    shift
    if "$@"
    then
        record_result PASS "$name"
    else
        record_result FAIL "$name"
        FAILED_CASES=$((FAILED_CASES + 1))
    fi
}

wait_http()
{
    local url=$1
    local deadline=$((SECONDS + 120))
    while (( SECONDS < deadline ))
    do
        if curl -fsS "$url" >/dev/null 2>&1
        then
            return 0
        fi
        sleep 2
    done
    return 1
}

prometheus_query()
{
    local expression=$1
    curl -fsS --get \
        --data-urlencode "query=$expression" \
        "$PROMETHEUS_URL/api/v1/query"
}

query_scalar()
{
    python3 -c '
import json, sys
payload = json.load(sys.stdin)
result = payload["data"]["result"]
print(result[0]["value"][1] if result else "0")
'
}

all_targets_up()
{
    local up_count
    up_count=$(prometheus_query 'sum(up{job=~"(indexer|outbox-publisher|risk-engine|tx-manager|liquidator|api-server|oracle-coordinator|kube-state-metrics)"} == 1)' | query_scalar) || return 1
    [[ ${up_count%.*} -eq 10 ]]
}

wait_all_targets_up()
{
    local deadline=$((SECONDS + 120))
    while (( SECONDS < deadline ))
    do
        if all_targets_up
        then
            return 0
        fi
        sleep 2
    done
    return 1
}

required_metrics_present()
{
    local names
    names=$(curl -fsS "$PROMETHEUS_URL/api/v1/label/__name__/values") || return 1
    local required=(
        indexed_block_height
        chain_head_height
        indexer_lag_blocks
        risk_positions_scanned_total
        liquidation_candidates_total
        liquidation_success_total
        liquidation_failure_total
        rpc_requests_total
        rpc_errors_total
        rpc_block_height
        rpc_failovers_total
        rpc_broadcast_success_total
        tx_pending_total
        tx_included_total
        tx_finalized_total
        tx_reorged_total
        tx_failed_total
        oracle_round
        oracle_last_update_timestamp_seconds
        service_ready
        kube_deployment_status_replicas_available
    )
    for metric in "${required[@]}"
    do
        if [[ $names != *"\"$metric\""* ]]
        then
            printf 'Missing metric: %s\n' "$metric"
            return 1
        fi
    done
}

liquidation_observed()
{
    local deadline=$((SECONDS + 120))
    while (( SECONDS < deadline ))
    do
        local value
        value=$(prometheus_query 'sum(liquidation_success_total)' | query_scalar) || return 1
        if [[ ${value%.*} -gt 0 ]]
        then
            return 0
        fi
        sleep 2
    done
    return 1
}

cd "$PROJECT_ROOT"
kind_command=(./scripts/run-kind.sh)
if [[ $CLEAN == true ]]
then
    kind_command+=(--clean)
fi
if "${kind_command[@]}"
then
    record_result PASS "kind environment startup"
else
    record_result FAIL "kind environment startup"
    exit 1
fi

check "Prometheus is ready" wait_http "$PROMETHEUS_URL/-/ready"
check "Grafana is ready" wait_http "$GRAFANA_URL/api/health"
check "Protocol API is ready" wait_http "$API_URL/ready"

check "All ten scrape targets are up" wait_all_targets_up

curl -fsS "$PROMETHEUS_URL/api/v1/targets" > "$RUN_DIR/prometheus-targets.json"
curl -fsS "$GRAFANA_URL/api/health" > "$RUN_DIR/grafana-health.json"
prometheus_query 'up' > "$RUN_DIR/up.json"
prometheus_query 'service_ready' > "$RUN_DIR/service-ready.json"

check "Required protocol metrics are present" required_metrics_present

./scripts/create-liquidation-scenario.sh .env.kind
check "Liquidation success is visible in Prometheus" liquidation_observed

prometheus_query 'max(chain_head_height)' > "$RUN_DIR/chain-head.json"
prometheus_query 'max(rpc_block_height)' > "$RUN_DIR/rpc-block-height.json"
prometheus_query 'max(indexer_lag_blocks)' > "$RUN_DIR/indexer-lag.json"
prometheus_query 'sum(liquidation_success_total)' > "$RUN_DIR/liquidation-success.json"
prometheus_query 'max(oracle_round)' > "$RUN_DIR/oracle-round.json"

kubectl --context kind-dlp get pods -n dlp -o wide > "$RUN_DIR/pods.log"

printf '\nArtifacts: %s\n' "$RUN_DIR"
if (( FAILED_CASES != 0 ))
then
    printf '%d observability check(s) failed.\n' "$FAILED_CASES"
    exit 1
fi
printf 'All observability checks passed.\n'
