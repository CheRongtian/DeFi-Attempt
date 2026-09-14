CREATE TABLE IF NOT EXISTS sync_state
(
    chain_id NUMERIC(78, 0) PRIMARY KEY,
    block_number BIGINT NOT NULL CHECK (block_number >= 0),
    block_hash CHAR(66) NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
