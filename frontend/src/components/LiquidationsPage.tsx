import type { ApiEnvelope, Liquidation } from '../api/types'
import { formatBaseUnits, shortHex } from '../format'
import { EmptyState, ErrorState, LoadingState, StatusBadge } from './States'
import type { Loadable } from './types'

export function LiquidationsPage({ state }: { state: Loadable<ApiEnvelope<Liquidation[]>> }) {
  if (state.loading) return <LoadingState label="Loading liquidation history…" />
  if (state.error) return <ErrorState title="Liquidation history unavailable" error={state.error} />
  if (!state.data || state.data.data.length === 0) {
    return <EmptyState title="No liquidations recorded">Indexed liquidation events will appear here.</EmptyState>
  }

  return (
    <section className="page" aria-labelledby="liquidations-title">
      <div className="page-heading">
        <div>
          <p className="eyebrow">Indexed history</p>
          <h1 id="liquidations-title">Liquidations</h1>
        </div>
        <StatusBadge tone="neutral">{state.data.data.length} records</StatusBadge>
      </div>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th scope="col">Block</th>
              <th scope="col">Borrower</th>
              <th scope="col">Repaid</th>
              <th scope="col">Collateral seized</th>
              <th scope="col">Transaction</th>
            </tr>
          </thead>
          <tbody>
            {state.data.data.map((liquidation) => (
              <tr key={liquidation.transactionHash}>
                <td>{liquidation.blockNumber}</td>
                <td title={liquidation.borrower}>{shortHex(liquidation.borrower)}</td>
                <td>{formatBaseUnits(liquidation.repaidAmount, 6)} USDC</td>
                <td>{formatBaseUnits(liquidation.collateralSeized, 18)} WETH</td>
                <td title={liquidation.transactionHash}>{shortHex(liquidation.transactionHash)}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  )
}
