import { getAddress, isAddress, type Address } from 'viem'

export interface AppConfig {
  apiBaseUrl: string
  rpcUrl: string
  chainId: number
  chainName: string
  contracts: {
    lendingPool: Address
    priceOracle: Address
    riskManager: Address
    weth: Address
    usdc: Address
  }
}

export type ConfigResult =
  | { ok: true; value: AppConfig }
  | { ok: false; issues: string[] }

type Environment = Record<string, string | undefined>

function readAddress(environment: Environment, key: string, issues: string[]): Address | undefined {
  const value = environment[key]
  if (!value) {
    issues.push(`${key} is missing`)
    return undefined
  }
  if (!isAddress(value)) {
    issues.push(`${key} is not a valid contract address`)
    return undefined
  }
  return getAddress(value)
}

export function parseAppConfig(environment: Environment): ConfigResult {
  const issues: string[] = []
  const apiBaseUrl = environment.VITE_API_BASE_URL?.replace(/\/$/, '')
  const rpcUrl = environment.VITE_RPC_URL
  const chainId = Number(environment.VITE_CHAIN_ID)
  const chainName = environment.VITE_CHAIN_NAME?.trim()

  if (!apiBaseUrl) issues.push('VITE_API_BASE_URL is missing')
  if (!rpcUrl) {
    issues.push('VITE_RPC_URL is missing')
  } else {
    try {
      const parsed = new URL(rpcUrl)
      if (parsed.protocol !== 'http:' && parsed.protocol !== 'https:') {
        issues.push('VITE_RPC_URL must use HTTP or HTTPS')
      }
    } catch {
      issues.push('VITE_RPC_URL is not a valid URL')
    }
  }
  if (!Number.isSafeInteger(chainId) || chainId <= 0) {
    issues.push('VITE_CHAIN_ID must be a positive integer')
  }
  if (!chainName) issues.push('VITE_CHAIN_NAME is missing')

  const lendingPool = readAddress(environment, 'VITE_LENDING_POOL_ADDRESS', issues)
  const priceOracle = readAddress(environment, 'VITE_PRICE_ORACLE_ADDRESS', issues)
  const riskManager = readAddress(environment, 'VITE_RISK_MANAGER_ADDRESS', issues)
  const weth = readAddress(environment, 'VITE_WETH_ADDRESS', issues)
  const usdc = readAddress(environment, 'VITE_USDC_ADDRESS', issues)

  if (
    issues.length > 0 ||
    !apiBaseUrl ||
    !rpcUrl ||
    !chainName ||
    !lendingPool ||
    !priceOracle ||
    !riskManager ||
    !weth ||
    !usdc
  ) {
    return { ok: false, issues }
  }

  return {
    ok: true,
    value: {
      apiBaseUrl,
      rpcUrl,
      chainId,
      chainName,
      contracts: { lendingPool, priceOracle, riskManager, weth, usdc },
    },
  }
}
