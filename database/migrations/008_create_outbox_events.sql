CREATE TABLE IF NOT EXISTS chain_versions
(
    chain_id NUMERIC(78, 0) PRIMARY KEY,
    canonical_version BIGINT NOT NULL CHECK (canonical_version > 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

INSERT INTO chain_versions (chain_id, canonical_version)
SELECT chain_id, 1
FROM sync_state
ON CONFLICT (chain_id) DO NOTHING;

CREATE TABLE IF NOT EXISTS outbox_events
(
    event_id TEXT PRIMARY KEY,
    event_type TEXT NOT NULL,
    aggregate_id TEXT NOT NULL,
    chain_id NUMERIC(78, 0) NOT NULL,
    block_number BIGINT NOT NULL CHECK (block_number >= 0),
    block_hash CHAR(66) NOT NULL,
    transaction_hash CHAR(66),
    log_index BIGINT CHECK (log_index >= 0),
    canonical_version BIGINT NOT NULL CHECK (canonical_version > 0),
    payload JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    published_at TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS outbox_events_unpublished
    ON outbox_events (created_at, event_id)
    WHERE published_at IS NULL;

CREATE TABLE IF NOT EXISTS processed_events
(
    consumer_name TEXT NOT NULL,
    event_id TEXT NOT NULL,
    processed_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (consumer_name, event_id)
);
