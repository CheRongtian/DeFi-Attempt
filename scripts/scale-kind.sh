#!/usr/bin/env bash

set -euo pipefail

NAMESPACE=dlp
CLUSTER_NAME=${DLP_KIND_CLUSTER:-dlp}
KUBE_CONTEXT="kind-$CLUSTER_NAME"

kubectl --context "$KUBE_CONTEXT" scale deployment/api-server --replicas=3 -n "$NAMESPACE"
kubectl --context "$KUBE_CONTEXT" rollout status deployment/api-server \
    -n "$NAMESPACE" --timeout=180s

kubectl --context "$KUBE_CONTEXT" scale deployment/risk-engine --replicas=2 -n "$NAMESPACE"
kubectl --context "$KUBE_CONTEXT" rollout status deployment/risk-engine \
    -n "$NAMESPACE" --timeout=180s

kubectl --context "$KUBE_CONTEXT" scale deployment/liquidator --replicas=3 -n "$NAMESPACE"
kubectl --context "$KUBE_CONTEXT" rollout status deployment/liquidator \
    -n "$NAMESPACE" --timeout=180s

printf 'Scaled API to 3, Risk Engine to 2, and Liquidator to 3 replicas.\n'
