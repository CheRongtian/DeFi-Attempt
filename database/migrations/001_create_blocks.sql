CREATE TABLE IF NOT EXISTS blocks
(
    chain_id NUMERIC(78, 0) NOT NULL,
    block_number BIGINT NOT NULL CHECK (block_number >= 0),
    block_hash CHAR(66) NOT NULL,
    parent_hash CHAR(66) NOT NULL,
    canonical BOOLEAN NOT NULL DEFAULT TRUE,
    indexed_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (chain_id, block_hash)
);

CREATE UNIQUE INDEX IF NOT EXISTS blocks_one_canonical_height
    ON blocks (chain_id, block_number)
    WHERE canonical;

CREATE INDEX IF NOT EXISTS blocks_canonical_tip
    ON blocks (chain_id, block_number DESC)
    WHERE canonical;
