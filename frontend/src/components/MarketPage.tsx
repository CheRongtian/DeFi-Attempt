import type { MarketOverview } from '../api/types'
import { formatBaseUnits, formatBasisPoints, formatUsdPrice, shortHex } from '../format'
import { ErrorState, LoadingState, Metric, StatusBadge } from './States'
import type { Loadable } from './types'

export function MarketPage({ state }: { state: Loadable<MarketOverview> }) {
  if (state.loading) return <LoadingState label="Loading indexed market data…" />
  if (state.error) return <ErrorState title="Market data unavailable" error={state.error} />
  if (!state.data) return <ErrorState title="Market data unavailable" error="The API returned no market." />

  const { market, stats, freshness } = state.data
  return (
    <section className="page" aria-labelledby="market-title">
      <div className="page-heading">
        <div>
          <p className="eyebrow">Indexed market</p>
          <h1 id="market-title">WETH / USDC Market</h1>
        </div>
        <StatusBadge tone={freshness.indexLag === 0 ? 'good' : 'warning'}>
          {freshness.indexLag === 0
            ? 'Synced'
            : `${freshness.indexLag} block${freshness.indexLag === 1 ? '' : 's'} behind`}
        </StatusBadge>
      </div>

      <dl className="metric-grid">
        <Metric label="WETH price" value={formatUsdPrice(market.wethPrice, market.priceDecimals)} />
        <Metric label="USDC price" value={formatUsdPrice(market.usdcPrice, market.priceDecimals)} />
        <Metric label="Available liquidity" value={`${formatBaseUnits(stats.availableUsdcLiquidity, market.usdcDecimals, 2)} USDC`} />
        <Metric label="Open positions" value={stats.positionCount.toLocaleString()} />
      </dl>

      <div className="panel-grid">
        <article className="panel">
          <p className="card-label">Risk configuration</p>
          <dl className="detail-list">
            <Metric label="Maximum LTV" value={formatBasisPoints(market.ltvBps)} />
            <Metric label="Liquidation threshold" value={formatBasisPoints(market.liquidationThresholdBps)} />
            <Metric label="Borrow index" value={formatBaseUnits(market.borrowIndex, 27, 6)} />
          </dl>
        </article>
        <article className="panel">
          <p className="card-label">Contracts</p>
          <dl className="detail-list mono-values">
            <Metric label="LendingPool" value={shortHex(market.pool)} detail={market.pool} />
            <Metric label="PriceOracle" value={shortHex(market.oracle)} detail={market.oracle} />
            <Metric label="Chain ID" value={market.chainId} />
          </dl>
        </article>
      </div>
    </section>
  )
}
