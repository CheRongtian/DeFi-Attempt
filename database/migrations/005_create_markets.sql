CREATE TABLE IF NOT EXISTS markets
(
    chain_id NUMERIC(78, 0) PRIMARY KEY,
    pool_address CHAR(42) NOT NULL,
    oracle_address CHAR(42) NOT NULL,
    weth_address CHAR(42) NOT NULL,
    usdc_address CHAR(42) NOT NULL,
    total_weth_collateral NUMERIC(78, 0) NOT NULL,
    total_scaled_usdc_supply NUMERIC(78, 0) NOT NULL,
    total_scaled_usdc_debt NUMERIC(78, 0) NOT NULL,
    available_usdc_liquidity NUMERIC(78, 0) NOT NULL,
    protocol_reserve NUMERIC(78, 0) NOT NULL,
    bad_debt NUMERIC(78, 0) NOT NULL,
    borrow_index NUMERIC(78, 0) NOT NULL,
    liquidity_index NUMERIC(78, 0) NOT NULL,
    last_interest_timestamp NUMERIC(78, 0) NOT NULL,
    weth_price NUMERIC(78, 0) NOT NULL,
    weth_price_updated_at NUMERIC(78, 0) NOT NULL,
    weth_max_price_age NUMERIC(78, 0) NOT NULL,
    usdc_price NUMERIC(78, 0) NOT NULL,
    usdc_price_updated_at NUMERIC(78, 0) NOT NULL,
    usdc_max_price_age NUMERIC(78, 0) NOT NULL,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
