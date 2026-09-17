import { useState, type FormEvent } from 'react'

import type { RiskResult } from '../api/types'
import { formatHealthFactor, shortHex } from '../format'
import type {
  AssetSymbol,
  TransactionAction,
  TransactionProgress,
  TransactionRequest,
} from '../hooks/useProtocolTransaction'
import { ErrorState, StatusBadge } from './States'
import type { Loadable } from './types'

interface TransactionPageProps {
  connected: boolean
  wrongNetwork: boolean
  configurationMismatch: boolean
  busy: boolean
  progress: TransactionProgress
  indexSynced: boolean
  currentHealthFactor?: string
  borrowPreview: Loadable<RiskResult>
  onSubmit: (request: TransactionRequest) => void
  onPreviewBorrow: (amount: string) => void
  onReset: () => void
}

const actionLabels: Record<TransactionAction, string> = {
  supply: 'Supply',
  borrow: 'Borrow',
  repay: 'Repay',
  withdraw: 'Withdraw',
}

function progressLabel(progress: TransactionProgress, indexSynced: boolean) {
  switch (progress.phase) {
    case 'approval-wallet':
      return 'Waiting for token approval confirmation in wallet.'
    case 'approval-pending':
      return 'Token approval submitted and awaiting a receipt.'
    case 'transaction-wallet':
      return progress.message ?? 'Waiting for transaction confirmation in wallet.'
    case 'transaction-pending':
      return 'Transaction submitted and awaiting an on-chain receipt.'
    case 'confirmed':
      return progress.message ?? (indexSynced
        ? 'Transaction confirmed and indexed state refreshed.'
        : 'Transaction confirmed on-chain. The Indexer is still catching up.')
    case 'error':
      return progress.message ?? 'Transaction failed.'
    default:
      return 'No transaction is currently active.'
  }
}

export function TransactionPage({
  connected,
  wrongNetwork,
  configurationMismatch,
  busy,
  progress,
  indexSynced,
  currentHealthFactor,
  borrowPreview,
  onSubmit,
  onPreviewBorrow,
  onReset,
}: TransactionPageProps) {
  const [action, setAction] = useState<TransactionAction>('supply')
  const [asset, setAsset] = useState<AssetSymbol>('USDC')
  const [amount, setAmount] = useState('')
  const [validationError, setValidationError] = useState<string>()

  const changeAction = (nextAction: TransactionAction) => {
    setAction(nextAction)
    if (nextAction === 'borrow' || nextAction === 'repay') setAsset('USDC')
    setValidationError(undefined)
  }

  const submit = (event: FormEvent) => {
    event.preventDefault()
    if (!amount.trim() || Number(amount) <= 0) {
      setValidationError('Enter an amount greater than zero.')
      return
    }
    setValidationError(undefined)
    onSubmit({ action, asset, amount })
  }

  const disabled = !connected || wrongNetwork || configurationMismatch || busy
  const walletState = wrongNetwork
    ? 'Wrong network'
    : configurationMismatch
      ? 'Configuration mismatch'
      : connected
        ? 'Wallet ready'
        : 'Wallet disconnected'
  return (
    <section className="page" aria-labelledby="transactions-title">
      <div className="page-heading">
        <div>
          <p className="eyebrow">Wallet-signed writes</p>
          <h1 id="transactions-title">Transactions</h1>
        </div>
        <StatusBadge tone={wrongNetwork || configurationMismatch ? 'bad' : connected ? 'good' : 'neutral'}>
          {walletState}
        </StatusBadge>
      </div>

      <div className="panel-grid">
        <form className="panel transaction-form" onSubmit={submit}>
          <fieldset className="segmented-control">
            <legend>Action</legend>
            {Object.entries(actionLabels).map(([value, label]) => (
              <button
                key={value}
                className={action === value ? 'selected' : ''}
                type="button"
                aria-pressed={action === value}
                onClick={() => changeAction(value as TransactionAction)}
              >
                {label}
              </button>
            ))}
          </fieldset>

          <label htmlFor="transaction-asset">Asset</label>
          <select
            id="transaction-asset"
            value={asset}
            disabled={action === 'borrow' || action === 'repay'}
            onChange={(event) => setAsset(event.target.value as AssetSymbol)}
          >
            <option value="USDC">USDC</option>
            <option value="WETH">WETH</option>
          </select>

          <label htmlFor="transaction-amount">Amount</label>
          <input
            id="transaction-amount"
            inputMode="decimal"
            value={amount}
            onChange={(event) => setAmount(event.target.value)}
            placeholder={asset === 'USDC' ? '1000' : '1.0'}
            aria-describedby={validationError ? 'transaction-error' : undefined}
          />
          {validationError ? <span id="transaction-error" className="field-error" role="alert">{validationError}</span> : null}

          {action === 'borrow' ? (
            <div className="risk-preview">
              <span>Current Health Factor: {currentHealthFactor ? formatHealthFactor(currentHealthFactor) : 'No indexed position'}</span>
              <button className="button button-secondary" type="button" disabled={!amount || borrowPreview.loading} onClick={() => onPreviewBorrow(amount)}>
                {borrowPreview.loading ? 'Simulating…' : 'Preview borrow risk'}
              </button>
              {borrowPreview.error ? <span className="field-error" role="alert">{borrowPreview.error}</span> : null}
              {borrowPreview.data ? (
                <span>
                  Projected Health Factor: <strong>{formatHealthFactor(borrowPreview.data.healthFactorWad)}</strong>
                  {' · '}{borrowPreview.data.liquidatable ? 'Liquidatable' : 'Above liquidation threshold'}
                </span>
              ) : null}
            </div>
          ) : null}

          <button className="button button-primary button-wide" type="submit" disabled={disabled}>
            {busy ? 'Transaction in progress…' : `${actionLabels[action]} ${asset}`}
          </button>
          {!connected ? <span className="form-hint">Connect a wallet to enable protocol transactions.</span> : null}
          {wrongNetwork ? <span className="form-hint">Switch networks before submitting.</span> : null}
          {configurationMismatch ? <span className="form-hint">Resolve the API and contract configuration mismatch before submitting.</span> : null}
        </form>

        <article className="panel transaction-status" aria-live="polite">
          <p className="card-label">Transaction status</p>
          <h2>{progress.phase === 'idle' ? 'Ready' : progress.phase.replace('-', ' ')}</h2>
          <p>{progressLabel(progress, indexSynced)}</p>
          {progress.hash ? <code title={progress.hash}>{shortHex(progress.hash)}</code> : null}
          {progress.phase !== 'idle' && !busy ? (
            <button className="button button-secondary" type="button" onClick={onReset}>Clear status</button>
          ) : null}
        </article>
      </div>
    </section>
  )
}
