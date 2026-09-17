import type { ApiEnvelope, Position } from '../api/types'
import { formatBaseUnits, formatHealthFactor, shortHex } from '../format'
import { EmptyState, ErrorState, LoadingState, Metric, StatusBadge } from './States'
import type { Loadable } from './types'

interface PositionPageProps {
  connected: boolean
  wrongNetwork: boolean
  address?: string
  state: Loadable<ApiEnvelope<Position> | null>
}

export function PositionPage({ connected, wrongNetwork, address, state }: PositionPageProps) {
  if (!connected || !address) {
    return <EmptyState title="Wallet disconnected">Connect a wallet to load its indexed lending position.</EmptyState>
  }
  if (wrongNetwork) {
    return <ErrorState title="Wrong network" error="Switch the wallet to the configured protocol network." />
  }
  if (state.loading) return <LoadingState label="Loading wallet position…" />
  if (state.error) return <ErrorState title="Position unavailable" error={state.error} />
  if (!state.data) {
    return <EmptyState title="No indexed position">This wallet has no supplied collateral, liquidity, or debt yet.</EmptyState>
  }

  const position = state.data.data
  return (
    <section className="page" aria-labelledby="position-title">
      <div className="page-heading">
        <div>
          <p className="eyebrow">Wallet position</p>
          <h1 id="position-title">{shortHex(position.address)}</h1>
        </div>
        <StatusBadge tone={position.liquidatable ? 'bad' : 'good'}>
          {position.liquidatable ? 'Liquidatable' : 'Healthy'}
        </StatusBadge>
      </div>

      <dl className="metric-grid">
        <Metric label="WETH collateral" value={`${formatBaseUnits(position.wethCollateral, 18)} WETH`} />
        <Metric label="USDC debt" value={`${formatBaseUnits(position.usdcDebt, 6)} USDC`} />
        <Metric label="USDC supplied" value={`${formatBaseUnits(position.usdcSupply, 6)} USDC`} />
        <Metric label="Health Factor" value={formatHealthFactor(position.healthFactorWad)} />
      </dl>

      <article className="panel">
        <p className="card-label">Deterministic risk state</p>
        <dl className="detail-list">
          <Metric label="Collateral value" value={`${formatBaseUnits(position.collateralValueUsdWad, 18, 2)} USD`} />
          <Metric label="Debt value" value={`${formatBaseUnits(position.debtValueUsdWad, 18, 2)} USD`} />
          <Metric label="Maximum borrow" value={`${formatBaseUnits(position.maxBorrowUsdWad, 18, 2)} USD`} />
          <Metric label="Indexed block" value={state.data.freshness?.indexedBlock ?? 'Unavailable'} />
        </dl>
      </article>
    </section>
  )
}
