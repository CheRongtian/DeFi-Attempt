#!/usr/bin/env bash

set -uo pipefail

PROJECT_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CLUSTER_NAME=${DLP_KIND_CLUSTER:-dlp}
KUBE_CONTEXT="kind-$CLUSTER_NAME"
NAMESPACE=dlp
ENV_FILE="$PROJECT_ROOT/.env.kind"
RPC_URL=http://127.0.0.1:8546
API_URL=http://127.0.0.1:18080
WAIT_SECONDS=180
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
RUN_DIR="$PROJECT_ROOT/artifacts/recovery/$RUN_ID"
SERVICES_DIR="$RUN_DIR/services"
RUN_LOG="$RUN_DIR/recovery.log"
SUMMARY_LOG="$RUN_DIR/summary.log"

mkdir -p "$SERVICES_DIR"
: > "$SUMMARY_LOG"
exec > >(tee -a "$RUN_LOG") 2>&1

FAILED_CASES=0
ACTIVE_SNAPSHOT=
POSTGRES_MUTATED=false
NATS_MUTATED=false
RPC_A_MUTATED=false
LIQUIDATOR_MUTATED=false
ORACLE_MUTATED=false
TX_MANAGER_MUTATED=false
TX_CONFIRMATIONS_MUTATED=false

POSTGRES_REPLICAS=1
NATS_REPLICAS=1
RPC_A_REPLICAS=1
LIQUIDATOR_REPLICAS=1
ORACLE_REPLICAS=3
TX_MANAGER_REPLICAS=1

kube()
{
    kubectl --context "$KUBE_CONTEXT" "$@"
}

sql()
{
    local query=$1
    kube exec -n "$NAMESPACE" postgres-0 -- \
        psql -U dlp -d dlp -v ON_ERROR_STOP=1 -Atqc "$query"
}

sql_report()
{
    local query=$1
    kube exec -n "$NAMESPACE" postgres-0 -- \
        psql -U dlp -d dlp -v ON_ERROR_STOP=1 -c "$query"
}

log()
{
    printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*"
}

record_result()
{
    local status=$1
    local name=$2
    printf '%-5s %s\n' "$status" "$name" | tee -a "$SUMMARY_LOG"
}

wait_until()
{
    local timeout=$1
    local description=$2
    shift 2

    local deadline=$((SECONDS + timeout))
    while (( SECONDS < deadline ))
    do
        if "$@"
        then
            return 0
        fi
        sleep 1
    done

    log "Timed out waiting for $description."
    return 1
}

replica_count()
{
    local kind=$1
    local name=$2
    kube get "$kind/$name" -n "$NAMESPACE" -o jsonpath='{.spec.replicas}'
}

pod_absent()
{
    local app=$1
    [[ -z $(kube get pods -n "$NAMESPACE" -l "app=$app" -o name 2>/dev/null) ]]
}

postgres_ready()
{
    kube exec -n "$NAMESPACE" postgres-0 -- pg_isready -U dlp -d dlp >/dev/null 2>&1
}

index_at_least()
{
    local target=$1
    local current
    current=$(sql "SELECT COALESCE(MAX(block_number), 0) FROM sync_state WHERE chain_id = 31337" 2>/dev/null) || return 1
    [[ $current =~ ^[0-9]+$ ]] && (( current >= target ))
}

sync_matches_chain()
{
    local row
    if ! row=$(sql "SELECT block_number || '|' || block_hash FROM sync_state WHERE chain_id = 31337" 2>/dev/null)
    then
        log "Failed to read the indexed chain cursor."
        return 1
    fi
    if [[ -z $row ]]
    then
        log "The indexed chain cursor is empty."
        return 1
    fi

    local block_number=${row%%|*}
    local database_hash=${row#*|}
    local chain_hash
    if ! chain_hash=$(cast block "$block_number" --field hash --rpc-url "$RPC_URL" 2>&1)
    then
        log "Failed to read canonical block $block_number: $chain_hash"
        return 1
    fi

    if [[ $(printf '%s' "$database_hash" | tr '[:upper:]' '[:lower:]') \
        != $(printf '%s' "$chain_hash" | tr '[:upper:]' '[:lower:]') ]]
    then
        log "Indexed block $block_number does not match the canonical chain: database=$database_hash chain=$chain_hash"
        return 1
    fi
}

api_ready()
{
    curl -fsS "$API_URL/protocol/stats" >/dev/null 2>&1
}

oracle_ready()
{
    local holder
    holder=$(kube get lease oracle-coordinator -n "$NAMESPACE" \
        -o jsonpath='{.spec.holderIdentity}' 2>/dev/null) || return 1
    [[ -n $holder ]] || return 1

    local publications
    publications=$(sql "SELECT COUNT(*) FROM oracle_publications" 2>/dev/null) || return 1
    [[ $publications =~ ^[0-9]+$ ]] && (( publications > 0 ))
}

block_flow_complete()
{
    local block_hash=$1
    local complete
    complete=$(sql "
        SELECT CASE WHEN
            EXISTS
            (
                SELECT 1 FROM outbox_events
                WHERE lower(block_hash) = lower('$block_hash')
                  AND published_at IS NOT NULL
            )
            AND EXISTS
            (
                SELECT 1
                FROM processed_events AS processed
                JOIN outbox_events AS event ON event.event_id = processed.event_id
                WHERE processed.consumer_name = 'risk-engine'
                  AND lower(event.block_hash) = lower('$block_hash')
            )
        THEN 1 ELSE 0 END
    " 2>/dev/null) || return 1
    [[ $complete == 1 ]]
}

tx_status_is()
{
    local job_id=$1
    local expected=$2
    [[ $(sql "SELECT status FROM tx_jobs WHERE job_id = '$job_id'" 2>/dev/null) == "$expected" ]]
}

oracle_finalized_after()
{
    local round_id=$1
    local count
    count=$(sql "
        SELECT COUNT(*)
        FROM oracle_publications AS publication
        JOIN tx_jobs AS transaction ON transaction.job_id = publication.tx_job_id
        WHERE publication.round_id > $round_id
          AND transaction.status = 'Finalized'
    " 2>/dev/null) || return 1
    [[ $count =~ ^[0-9]+$ ]] && (( count > 0 ))
}

deployment_replaced_pod()
{
    local app=$1
    local old_uid=$2
    local desired ready uids
    desired=$(replica_count deployment "$app" 2>/dev/null) || return 1
    ready=$(kube get deployment "$app" -n "$NAMESPACE" \
        -o jsonpath='{.status.readyReplicas}' 2>/dev/null) || return 1
    ready=${ready:-0}
    uids=$(kube get pods -n "$NAMESPACE" -l "app=$app" \
        -o jsonpath='{range .items[*]}{.metadata.uid}{"\n"}{end}' 2>/dev/null) || return 1
    [[ $ready == "$desired" && $uids != *"$old_uid"* ]]
}

oracle_holder_changed()
{
    local old_holder=$1
    local holder
    holder=$(kube get lease oracle-coordinator -n "$NAMESPACE" \
        -o jsonpath='{.spec.holderIdentity}' 2>/dev/null) || return 1
    [[ -n $holder && $holder != "$old_holder" ]]
}

oracle_database_holder_is()
{
    local expected=$1
    [[ $(sql "
        SELECT holder
        FROM oracle_leases
        WHERE chain_id = 31337
          AND lower(oracle_address) = lower('$DLP_ORACLE_ADDRESS')
    " 2>/dev/null) == "$expected" ]]
}

jetstream_ready()
{
    kube exec -n "$NAMESPACE" nats-0 -- \
        wget -q -O - http://127.0.0.1:8222/jsz 2>/dev/null \
        | grep -Eq '"streams"[[:space:]]*:[[:space:]]*[1-9][0-9]*'
}

send_price()
{
    local price=$1
    local output
    output=$(cast send "$DLP_ORACLE_ADDRESS" "setPrice(address,uint256)" \
        "$DLP_WETH_ADDRESS" "$price" \
        --rpc-url "$RPC_URL" \
        --private-key "$DLP_OPERATOR_PRIVATE_KEY" 2>&1) || {
            printf '%s\n' "$output"
            return 1
        }
    printf '%s\n' "$output"

    LAST_TX_HASH=$(printf '%s\n' "$output" | awk '$1 == "transactionHash" {print $2; exit}')
    LAST_BLOCK_HASH=$(printf '%s\n' "$output" | awk '$1 == "blockHash" {print $2; exit}')

    [[ $LAST_TX_HASH =~ ^0x[0-9a-fA-F]{64}$ \
        && $LAST_BLOCK_HASH =~ ^0x[0-9a-fA-F]{64}$ ]]
}

queue_price_job()
{
    local job_id=$1
    local calldata operator oracle current_round next_round reported_at
    current_round=$(cast call "$DLP_ORACLE_ADDRESS" "latestRoundIds(address)(uint256)" \
        "$DLP_USDC_ADDRESS" --rpc-url "$RPC_URL") || return 1
    next_round=$((current_round + 1))
    reported_at=$(cast block latest --field timestamp --rpc-url "$RPC_URL") || return 1
    calldata=$(cast calldata "publishPrice(address,uint256,uint256,uint256)" \
        "$DLP_USDC_ADDRESS" 100000000 "$reported_at" "$next_round") || return 1
    operator=$(printf '%s' "$DLP_OPERATOR_ADDRESS" | tr '[:upper:]' '[:lower:]')
    oracle=$(printf '%s' "$DLP_ORACLE_ADDRESS" | tr '[:upper:]' '[:lower:]')

    sql "
        INSERT INTO tx_jobs
            (job_id, chain_id, wallet_address, to_address, value, calldata, status)
        VALUES
            ('$job_id', 31337, '$operator', '$oracle', 0, '$calldata', 'Pending')
    " >/dev/null
}

restore_mutations()
{
    local restored=true

    if [[ -n $ACTIVE_SNAPSHOT ]]
    then
        log "Restoring active Anvil snapshot $ACTIVE_SNAPSHOT."
        cast rpc evm_revert "$ACTIVE_SNAPSHOT" --rpc-url "$RPC_URL" >/dev/null 2>&1 || restored=false
        ACTIVE_SNAPSHOT=
    fi

    if [[ $POSTGRES_MUTATED == true ]]
    then
        kube scale statefulset/postgres --replicas="$POSTGRES_REPLICAS" -n "$NAMESPACE" >/dev/null || restored=false
        kube rollout status statefulset/postgres -n "$NAMESPACE" \
            --timeout="${WAIT_SECONDS}s" >/dev/null || restored=false
        POSTGRES_MUTATED=false
    fi

    if [[ $NATS_MUTATED == true ]]
    then
        kube scale statefulset/nats --replicas="$NATS_REPLICAS" -n "$NAMESPACE" >/dev/null || restored=false
        kube rollout status statefulset/nats -n "$NAMESPACE" \
            --timeout="${WAIT_SECONDS}s" >/dev/null || restored=false
        NATS_MUTATED=false
    fi

    if [[ $RPC_A_MUTATED == true ]]
    then
        kube scale deployment/rpc-a --replicas="$RPC_A_REPLICAS" -n "$NAMESPACE" >/dev/null || restored=false
        kube rollout status deployment/rpc-a -n "$NAMESPACE" \
            --timeout="${WAIT_SECONDS}s" >/dev/null || restored=false
        RPC_A_MUTATED=false
    fi

    if [[ $LIQUIDATOR_MUTATED == true ]]
    then
        kube scale deployment/liquidator --replicas="$LIQUIDATOR_REPLICAS" -n "$NAMESPACE" >/dev/null || restored=false
        kube rollout status deployment/liquidator -n "$NAMESPACE" \
            --timeout="${WAIT_SECONDS}s" >/dev/null || restored=false
        LIQUIDATOR_MUTATED=false
    fi

    if [[ $ORACLE_MUTATED == true ]]
    then
        kube scale deployment/oracle-coordinator --replicas="$ORACLE_REPLICAS" \
            -n "$NAMESPACE" >/dev/null || restored=false
        kube rollout status deployment/oracle-coordinator -n "$NAMESPACE" \
            --timeout="${WAIT_SECONDS}s" >/dev/null || restored=false
        ORACLE_MUTATED=false
    fi

    if [[ $TX_MANAGER_MUTATED == true ]]
    then
        kube scale deployment/tx-manager --replicas="$TX_MANAGER_REPLICAS" \
            -n "$NAMESPACE" >/dev/null || restored=false
        kube rollout status deployment/tx-manager -n "$NAMESPACE" \
            --timeout="${WAIT_SECONDS}s" >/dev/null || restored=false
        TX_MANAGER_MUTATED=false
    fi

    if [[ $TX_CONFIRMATIONS_MUTATED == true ]]
    then
        kube set env deployment/tx-manager DLP_TX_CONFIRMATIONS- \
            -n "$NAMESPACE" >/dev/null || restored=false
        kube rollout status deployment/tx-manager -n "$NAMESPACE" \
            --timeout="${WAIT_SECONDS}s" >/dev/null || restored=false
        TX_CONFIRMATIONS_MUTATED=false
    fi

    [[ $restored == true ]]
}

run_case()
{
    local name=$1
    local test_function=$2
    local status=0

    printf '\n============================================================\n'
    log "CASE: $name"
    printf '============================================================\n'

    "$test_function" || status=$?
    if ! restore_mutations
    then
        log "The environment could not be fully restored after $name."
        status=1
    fi

    if (( status == 0 ))
    then
        record_result PASS "$name"
    else
        record_result FAIL "$name"
        FAILED_CASES=$((FAILED_CASES + 1))
    fi
}

delete_and_wait_for_replacement()
{
    local app=$1
    local pod uid
    pod=$(kube get pods -n "$NAMESPACE" -l "app=$app" \
        -o jsonpath='{.items[0].metadata.name}') || return 1
    uid=$(kube get pod "$pod" -n "$NAMESPACE" -o jsonpath='{.metadata.uid}') || return 1
    log "Deleting $pod."
    kube delete pod "$pod" -n "$NAMESPACE" || return 1
    wait_until "$WAIT_SECONDS" "$app replacement pod" \
        deployment_replaced_pod "$app" "$uid"
}

test_pod_recovery()
{
    local app job_id
    for app in indexer risk-engine tx-manager liquidator
    do
        delete_and_wait_for_replacement "$app" || return 1
    done

    job_id="recovery-tx-restart-$RUN_ID"
    TX_MANAGER_MUTATED=true
    kube scale deployment/tx-manager --replicas=0 -n "$NAMESPACE" || return 1
    wait_until 60 "Tx Manager pod removal" pod_absent tx-manager || return 1
    queue_price_job "$job_id" || return 1
    kube scale deployment/tx-manager --replicas="$TX_MANAGER_REPLICAS" \
        -n "$NAMESPACE" || return 1
    kube rollout status deployment/tx-manager -n "$NAMESPACE" \
        --timeout="${WAIT_SECONDS}s" || return 1
    wait_until "$WAIT_SECONDS" "pending transaction recovery after Tx Manager restart" \
        tx_status_is "$job_id" Finalized || return 1

    send_price 300200000000 || return 1
    wait_until "$WAIT_SECONDS" "post-crash block processing" \
        block_flow_complete "$LAST_BLOCK_HASH"
}

test_database_reconnect()
{
    local round_before target_head
    round_before=$(sql "SELECT COALESCE(MAX(last_round_id), 0) FROM oracle_rounds") || return 1

    POSTGRES_MUTATED=true
    kube scale statefulset/postgres --replicas=0 -n "$NAMESPACE" || return 1
    wait_until 60 "PostgreSQL pod removal" pod_absent postgres || return 1

    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    target_head=$(cast block-number --rpc-url "$RPC_URL") || return 1

    kube scale statefulset/postgres --replicas="$POSTGRES_REPLICAS" -n "$NAMESPACE" || return 1
    kube rollout status statefulset/postgres -n "$NAMESPACE" \
        --timeout="${WAIT_SECONDS}s" || return 1
    wait_until 60 "PostgreSQL readiness" postgres_ready || return 1
    wait_until "$WAIT_SECONDS" "Indexer database reconnection" \
        index_at_least "$target_head" || return 1

    send_price 300300000000 || return 1
    wait_until "$WAIT_SECONDS" "post-reconnect event delivery" \
        block_flow_complete "$LAST_BLOCK_HASH" || return 1
    wait_until 150 "post-reconnect Oracle transaction finality" \
        oracle_finalized_after "$round_before" || return 1
    api_ready
}

test_jetstream_restart()
{
    jetstream_ready || return 1

    NATS_MUTATED=true
    kube scale statefulset/nats --replicas=0 -n "$NAMESPACE" || return 1
    wait_until 60 "NATS pod removal" pod_absent nats || return 1
    kube scale statefulset/nats --replicas="$NATS_REPLICAS" -n "$NAMESPACE" || return 1
    kube rollout status statefulset/nats -n "$NAMESPACE" \
        --timeout="${WAIT_SECONDS}s" || return 1
    wait_until 60 "JetStream recovery" jetstream_ready || return 1

    send_price 300400000000 || return 1
    wait_until "$WAIT_SECONDS" "post-JetStream event delivery" \
        block_flow_complete "$LAST_BLOCK_HASH"
}

test_rpc_failover()
{
    local target_head job_id
    job_id="recovery-rpc-$RUN_ID"

    RPC_A_MUTATED=true
    kube scale deployment/rpc-a --replicas=0 -n "$NAMESPACE" || return 1
    wait_until 60 "primary RPC pod removal" pod_absent rpc-a || return 1

    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    target_head=$(cast block-number --rpc-url "$RPC_URL") || return 1
    wait_until "$WAIT_SECONDS" "Indexer RPC failover" \
        index_at_least "$target_head" || return 1

    if ! curl -fsS "$API_URL/markets" >/dev/null
    then
        log "The markets API did not fail over after the primary RPC stopped."
        return 1
    fi
    if ! curl -fsS "$API_URL/protocol/stats" >/dev/null
    then
        log "The protocol stats API did not fail over after the primary RPC stopped."
        return 1
    fi
    if ! curl -fsS "$API_URL/liquidations" >/dev/null
    then
        log "The liquidations API did not fail over after the primary RPC stopped."
        return 1
    fi

    queue_price_job "$job_id" || return 1
    wait_until "$WAIT_SECONDS" "transaction finality through the failover RPC" \
        tx_status_is "$job_id" Finalized
}

test_liquidator_lease_recovery()
{
    local cursor block_number block_hash canonical_version job_id source_event operator weth usdc
    cursor=$(sql "SELECT block_number || '|' || block_hash FROM sync_state WHERE chain_id = 31337") || return 1
    block_number=${cursor%%|*}
    block_hash=${cursor#*|}
    canonical_version=$(sql "SELECT canonical_version FROM chain_versions WHERE chain_id = 31337") || return 1
    job_id="recovery-liquidator-$RUN_ID"
    source_event="recovery.liquidator:$RUN_ID"
    operator=$(printf '%s' "$DLP_OPERATOR_ADDRESS" | tr '[:upper:]' '[:lower:]')
    weth=$(printf '%s' "$DLP_WETH_ADDRESS" | tr '[:upper:]' '[:lower:]')
    usdc=$(printf '%s' "$DLP_USDC_ADDRESS" | tr '[:upper:]' '[:lower:]')

    LIQUIDATOR_MUTATED=true
    kube scale deployment/liquidator --replicas=0 -n "$NAMESPACE" || return 1
    wait_until 60 "Liquidator pod removal" pod_absent liquidator || return 1

    sql "
        INSERT INTO liquidation_jobs
        (
            job_id, source_event_id, chain_id, borrower_address,
            debt_asset, collateral_asset, health_factor, max_repay,
            expected_bonus, expected_collateral, expected_bad_debt,
            block_number, block_hash, canonical_version, status,
            worker_id, lease_until, fencing_token
        )
        VALUES
        (
            '$job_id', '$source_event', 31337, '$operator',
            '$usdc', '$weth', 1, 1,
            0, 1, 0,
            $block_number, '$block_hash', $canonical_version, 'Claimed',
            'crashed-liquidator', NOW() + INTERVAL '5 seconds', 1
        )
    " >/dev/null || return 1

    kube scale deployment/liquidator --replicas="$LIQUIDATOR_REPLICAS" \
        -n "$NAMESPACE" || return 1
    kube rollout status deployment/liquidator -n "$NAMESPACE" \
        --timeout="${WAIT_SECONDS}s" || return 1

    wait_until "$WAIT_SECONDS" "expired liquidation lease takeover" \
        liquidator_job_recovered "$job_id"
}

liquidator_job_recovered()
{
    local job_id=$1
    [[ $(sql "
        SELECT CASE WHEN
            status = 'Invalidated'
            AND worker_id <> 'crashed-liquidator'
            AND fencing_token >= 2
        THEN 1 ELSE 0 END
        FROM liquidation_jobs
        WHERE job_id = '$job_id'
    " 2>/dev/null) == 1 ]]
}

test_oracle_leader_recovery()
{
    local old_holder old_round old_token new_holder new_token
    old_holder=$(kube get lease oracle-coordinator -n "$NAMESPACE" \
        -o jsonpath='{.spec.holderIdentity}') || return 1
    old_round=$(sql "SELECT COALESCE(MAX(last_round_id), 0) FROM oracle_rounds") || return 1
    old_token=$(sql "
        SELECT fencing_token FROM oracle_leases
        WHERE chain_id = 31337
          AND lower(oracle_address) = lower('$DLP_ORACLE_ADDRESS')
    ") || return 1

    log "Deleting Oracle leader $old_holder."
    kube delete pod "$old_holder" -n "$NAMESPACE" || return 1
    wait_until 60 "new Kubernetes Oracle leader" \
        oracle_holder_changed "$old_holder" || return 1
    new_holder=$(kube get lease oracle-coordinator -n "$NAMESPACE" \
        -o jsonpath='{.spec.holderIdentity}') || return 1

    wait_until 150 "new Oracle database fence holder" \
        oracle_database_holder_is "$new_holder" || return 1
    new_token=$(sql "
        SELECT fencing_token FROM oracle_leases
        WHERE chain_id = 31337
          AND lower(oracle_address) = lower('$DLP_ORACLE_ADDRESS')
    ") || return 1
    (( new_token > old_token )) || return 1

    wait_until 150 "publication from the new fenced Oracle leader" \
        oracle_publication_uses_fence "$new_token" || return 1
    wait_until 150 "new Oracle round finality" \
        oracle_finalized_after "$old_round"
}

oracle_publication_uses_fence()
{
    local fencing_token=$1
    local count
    count=$(sql "SELECT COUNT(*) FROM oracle_publications WHERE fencing_token = $fencing_token" 2>/dev/null) || return 1
    [[ $count =~ ^[0-9]+$ ]] && (( count > 0 ))
}

test_deep_reorg()
{
    local orphan_price=300600000000
    local canonical_price=300700000000
    local orphan_tx orphan_row orphan_block orphan_hash old_version orphan_version new_version
    local reorg_job reorg_source operator weth usdc target_head

    ORACLE_MUTATED=true
    kube scale deployment/oracle-coordinator --replicas=0 -n "$NAMESPACE" || return 1
    wait_until 60 "Oracle pod removal before reorg" pod_absent oracle-coordinator || return 1
    wait_until 90 "outstanding transactions before reorg" no_active_transactions || return 1

    LIQUIDATOR_MUTATED=true
    kube scale deployment/liquidator --replicas=0 -n "$NAMESPACE" || return 1
    wait_until 60 "Liquidator pod removal before reorg" pod_absent liquidator || return 1

    target_head=$(cast block-number --rpc-url "$RPC_URL") || return 1
    wait_until "$WAIT_SECONDS" "Indexer before reorg" index_at_least "$target_head" || return 1
    old_version=$(sql "SELECT canonical_version FROM chain_versions WHERE chain_id = 31337") || return 1

    ACTIVE_SNAPSHOT=$(cast rpc evm_snapshot --rpc-url "$RPC_URL" | tr -d '"') || return 1
    send_price "$orphan_price" || return 1
    orphan_tx=$LAST_TX_HASH
    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    target_head=$(cast block-number --rpc-url "$RPC_URL") || return 1
    wait_until "$WAIT_SECONDS" "orphan branch indexing" index_at_least "$target_head" || return 1

    orphan_row=$(sql "
        SELECT block_number || '|' || block_hash
        FROM raw_logs
        WHERE lower(transaction_hash) = lower('$orphan_tx') AND canonical
        LIMIT 1
    ") || return 1
    [[ -n $orphan_row ]] || return 1
    orphan_block=${orphan_row%%|*}
    orphan_hash=${orphan_row#*|}
    orphan_version=$(sql "SELECT canonical_version FROM chain_versions WHERE chain_id = 31337") || return 1
    wait_until "$WAIT_SECONDS" "orphan branch event publication" \
        block_flow_complete "$orphan_hash" || return 1

    reorg_job="recovery-reorg-job-$RUN_ID"
    reorg_source="recovery.reorg:$RUN_ID"
    operator=$(printf '%s' "$DLP_OPERATOR_ADDRESS" | tr '[:upper:]' '[:lower:]')
    weth=$(printf '%s' "$DLP_WETH_ADDRESS" | tr '[:upper:]' '[:lower:]')
    usdc=$(printf '%s' "$DLP_USDC_ADDRESS" | tr '[:upper:]' '[:lower:]')
    sql "
        INSERT INTO liquidation_jobs
        (
            job_id, source_event_id, chain_id, borrower_address,
            debt_asset, collateral_asset, health_factor, max_repay,
            expected_bonus, expected_collateral, expected_bad_debt,
            block_number, block_hash, canonical_version, status,
            worker_id, lease_until, fencing_token
        )
        VALUES
        (
            '$reorg_job', '$reorg_source', 31337, '$operator',
            '$usdc', '$weth', 1, 1,
            0, 1, 0,
            $orphan_block, '$orphan_hash', $orphan_version, 'Claimed',
            'reorg-liquidator', NOW() + INTERVAL '5 minutes', 1
        )
    " >/dev/null || return 1

    cast rpc evm_revert "$ACTIVE_SNAPSHOT" --rpc-url "$RPC_URL" >/dev/null || return 1
    ACTIVE_SNAPSHOT=
    delete_and_wait_for_replacement indexer || return 1
    send_price "$canonical_price" || return 1
    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    target_head=$(cast block-number --rpc-url "$RPC_URL") || return 1
    wait_until "$WAIT_SECONDS" "canonical replacement branch indexing" \
        index_at_least "$target_head" || return 1

    local orphan_log_count indexed_price
    orphan_log_count=$(sql "
        SELECT COUNT(*) FROM raw_logs
        WHERE lower(transaction_hash) = lower('$orphan_tx') AND NOT canonical
    ") || return 1
    if (( orphan_log_count == 0 ))
    then
        log "The orphaned transaction $orphan_tx is still canonical after the reorg."
        return 1
    fi

    indexed_price=$(sql "SELECT weth_price FROM markets WHERE chain_id = 31337") || return 1
    if [[ $indexed_price != "$canonical_price" ]]
    then
        log "The rebuilt market price is incorrect: expected=$canonical_price actual=$indexed_price"
        return 1
    fi
    sync_matches_chain || return 1

    new_version=$(sql "SELECT canonical_version FROM chain_versions WHERE chain_id = 31337") || return 1
    if (( new_version <= old_version ))
    then
        log "The canonical version did not advance after the reorg: before=$old_version after=$new_version"
        return 1
    fi
    wait_until "$WAIT_SECONDS" "reorg event convergence" \
        reorg_event_processed_after "$old_version" || return 1

    kube scale deployment/liquidator --replicas="$LIQUIDATOR_REPLICAS" \
        -n "$NAMESPACE" || return 1
    kube rollout status deployment/liquidator -n "$NAMESPACE" \
        --timeout="${WAIT_SECONDS}s" || return 1
    wait_until "$WAIT_SECONDS" "orphaned liquidation job invalidation" \
        liquidation_job_status_is "$reorg_job" Invalidated
}

no_active_transactions()
{
    [[ $(sql "
        SELECT COUNT(*) FROM tx_jobs
        WHERE status IN ('Pending', 'Submitted', 'Included')
    " 2>/dev/null) == 0 ]]
}

reorg_event_processed_after()
{
    local canonical_version=$1
    [[ $(sql "
        SELECT CASE WHEN
            event.published_at IS NOT NULL
            AND processed.event_id IS NOT NULL
        THEN 1 ELSE 0 END
        FROM outbox_events AS event
        LEFT JOIN processed_events AS processed
          ON processed.consumer_name = 'risk-engine'
         AND processed.event_id = event.event_id
        WHERE event.event_type = 'chain.reorg'
          AND event.canonical_version > $canonical_version
        ORDER BY event.canonical_version DESC
        LIMIT 1
    " 2>/dev/null) == 1 ]]
}

liquidation_job_status_is()
{
    local job_id=$1
    local expected=$2
    [[ $(sql "SELECT status FROM liquidation_jobs WHERE job_id = '$job_id'" 2>/dev/null) == "$expected" ]]
}

test_transaction_reorg()
{
    local job_id="recovery-transaction-reorg-$RUN_ID"

    ORACLE_MUTATED=true
    kube scale deployment/oracle-coordinator --replicas=0 -n "$NAMESPACE" || return 1
    wait_until 60 "Oracle pod removal before transaction reorg" \
        pod_absent oracle-coordinator || return 1
    wait_until 90 "outstanding transactions before transaction reorg" \
        no_active_transactions || return 1

    TX_CONFIRMATIONS_MUTATED=true
    kube set env deployment/tx-manager DLP_TX_CONFIRMATIONS=20 \
        -n "$NAMESPACE" >/dev/null || return 1
    kube rollout status deployment/tx-manager -n "$NAMESPACE" \
        --timeout="${WAIT_SECONDS}s" || return 1

    ACTIVE_SNAPSHOT=$(cast rpc evm_snapshot --rpc-url "$RPC_URL" | tr -d '"') || return 1
    queue_price_job "$job_id" || return 1
    wait_until "$WAIT_SECONDS" "transaction inclusion before reorg" \
        tx_status_is "$job_id" Included || return 1

    cast rpc evm_revert "$ACTIVE_SNAPSHOT" --rpc-url "$RPC_URL" >/dev/null || return 1
    ACTIVE_SNAPSHOT=
    cast rpc evm_mine --rpc-url "$RPC_URL" >/dev/null || return 1
    wait_until "$WAIT_SECONDS" "transaction Reorged state" \
        tx_status_is "$job_id" Reorged
}

test_final_invariants()
{
    local violations
    sync_matches_chain || return 1
    if ! api_ready
    then
        log "The API is unavailable during the final recovery check."
        return 1
    fi

    violations=$(sql "
        SELECT
            (SELECT COUNT(*) FROM
                (SELECT block_number FROM blocks WHERE canonical GROUP BY chain_id, block_number HAVING COUNT(*) > 1) AS duplicate_blocks)
          + (SELECT COUNT(*) FROM
                (SELECT event_id FROM outbox_events GROUP BY event_id HAVING COUNT(*) > 1) AS duplicate_events)
          + (SELECT COUNT(*) FROM
                (SELECT consumer_name, event_id FROM processed_events GROUP BY consumer_name, event_id HAVING COUNT(*) > 1) AS duplicate_consumption)
          + (SELECT COUNT(*) FROM
                (SELECT wallet_address, nonce FROM tx_jobs
                 WHERE nonce IS NOT NULL AND status NOT IN ('Failed', 'Replaced', 'Reorged')
                 GROUP BY chain_id, wallet_address, nonce HAVING COUNT(*) > 1) AS duplicate_nonces)
          + (SELECT COUNT(*) FROM liquidation_jobs
             WHERE status = 'Claimed' AND lease_until <= NOW())
    ") || return 1
    if [[ $violations != 0 ]]
    then
        log "The final recovery check found $violations invariant violation(s)."
        return 1
    fi
}

collect_diagnostics()
{
    log "Collecting final diagnostics."
    kube get pods -n "$NAMESPACE" -o wide > "$RUN_DIR/pods.log" 2>&1 || true
    kube get deployments,statefulsets -n "$NAMESPACE" \
        > "$RUN_DIR/workloads.log" 2>&1 || true
    kube get events -n "$NAMESPACE" --sort-by=.lastTimestamp \
        > "$RUN_DIR/kubernetes-events.log" 2>&1 || true

    local app
    for app in postgres nats anvil rpc-a rpc-b indexer outbox-publisher risk-engine \
        tx-manager liquidator oracle-coordinator api-server
    do
        kube logs -n "$NAMESPACE" -l "app=$app" --all-containers=true \
            --prefix=true --tail=-1 > "$SERVICES_DIR/$app.log" 2>&1 || true
    done

    sql_report "
        SELECT * FROM sync_state ORDER BY chain_id;
        SELECT * FROM chain_versions ORDER BY chain_id;
        SELECT status, COUNT(*) FROM tx_jobs GROUP BY status ORDER BY status;
        SELECT status, COUNT(*) FROM liquidation_jobs GROUP BY status ORDER BY status;
        SELECT COUNT(*) AS unpublished_outbox_events FROM outbox_events WHERE published_at IS NULL;
        SELECT * FROM oracle_leases ORDER BY chain_id, oracle_address;
        SELECT * FROM oracle_rounds ORDER BY chain_id, oracle_address, asset_address;
    " > "$RUN_DIR/database-state.log" 2>&1 || true

    {
        printf '%s\n' '--- /markets ---'
        curl -sS "$API_URL/markets"
        printf '\n%s\n' '--- /liquidations ---'
        curl -sS "$API_URL/liquidations"
        printf '\n%s\n' '--- /protocol/stats ---'
        curl -sS "$API_URL/protocol/stats"
        printf '\n'
    } > "$RUN_DIR/api-state.log" 2>&1 || true
}

finish()
{
    local status=$?
    trap - EXIT
    restore_mutations || true
    collect_diagnostics
    printf '\nRecovery run artifacts:\n'
    printf '  Main log:    %s\n' "$RUN_LOG"
    printf '  Summary:     %s\n' "$SUMMARY_LOG"
    printf '  Diagnostics: %s\n' "$RUN_DIR"
    exit "$status"
}

trap 'exit 130' INT TERM
trap finish EXIT

log "Starting Stage 48 recovery run $RUN_ID."
if [[ $CLEAN == true ]]
then
    "$PROJECT_ROOT/scripts/run-kind.sh" --clean
else
    "$PROJECT_ROOT/scripts/run-kind.sh"
fi
startup_status=$?
if (( startup_status != 0 ))
then
    record_result FAIL "kind environment startup"
    exit "$startup_status"
fi

if [[ ! -f $ENV_FILE ]]
then
    record_result FAIL "kind environment file"
    exit 1
fi
source "$ENV_FILE"

POSTGRES_REPLICAS=$(replica_count statefulset postgres)
NATS_REPLICAS=$(replica_count statefulset nats)
RPC_A_REPLICAS=$(replica_count deployment rpc-a)
LIQUIDATOR_REPLICAS=$(replica_count deployment liquidator)
ORACLE_REPLICAS=$(replica_count deployment oracle-coordinator)
TX_MANAGER_REPLICAS=$(replica_count deployment tx-manager)

if ! wait_until "$WAIT_SECONDS" "PostgreSQL baseline" postgres_ready \
    || ! wait_until "$WAIT_SECONDS" "API baseline" api_ready \
    || ! wait_until "$WAIT_SECONDS" "Oracle baseline" oracle_ready
then
    record_result FAIL "baseline readiness"
    exit 1
fi
record_result PASS "baseline readiness"

run_case "Pod crash and Kubernetes recreation" test_pod_recovery
run_case "PostgreSQL disconnect and reconnect" test_database_reconnect
run_case "JetStream restart and event recovery" test_jetstream_restart
run_case "Primary RPC failure and failover" test_rpc_failover
run_case "Liquidator crash, lease expiry, and fencing takeover" test_liquidator_lease_recovery
run_case "Oracle leader crash and fenced failover" test_oracle_leader_recovery
run_case "Deep chain reorg and canonical state rebuild" test_deep_reorg
run_case "Included transaction reorg" test_transaction_reorg
run_case "Final recovery invariants" test_final_invariants

printf '\n============================================================\n'
if (( FAILED_CASES == 0 ))
then
    log "All Stage 48 recovery cases passed."
    exit 0
fi

log "$FAILED_CASES Stage 48 recovery case(s) failed."
exit 1
