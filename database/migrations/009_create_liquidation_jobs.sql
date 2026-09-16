CREATE TABLE IF NOT EXISTS liquidation_jobs
(
    job_id TEXT PRIMARY KEY,
    source_event_id TEXT NOT NULL UNIQUE,
    chain_id NUMERIC(78, 0) NOT NULL,
    borrower_address CHAR(42) NOT NULL,
    debt_asset CHAR(42) NOT NULL,
    collateral_asset CHAR(42) NOT NULL,
    health_factor NUMERIC(78, 0) NOT NULL,
    max_repay NUMERIC(78, 0) NOT NULL,
    expected_bonus NUMERIC(78, 0) NOT NULL,
    expected_collateral NUMERIC(78, 0) NOT NULL,
    expected_bad_debt NUMERIC(78, 0) NOT NULL,
    block_number BIGINT NOT NULL CHECK (block_number >= 0),
    block_hash CHAR(66) NOT NULL,
    canonical_version BIGINT NOT NULL CHECK (canonical_version > 0),
    status TEXT NOT NULL CHECK
    (
        status IN ('Available', 'Claimed', 'Submitted', 'Completed', 'Expired', 'Failed', 'Invalidated')
    ),
    worker_id TEXT,
    lease_until TIMESTAMPTZ,
    fencing_token BIGINT NOT NULL DEFAULT 0 CHECK (fencing_token >= 0),
    tx_job_id TEXT REFERENCES tx_jobs(job_id),
    error_message TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS liquidation_jobs_claimable
    ON liquidation_jobs (status, created_at);

CREATE INDEX IF NOT EXISTS liquidation_jobs_submitted
    ON liquidation_jobs (status, tx_job_id)
    WHERE status = 'Submitted';
