CREATE TABLE IF NOT EXISTS raw_logs
(
    chain_id NUMERIC(78, 0) NOT NULL,
    block_number BIGINT NOT NULL CHECK (block_number >= 0),
    block_hash CHAR(66) NOT NULL,
    transaction_hash CHAR(66) NOT NULL,
    transaction_index BIGINT NOT NULL CHECK (transaction_index >= 0),
    log_index BIGINT NOT NULL CHECK (log_index >= 0),
    contract_address CHAR(42) NOT NULL,
    topics JSONB NOT NULL,
    data TEXT NOT NULL,
    canonical BOOLEAN NOT NULL DEFAULT TRUE,
    indexed_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (chain_id, block_hash, transaction_hash, log_index)
);

CREATE INDEX IF NOT EXISTS raw_logs_canonical_replay
    ON raw_logs (chain_id, block_number, log_index)
    WHERE canonical;
