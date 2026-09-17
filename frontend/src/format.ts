import { formatUnits } from 'viem'

export function formatBaseUnits(value: string, decimals: number, maximumFractionDigits = 4) {
  const [whole, fraction = ''] = formatUnits(BigInt(value), decimals).split('.')
  const visibleFraction = fraction.slice(0, maximumFractionDigits).replace(/0+$/, '')
  const groupedWhole = new Intl.NumberFormat('en-US').format(BigInt(whole))
  return visibleFraction ? `${groupedWhole}.${visibleFraction}` : groupedWhole
}

export function formatUsdPrice(value: string, decimals = 8) {
  return `$${formatBaseUnits(value, decimals, 2)}`
}

export function formatHealthFactor(value: string) {
  const healthFactor = BigInt(value)
  if (healthFactor > 10n ** 50n) return 'No debt'
  return formatBaseUnits(value, 18, 3)
}

export function formatBasisPoints(value: string) {
  return `${formatBaseUnits(value, 2, 2)}%`
}

export function shortHex(value: string) {
  return value.length <= 14 ? value : `${value.slice(0, 8)}…${value.slice(-6)}`
}

export function errorMessage(error: unknown) {
  if (error instanceof Error) return error.message
  return 'An unknown error occurred'
}
