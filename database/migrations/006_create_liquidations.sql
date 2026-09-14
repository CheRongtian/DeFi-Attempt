CREATE TABLE IF NOT EXISTS liquidations
(
    chain_id NUMERIC(78, 0) NOT NULL,
    block_number BIGINT NOT NULL CHECK (block_number >= 0),
    block_hash CHAR(66) NOT NULL,
    transaction_hash CHAR(66) NOT NULL,
    log_index BIGINT NOT NULL CHECK (log_index >= 0),
    liquidator_address CHAR(42) NOT NULL,
    borrower_address CHAR(42) NOT NULL,
    debt_asset CHAR(42) NOT NULL,
    collateral_asset CHAR(42) NOT NULL,
    repaid_amount NUMERIC(78, 0) NOT NULL,
    collateral_seized NUMERIC(78, 0) NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (chain_id, block_hash, transaction_hash, log_index)
);

CREATE INDEX IF NOT EXISTS liquidations_by_borrower
    ON liquidations (chain_id, borrower_address, block_number DESC);
