CREATE TABLE IF NOT EXISTS positions
(
    chain_id NUMERIC(78, 0) NOT NULL,
    user_address CHAR(42) NOT NULL,
    weth_collateral NUMERIC(78, 0) NOT NULL,
    scaled_usdc_supply NUMERIC(78, 0) NOT NULL,
    scaled_usdc_debt NUMERIC(78, 0) NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (chain_id, user_address)
);
