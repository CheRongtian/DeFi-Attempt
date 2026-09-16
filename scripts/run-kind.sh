#!/usr/bin/env bash

set -euo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CLUSTER_NAME=${DLP_KIND_CLUSTER:-dlp}
KUBE_CONTEXT="kind-$CLUSTER_NAME"
NAMESPACE=dlp
ENV_FILE="$PROJECT_ROOT/.env.kind"
ANVIL_PORT=8546
HOST_RPC_URL="http://127.0.0.1:$ANVIL_PORT"
CLEAN=false

IMAGES=(
    indexer
    outbox-publisher
    risk-engine
    tx-manager
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
kube rollout status statefulset/postgres -n "$NAMESPACE" --timeout=180s
kube rollout status statefulset/nats -n "$NAMESPACE" --timeout=180s
kube rollout status deployment/anvil -n "$NAMESPACE" --timeout=180s

kube create configmap dlp-migrations \
    --namespace "$NAMESPACE" \
    --from-file="$PROJECT_ROOT/database/migrations" \
    --dry-run=client \
    --output=yaml | kube apply -f -
kube delete job database-migrations -n "$NAMESPACE" --ignore-not-found
kube apply -f "$PROJECT_ROOT/k8s/migrations-job.yaml"
kube wait --for=condition=complete job/database-migrations \
    -n "$NAMESPACE" --timeout=180s

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
    if [[ $cluster_exists == true ]]
    then
        printf 'The existing kind cluster has no matching contract deployment. Rerun with --clean.\n' >&2
        exit 1
    fi
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
    for deployment in indexer outbox-publisher risk-engine tx-manager liquidator api-server
    do
        kube rollout restart "deployment/$deployment" -n "$NAMESPACE"
    done
fi

for deployment in indexer outbox-publisher risk-engine tx-manager liquidator api-server
do
    kube rollout status "deployment/$deployment" -n "$NAMESPACE" --timeout=180s
done

printf '\nThe single-replica kind deployment is running.\n'
printf '  API: http://127.0.0.1:18080\n'
printf '  Scale: ./scripts/scale-kind.sh\n'
