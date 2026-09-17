import { useEffect, useState, type FormEvent } from 'react'
import { isAddress, parseUnits } from 'viem'

import type { RiskResult, RiskSimulationInput } from '../api/types'
import { formatBaseUnits, formatHealthFactor } from '../format'
import { ErrorState, Metric, StatusBadge } from './States'
import type { Loadable } from './types'

interface RiskPageProps {
  walletAddress?: string
  result: Loadable<RiskResult>
  onSimulate: (input: RiskSimulationInput) => void
}

export function RiskPage({ walletAddress, result, onSimulate }: RiskPageProps) {
  const [address, setAddress] = useState(walletAddress ?? '')
  const [wethCollateral, setWethCollateral] = useState('10')
  const [usdcDebt, setUsdcDebt] = useState('10000')
  const [wethPrice, setWethPrice] = useState('3000')
  const [validationError, setValidationError] = useState<string>()

  useEffect(() => {
    if (walletAddress) setAddress(walletAddress)
  }, [walletAddress])

  const submit = (event: FormEvent) => {
    event.preventDefault()
    if (!isAddress(address)) {
      setValidationError('Enter a valid Ethereum address.')
      return
    }
    try {
      const parsedCollateral = parseUnits(wethCollateral, 18)
      const parsedDebt = parseUnits(usdcDebt, 6)
      const parsedWethPrice = parseUnits(wethPrice, 8)
      if (parsedCollateral < 0n || parsedDebt < 0n || parsedWethPrice <= 0n) {
        throw new Error('Invalid risk input')
      }
      const input: RiskSimulationInput = {
        address,
        wethCollateral: parsedCollateral.toString(),
        usdcDebt: parsedDebt.toString(),
        wethPrice: parsedWethPrice.toString(),
        usdcPrice: parseUnits('1', 8).toString(),
      }
      setValidationError(undefined)
      onSimulate(input)
    } catch {
      setValidationError('Collateral and debt must be non-negative, and the WETH price must be greater than zero.')
    }
  }

  return (
    <section className="page" aria-labelledby="risk-title">
      <div className="page-heading">
        <div>
          <p className="eyebrow">Deterministic C++ model</p>
          <h1 id="risk-title">Risk Simulation</h1>
        </div>
        <StatusBadge tone="neutral">No wallet transaction</StatusBadge>
      </div>

      <div className="panel-grid">
        <form className="panel transaction-form" onSubmit={submit}>
          <label htmlFor="risk-address">Position address</label>
          <input
            id="risk-address"
            value={address}
            aria-describedby={validationError ? 'risk-error' : undefined}
            onChange={(event) => setAddress(event.target.value)}
          />
          <label htmlFor="risk-collateral">WETH collateral</label>
          <input
            id="risk-collateral"
            inputMode="decimal"
            value={wethCollateral}
            aria-describedby={validationError ? 'risk-error' : undefined}
            onChange={(event) => setWethCollateral(event.target.value)}
          />
          <label htmlFor="risk-debt">USDC debt</label>
          <input
            id="risk-debt"
            inputMode="decimal"
            value={usdcDebt}
            aria-describedby={validationError ? 'risk-error' : undefined}
            onChange={(event) => setUsdcDebt(event.target.value)}
          />
          <label htmlFor="risk-price">Simulated WETH price (USD)</label>
          <input
            id="risk-price"
            inputMode="decimal"
            value={wethPrice}
            aria-describedby={validationError ? 'risk-error' : undefined}
            onChange={(event) => setWethPrice(event.target.value)}
          />
          {validationError ? <span id="risk-error" className="field-error" role="alert">{validationError}</span> : null}
          <button className="button button-primary button-wide" type="submit" disabled={result.loading}>
            {result.loading ? 'Calculating…' : 'Run deterministic simulation'}
          </button>
        </form>

        <article className="panel">
          <p className="card-label">Simulation result</p>
          {result.error ? <ErrorState title="Simulation failed" error={result.error} /> : null}
          {!result.data && !result.error ? <p>Enter a scenario to calculate its authoritative risk output.</p> : null}
          {result.data ? (
            <>
              <StatusBadge tone={result.data.liquidatable ? 'bad' : 'good'}>
                {result.data.liquidatable ? 'Liquidatable' : 'Healthy'}
              </StatusBadge>
              <dl className="detail-list simulation-results">
                <Metric label="Health Factor" value={formatHealthFactor(result.data.healthFactorWad)} />
                <Metric label="Maximum borrow" value={`${formatBaseUnits(result.data.maxBorrowUsdWad, 18, 2)} USD`} />
                <Metric label="Collateral value" value={`${formatBaseUnits(result.data.collateralValueUsdWad, 18, 2)} USD`} />
                <Metric label="Debt value" value={`${formatBaseUnits(result.data.debtValueUsdWad, 18, 2)} USD`} />
              </dl>
            </>
          ) : null}
        </article>
      </div>
    </section>
  )
}
