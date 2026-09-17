#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CLUSTER_NAME=${DLP_KIND_CLUSTER:-dlp}
KUBE_CONTEXT="kind-$CLUSTER_NAME"
NAMESPACE=dlp
ENV_FILE="$PROJECT_ROOT/.env.kind"
ANVIL_PORT=8546
HOST_RPC_URL="http://127.0.0.1:$ANVIL_PORT"
WAIT_TIMEOUT=600s
CLEAN=false

IMAGES=(
    indexer
    outbox-publisher
    risk-engine
    tx-manager
    oracle-coordinator
    liquidator
    api-server
)

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

cd "$PROJECT_ROOT"

cluster_exists=false
if kind get clusters | grep -Fxq "$CLUSTER_NAME"
then
    cluster_exists=true
fi

if [[ $CLEAN == true && $cluster_exists == true ]]
then
    kind delete cluster --name "$CLUSTER_NAME"
    cluster_exists=false
fi

if [[ $cluster_exists != true ]]
then
    kind create cluster --name "$CLUSTER_NAME" --config "$PROJECT_ROOT/k8s/kind-config.yaml"
fi

kube()
{
    kubectl --context "$KUBE_CONTEXT" "$@"
}

for image in "${IMAGES[@]}"
do
    docker build --target "$image" --tag "dlp/$image:local" "$PROJECT_ROOT"
    kind load docker-image "dlp/$image:local" --name "$CLUSTER_NAME"
done

kube apply -f "$PROJECT_ROOT/k8s/namespace.yaml"
kube apply -f "$PROJECT_ROOT/k8s/infrastructure.yaml"
if [[ $cluster_exists == true ]]
then
    kube rollout restart deployment/rpc-a deployment/rpc-b -n "$NAMESPACE"
fi
kube rollout status statefulset/postgres -n "$NAMESPACE" --timeout="$WAIT_TIMEOUT"
kube rollout status statefulset/nats -n "$NAMESPACE" --timeout="$WAIT_TIMEOUT"
kube rollout status deployment/anvil -n "$NAMESPACE" --timeout="$WAIT_TIMEOUT"
kube rollout status deployment/rpc-a -n "$NAMESPACE" --timeout="$WAIT_TIMEOUT"
kube rollout status deployment/rpc-b -n "$NAMESPACE" --timeout="$WAIT_TIMEOUT"

kube create configmap dlp-migrations \
    --namespace "$NAMESPACE" \
    --from-file="$PROJECT_ROOT/database/migrations" \
    --dry-run=client \
    --output=yaml | kube apply -f -
kube delete job database-migrations -n "$NAMESPACE" --ignore-not-found
kube apply -f "$PROJECT_ROOT/k8s/migrations-job.yaml"
kube wait --for=condition=complete job/database-migrations \
    -n "$NAMESPACE" --timeout="$WAIT_TIMEOUT"

rpc_ready=false
for ((attempt = 0; attempt < 30; ++attempt))
do
    if cast block-number --rpc-url "$HOST_RPC_URL" >/dev/null 2>&1
    then
        rpc_ready=true
        break
    fi
    sleep 1
done

if [[ $rpc_ready != true ]]
then
    printf 'The kind Anvil RPC did not become ready on port %s.\n' "$ANVIL_PORT" >&2
    exit 1
fi

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
    printf 'Deploying contracts into the current kind chain.\n'
    "$PROJECT_ROOT/scripts/deploy-local.sh" "$HOST_RPC_URL" "$ENV_FILE"
else
    printf 'Using contracts from .env.kind.\n'
fi

source "$ENV_FILE"

kube create configmap dlp-contracts \
    --namespace "$NAMESPACE" \
    --from-literal=DLP_WETH_ADDRESS="$DLP_WETH_ADDRESS" \
    --from-literal=DLP_USDC_ADDRESS="$DLP_USDC_ADDRESS" \
    --from-literal=DLP_ORACLE_ADDRESS="$DLP_ORACLE_ADDRESS" \
    --from-literal=DLP_RISK_MANAGER_ADDRESS="$DLP_RISK_MANAGER_ADDRESS" \
    --from-literal=DLP_POOL_ADDRESS="$DLP_POOL_ADDRESS" \
    --from-literal=DLP_LIQUIDATION_MANAGER_ADDRESS="$DLP_LIQUIDATION_MANAGER_ADDRESS" \
    --from-literal=DLP_OPERATOR_ADDRESS="$DLP_OPERATOR_ADDRESS" \
    --dry-run=client \
    --output=yaml | kube apply -f -

kube create secret generic dlp-operator \
    --namespace "$NAMESPACE" \
    --from-literal=DLP_OPERATOR_PRIVATE_KEY="$DLP_OPERATOR_PRIVATE_KEY" \
    --dry-run=client \
    --output=yaml | kube apply -f -

kube apply -f "$PROJECT_ROOT/k8s/applications.yaml"

if [[ $cluster_exists == true ]]
then
    for deployment in indexer outbox-publisher risk-engine tx-manager oracle-coordinator liquidator api-server
    do
        kube rollout restart "deployment/$deployment" -n "$NAMESPACE"
    done
fi

for deployment in indexer outbox-publisher risk-engine tx-manager oracle-coordinator liquidator api-server
do
    kube rollout status "deployment/$deployment" -n "$NAMESPACE" --timeout="$WAIT_TIMEOUT"
done

kube create configmap prometheus-config \
    --namespace "$NAMESPACE" \
    --from-file=prometheus.yml="$PROJECT_ROOT/observability/prometheus/prometheus.yml" \
    --dry-run=client \
    --output=yaml | kube apply -f -
kube create configmap grafana-datasource \
    --namespace "$NAMESPACE" \
    --from-file=prometheus.yaml="$PROJECT_ROOT/observability/grafana/provisioning/datasources/prometheus.yaml" \
    --dry-run=client \
    --output=yaml | kube apply -f -
kube create configmap grafana-dashboard-provider \
    --namespace "$NAMESPACE" \
    --from-file=provider.yaml="$PROJECT_ROOT/observability/grafana/provisioning/dashboards/provider.yaml" \
    --dry-run=client \
    --output=yaml | kube apply -f -
kube create configmap grafana-dashboard \
    --namespace "$NAMESPACE" \
    --from-file=dlp-overview.json="$PROJECT_ROOT/observability/grafana/dashboards/dlp-overview.json" \
    --dry-run=client \
    --output=yaml | kube apply -f -
kube apply -f "$PROJECT_ROOT/k8s/observability.yaml"

if [[ $cluster_exists == true ]]
then
    kube rollout restart deployment/prometheus deployment/grafana -n "$NAMESPACE"
fi

for deployment in kube-state-metrics prometheus grafana
do
    kube rollout status "deployment/$deployment" -n "$NAMESPACE" --timeout="$WAIT_TIMEOUT"
done

printf '\nThe kind deployment is running with three Oracle Coordinator replicas.\n'
printf '  API: http://127.0.0.1:18080\n'
printf '  Prometheus: http://127.0.0.1:19090\n'
printf '  Grafana: http://127.0.0.1:13000/d/dlp-overview\n'
printf '  Scale: ./scripts/scale-kind.sh\n'
