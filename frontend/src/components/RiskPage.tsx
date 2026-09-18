import { useEffect, useState, type FormEvent } from 'react'
import { isAddress, parseUnits } from 'viem'

import type {
  OptionPricingInput,
  OptionPricingResult,
  OptionType,
  RiskResult,
  RiskSimulationInput,
} from '../api/types'
import { formatBaseUnits, formatHealthFactor } from '../format'
import { ErrorState, Metric, StatusBadge } from './States'
import type { Loadable } from './types'

interface OptionPricingControl {
  result: Loadable<OptionPricingResult>
  onPrice: (input: OptionPricingInput) => void
}

interface RiskPageProps {
  walletAddress?: string
  result: Loadable<RiskResult>
  onSimulate: (input: RiskSimulationInput) => void
  optionPricing?: OptionPricingControl
}

const usd = new Intl.NumberFormat('en-US', {
  style: 'currency',
  currency: 'USD',
  minimumFractionDigits: 2,
  maximumFractionDigits: 4,
})

export function RiskPage({ walletAddress, result, onSimulate, optionPricing }: RiskPageProps) {
  const [address, setAddress] = useState(walletAddress ?? '')
  const [wethCollateral, setWethCollateral] = useState('10')
  const [usdcDebt, setUsdcDebt] = useState('10000')
  const [wethPrice, setWethPrice] = useState('3000')
  const [validationError, setValidationError] = useState<string>()
  const [metalEnabled, setMetalEnabled] = useState(false)
  const [optionType, setOptionType] = useState<OptionType>('call')
  const [spot, setSpot] = useState('100')
  const [strike, setStrike] = useState('100')
  const [expiryYears, setExpiryYears] = useState('1')
  const [volatilityPercent, setVolatilityPercent] = useState('20')
  const [riskFreeRatePercent, setRiskFreeRatePercent] = useState('4')
  const [paths, setPaths] = useState('1000000')
  const [seed, setSeed] = useState('42')
  const [optionValidationError, setOptionValidationError] = useState<string>()

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

  const submitOptionPricing = (event: FormEvent) => {
    event.preventDefault()
    if (!optionPricing) return

    const input: OptionPricingInput = {
      optionType,
      spot: Number(spot),
      strike: Number(strike),
      expiryYears: Number(expiryYears),
      volatility: Number(volatilityPercent) / 100,
      riskFreeRate: Number(riskFreeRatePercent) / 100,
      paths: Number(paths),
      seed: Number(seed),
    }
    const valid =
      Number.isFinite(input.spot) && input.spot > 0 &&
      Number.isFinite(input.strike) && input.strike > 0 &&
      Number.isFinite(input.expiryYears) && input.expiryYears > 0 &&
      Number.isFinite(input.volatility) && input.volatility > 0 &&
      Number.isFinite(input.riskFreeRate) &&
      Number.isSafeInteger(input.paths) && input.paths > 0 && input.paths <= 10_000_000 &&
      Number.isSafeInteger(input.seed) && input.seed >= 0 && input.seed <= 4_294_967_295

    if (!valid) {
      setOptionValidationError('Use positive market values, 1 to 10000000 paths, and a seed from 0 to 4294967295.')
      return
    }

    setOptionValidationError(undefined)
    optionPricing.onPrice(input)
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

      {optionPricing ? (
        <section className="optional-feature" aria-labelledby="metal-option-title">
          <div className="panel feature-toggle-panel">
            <div>
              <p className="card-label">Optional local analytics</p>
              <h2 id="metal-option-title">Apple Metal option analytics</h2>
              <p id="metal-option-description">
                Runs a local Black–Scholes Monte Carlo estimate on the Mac GPU. It does not affect lending risk or send a transaction.
              </p>
            </div>
            <label className="switch-control">
              <input
                type="checkbox"
                role="switch"
                checked={metalEnabled}
                aria-describedby="metal-option-description metal-option-state"
                onChange={(event) => setMetalEnabled(event.target.checked)}
              />
              <span className="switch-track" aria-hidden="true"><span /></span>
              <span id="metal-option-state">{metalEnabled ? 'Enabled' : 'Disabled'}</span>
            </label>
          </div>

          {metalEnabled ? (
            <div className="panel-grid">
              <form className="panel transaction-form" onSubmit={submitOptionPricing}>
                <label htmlFor="option-type">Option type</label>
                <select id="option-type" value={optionType} onChange={(event) => setOptionType(event.target.value as OptionType)}>
                  <option value="call">Call</option>
                  <option value="put">Put</option>
                </select>
                <label htmlFor="option-spot">Spot price (USD)</label>
                <input id="option-spot" inputMode="decimal" value={spot} onChange={(event) => setSpot(event.target.value)} />
                <label htmlFor="option-strike">Strike price (USD)</label>
                <input id="option-strike" inputMode="decimal" value={strike} onChange={(event) => setStrike(event.target.value)} />
                <label htmlFor="option-expiry">Expiry (years)</label>
                <input id="option-expiry" inputMode="decimal" value={expiryYears} onChange={(event) => setExpiryYears(event.target.value)} />
                <label htmlFor="option-volatility">Volatility (%)</label>
                <input id="option-volatility" inputMode="decimal" value={volatilityPercent} onChange={(event) => setVolatilityPercent(event.target.value)} />
                <label htmlFor="option-rate">Risk-free rate (%)</label>
                <input id="option-rate" inputMode="decimal" value={riskFreeRatePercent} onChange={(event) => setRiskFreeRatePercent(event.target.value)} />
                <label htmlFor="option-paths">Monte Carlo paths</label>
                <input id="option-paths" inputMode="numeric" value={paths} onChange={(event) => setPaths(event.target.value)} />
                <label htmlFor="option-seed">Random seed</label>
                <input id="option-seed" inputMode="numeric" value={seed} onChange={(event) => setSeed(event.target.value)} />
                {optionValidationError ? <span className="field-error" role="alert">{optionValidationError}</span> : null}
                <button className="button button-primary button-wide" type="submit" disabled={optionPricing.result.loading}>
                  {optionPricing.result.loading ? 'Running on Metal…' : 'Price with Apple Metal'}
                </button>
              </form>

              <article className="panel" aria-live="polite">
                <p className="card-label">Option pricing result</p>
                {optionPricing.result.error ? (
                  <ErrorState title="Metal pricing unavailable" error={optionPricing.result.error} />
                ) : null}
                {!optionPricing.result.data && !optionPricing.result.error ? (
                  <p>Start the local Metal service, then run a call or put scenario.</p>
                ) : null}
                {optionPricing.result.data ? (
                  <>
                    <StatusBadge tone="good">Completed on {optionPricing.result.data.device}</StatusBadge>
                    <dl className="detail-list simulation-results">
                      <Metric label="Metal Monte Carlo" value={usd.format(optionPricing.result.data.monteCarloPrice)} />
                      <Metric label="Black–Scholes analytic" value={usd.format(optionPricing.result.data.analyticPrice)} />
                      <Metric label="Standard error" value={usd.format(optionPricing.result.data.standardError)} />
                      <Metric label="Absolute difference" value={usd.format(optionPricing.result.data.absoluteDifference)} />
                      <Metric label="Paths" value={optionPricing.result.data.paths.toLocaleString('en-US')} />
                      <Metric label="Elapsed time" value={`${optionPricing.result.data.elapsedMilliseconds.toFixed(2)} ms`} />
                    </dl>
                  </>
                ) : null}
              </article>
            </div>
          ) : null}
        </section>
      ) : null}
    </section>
  )
}
