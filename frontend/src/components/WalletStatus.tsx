import { shortHex } from '../format'
import { StatusBadge } from './States'

interface WalletStatusProps {
  connected: boolean
  address?: string
  chainName?: string
  targetChainName: string
  wrongNetwork: boolean
  connecting: boolean
  connectorAvailable: boolean
  error?: string
  onConnect: () => void
  onDisconnect: () => void
  onSwitchNetwork: () => void
}

export function WalletStatus({
  connected,
  address,
  chainName,
  targetChainName,
  wrongNetwork,
  connecting,
  connectorAvailable,
  error,
  onConnect,
  onDisconnect,
  onSwitchNetwork,
}: WalletStatusProps) {
  if (!connected) {
    return (
      <div className="wallet-controls">
        <button className="button button-primary" type="button" onClick={onConnect} disabled={connecting || !connectorAvailable}>
          {connecting ? 'Connecting…' : 'Connect wallet'}
        </button>
        {!connectorAvailable ? <span className="inline-error">Install or enable an injected wallet.</span> : null}
        {error ? <span className="inline-error" role="alert">{error}</span> : null}
      </div>
    )
  }

  return (
    <div className="wallet-controls wallet-connected">
      <div className="wallet-identity">
        <strong>{address ? shortHex(address) : 'Connected wallet'}</strong>
        <span>{chainName ?? 'Unknown network'}</span>
      </div>
      {wrongNetwork ? (
        <button className="button button-warning" type="button" onClick={onSwitchNetwork}>
          Switch to {targetChainName}
        </button>
      ) : (
        <StatusBadge tone="good">Correct network</StatusBadge>
      )}
      <button className="button button-secondary" type="button" onClick={onDisconnect}>
        Disconnect
      </button>
      {error ? <span className="inline-error" role="alert">{error}</span> : null}
    </div>
  )
}
