CREATE TABLE IF NOT EXISTS oracle_leases
(
    chain_id NUMERIC(78, 0) NOT NULL,
    oracle_address CHAR(42) NOT NULL,
    holder TEXT NOT NULL,
    lease_until TIMESTAMPTZ NOT NULL,
    fencing_token BIGINT NOT NULL CHECK (fencing_token > 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (chain_id, oracle_address)
);

CREATE TABLE IF NOT EXISTS oracle_rounds
(
    chain_id NUMERIC(78, 0) NOT NULL,
    oracle_address CHAR(42) NOT NULL,
    asset_address CHAR(42) NOT NULL,
    last_round_id BIGINT NOT NULL DEFAULT 0 CHECK (last_round_id >= 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (chain_id, oracle_address, asset_address)
);

CREATE TABLE IF NOT EXISTS oracle_publications
(
    publication_id TEXT PRIMARY KEY,
    chain_id NUMERIC(78, 0) NOT NULL,
    oracle_address CHAR(42) NOT NULL,
    asset_address CHAR(42) NOT NULL,
    price NUMERIC(78, 0) NOT NULL CHECK (price > 0),
    reported_at BIGINT NOT NULL CHECK (reported_at > 0),
    round_id BIGINT NOT NULL CHECK (round_id > 0),
    fencing_token BIGINT NOT NULL CHECK (fencing_token > 0),
    tx_job_id TEXT NOT NULL UNIQUE REFERENCES tx_jobs(job_id),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (chain_id, oracle_address, asset_address, round_id)
);
