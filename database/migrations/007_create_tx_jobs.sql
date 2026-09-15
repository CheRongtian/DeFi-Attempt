CREATE TABLE IF NOT EXISTS tx_jobs
(
    job_id TEXT PRIMARY KEY,
    chain_id NUMERIC(78, 0) NOT NULL,
    wallet_address CHAR(42) NOT NULL,
    to_address CHAR(42) NOT NULL,
    value NUMERIC(78, 0) NOT NULL,
    calldata TEXT NOT NULL,
    status TEXT NOT NULL CHECK
    (
        status IN ('Pending', 'Submitted', 'Included', 'Finalized', 'Failed', 'Replaced', 'Reorged')
    ),
    nonce NUMERIC(78, 0),
    max_priority_fee_per_gas NUMERIC(78, 0),
    max_fee_per_gas NUMERIC(78, 0),
    gas_limit NUMERIC(78, 0),
    raw_transaction TEXT,
    tx_hash CHAR(66),
    retry_count INTEGER NOT NULL DEFAULT 0 CHECK (retry_count >= 0),
    submitted_block_number BIGINT CHECK (submitted_block_number >= 0),
    included_block_number BIGINT CHECK (included_block_number >= 0),
    included_block_hash CHAR(66),
    confirmation_count BIGINT NOT NULL DEFAULT 0 CHECK (confirmation_count >= 0),
    error_message TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS tx_jobs_wallet_status
    ON tx_jobs (chain_id, wallet_address, status, created_at);

CREATE UNIQUE INDEX IF NOT EXISTS tx_jobs_active_nonce
    ON tx_jobs (chain_id, wallet_address, nonce)
    WHERE nonce IS NOT NULL AND status NOT IN ('Failed', 'Replaced', 'Reorged');
