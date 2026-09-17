import type { MarketOverview } from '../api/types'
import type { TransactionProgress } from '../hooks/useProtocolTransaction'
import { shortHex } from '../format'
import { ErrorState, LoadingState, Metric, StatusBadge } from './States'
import type { Loadable } from './types'

interface SystemPageProps {
  market: Loadable<MarketOverview>
  backendReady: Loadable<{ status: string }>
  transaction: TransactionProgress
}

export function SystemPage({ market, backendReady, transaction }: SystemPageProps) {
  if (market.loading) return <LoadingState label="Loading indexed and observed chain state…" />
  if (market.error || !market.data) {
    return <ErrorState title="System state unavailable" error={market.error ?? 'No freshness data was returned.'} />
  }
  const freshness = market.data.freshness
  const backendAvailable = backendReady.data?.status === 'ready'

  return (
    <section className="page" aria-labelledby="system-title">
      <div className="page-heading">
        <div>
          <p className="eyebrow">Runtime status</p>
          <h1 id="system-title">System Status</h1>
        </div>
        <StatusBadge tone={freshness.indexLag === 0 && backendAvailable ? 'good' : 'warning'}>
          {freshness.indexLag === 0 && backendAvailable ? 'Operational' : 'Degraded'}
        </StatusBadge>
      </div>

      <div className="panel-grid three-columns">
        <article className="panel">
          <p className="card-label">Indexed State</p>
          <dl className="detail-list mono-values">
            <Metric label="Indexed block" value={freshness.indexedBlock} />
            <Metric label="Block hash" value={shortHex(freshness.indexedBlockHash)} detail={freshness.indexedBlockHash} />
          </dl>
        </article>
        <article className="panel">
          <p className="card-label">Observed Chain State</p>
          <dl className="detail-list">
            <Metric label="Chain head" value={freshness.chainHead} />
            <Metric
              label="Index lag"
              value={`${freshness.indexLag} block${freshness.indexLag === 1 ? '' : 's'}`}
            />
            <Metric label="API" value={backendAvailable ? 'Ready' : backendReady.loading ? 'Loading' : 'Unavailable'} />
          </dl>
        </article>
        <article className="panel">
          <p className="card-label">Transaction Execution State</p>
          <dl className="detail-list mono-values">
            <Metric label="Current state" value={transaction.phase} />
            <Metric label="Transaction" value={transaction.hash ? shortHex(transaction.hash) : 'None'} detail={transaction.hash} />
          </dl>
        </article>
      </div>
    </section>
  )
}
