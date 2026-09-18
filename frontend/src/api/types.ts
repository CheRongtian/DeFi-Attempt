export interface Freshness {
  indexedBlock: number
  indexedBlockHash: string
  chainHead: number
  indexLag: number
}

export interface Market {
  chainId: string
  pool: string
  oracle: string
  weth: string
  usdc: string
  borrowIndex: string
  wethDecimals: number
  usdcDecimals: number
  priceDecimals: number
  ltvBps: string
  liquidationThresholdBps: string
  wethPrice: string
  usdcPrice: string
}

export interface ProtocolStats {
  positionCount: number
  liquidationCount: number
  totalWethCollateral: string
  totalScaledUsdcDebt: string
  availableUsdcLiquidity: string
  protocolReserve: string
  badDebt: string
}

export interface Position {
  address: string
  wethCollateral: string
  usdcDebt: string
  usdcSupply: string
  collateralValueUsdWad: string
  debtValueUsdWad: string
  maxBorrowUsdWad: string
  healthFactorWad: string
  liquidatable: boolean
}

export interface Liquidation {
  blockNumber: number
  transactionHash: string
  liquidator: string
  borrower: string
  repaidAmount: string
  collateralSeized: string
}

export type RiskResult = Omit<Position, 'usdcSupply'>

export interface MarketOverview {
  market: Market
  stats: ProtocolStats
  freshness: Freshness
}

export interface RiskSimulationInput {
  address: string
  wethCollateral: string
  usdcDebt: string
  wethPrice?: string
  usdcPrice?: string
}

export interface ApiEnvelope<T> {
  data: T
  freshness?: Freshness
}

export type OptionType = 'call' | 'put'

export interface OptionPricingInput {
  optionType: OptionType
  spot: number
  strike: number
  expiryYears: number
  volatility: number
  riskFreeRate: number
  paths: number
  seed: number
}

export interface OptionPricingResult {
  optionType: OptionType
  monteCarloPrice: number
  analyticPrice: number
  standardError: number
  absoluteDifference: number
  paths: number
  elapsedMilliseconds: number
  device: string
}
