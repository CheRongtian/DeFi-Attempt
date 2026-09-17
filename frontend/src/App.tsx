import { useMutation } from '@tanstack/react-query'
import { useMemo, useState } from 'react'
import { parseUnits } from 'viem'
import {
  useConnect,
  useConnection,
  useConnectors,
  useDisconnect,
  useSwitchChain,
} from 'wagmi'

import { ProtocolApi } from './api/client'
import type { RiskSimulationInput } from './api/types'
import { LiquidationsPage } from './components/LiquidationsPage'
import { MarketPage } from './components/MarketPage'
import { PositionPage } from './components/PositionPage'
import { RiskPage } from './components/RiskPage'
import { SystemPage } from './components/SystemPage'
import { TransactionPage } from './components/TransactionPage'
import type { Loadable } from './components/types'
import { WalletStatus } from './components/WalletStatus'
import type { AppConfig } from './config'
import { errorMessage } from './format'
import { useProtocolData } from './hooks/useProtocolData'
import { useProtocolTransaction } from './hooks/useProtocolTransaction'

type Page = 'market' | 'position' | 'transactions' | 'liquidations' | 'system' | 'risk'

const navigation: Array<{ id: Page; label: string }> = [
  { id: 'market', label: 'Market' },
  { id: 'position', label: 'Position' },
  { id: 'transactions', label: 'Transactions' },
  { id: 'liquidations', label: 'Liquidations' },
  { id: 'system', label: 'System' },
  { id: 'risk', label: 'Risk' },
]

interface QueryLike<T> {
  data?: T
  isLoading: boolean
  error: Error | null
}

function loadable<T>(query: QueryLike<T>): Loadable<T> {
  return {
    data: query.data,
    loading: query.isLoading,
    error: query.error ? errorMessage(query.error) : undefined,
  }
}

export function App({ config }: { config: AppConfig }) {
  const [page, setPage] = useState<Page>('market')
  const api = useMemo(() => new ProtocolApi(config.apiBaseUrl), [config.apiBaseUrl])
  const connection = useConnection()
  const connect = useConnect()
  const connectors = useConnectors()
  const disconnect = useDisconnect()
  const switchChain = useSwitchChain()
  const data = useProtocolData(api, connection.address)

  const market = data.market.data?.market
  const apiConfigurationMismatch = Boolean(
    market &&
      (market.chainId !== String(config.chainId) ||
        market.pool.toLowerCase() !== config.contracts.lendingPool.toLowerCase() ||
        market.oracle.toLowerCase() !== config.contracts.priceOracle.toLowerCase()),
  )
  const wrongNetwork = connection.isConnected && connection.chainId !== config.chainId
  const correctNetwork = connection.isConnected && !wrongNetwork && !apiConfigurationMismatch

  const transaction = useProtocolTransaction({
    config,
    address: connection.address,
    correctNetwork,
    onReceipt: data.refreshProtocolState,
  })

  const riskSimulation = useMutation({
    mutationFn: (input: RiskSimulationInput) => api.simulate(input),
  })
  const borrowPreview = useMutation({
    mutationFn: async (amount: string) => {
      const position = data.position.data?.data
      if (!connection.address || !position) {
        throw new Error('An indexed collateral position is required before borrowing.')
      }
      return api.simulate({
        address: connection.address,
        wethCollateral: position.wethCollateral,
        usdcDebt: (BigInt(position.usdcDebt) + parseUnits(amount, 6)).toString(),
      })
    },
  })

  const receiptBlock = transaction.progress.receiptBlock
  const indexSynced = Boolean(
    receiptBlock !== undefined &&
      data.market.data &&
      BigInt(data.market.data.freshness.indexedBlock) >= receiptBlock,
  )

  const renderPage = () => {
    switch (page) {
      case 'market':
        return <MarketPage state={loadable(data.market)} />
      case 'position':
        return (
          <PositionPage
            connected={connection.isConnected}
            wrongNetwork={wrongNetwork}
            address={connection.address}
            state={loadable(data.position)}
          />
        )
      case 'transactions':
        return (
          <TransactionPage
            connected={connection.isConnected}
            wrongNetwork={wrongNetwork}
            configurationMismatch={apiConfigurationMismatch}
            busy={transaction.busy}
            progress={transaction.progress}
            indexSynced={indexSynced}
            currentHealthFactor={data.position.data?.data.healthFactorWad}
            borrowPreview={{
              data: borrowPreview.data?.data,
              loading: borrowPreview.isPending,
              error: borrowPreview.error ? errorMessage(borrowPreview.error) : undefined,
            }}
            onSubmit={(request) => void transaction.execute(request)}
            onPreviewBorrow={(amount) => borrowPreview.mutate(amount)}
            onReset={transaction.reset}
          />
        )
      case 'liquidations':
        return <LiquidationsPage state={loadable(data.liquidations)} />
      case 'system':
        return (
          <SystemPage
            market={loadable(data.market)}
            backendReady={loadable(data.readiness)}
            transaction={transaction.progress}
          />
        )
      case 'risk':
        return (
          <RiskPage
            walletAddress={connection.address}
            result={{
              data: riskSimulation.data?.data,
              loading: riskSimulation.isPending,
              error: riskSimulation.error ? errorMessage(riskSimulation.error) : undefined,
            }}
            onSimulate={(input) => riskSimulation.mutate(input)}
          />
        )
    }
  }

  const walletError = connect.error ?? switchChain.error
  return (
    <>
      <a className="skip-link" href="#main-content">Skip to main content</a>
      <div className="dashboard-shell">
        <header className="dashboard-header">
          <a className="brand" href="/" aria-label="DLP protocol console home">
            <span className="brand-mark" aria-hidden="true">DLP</span>
            <span>Protocol Console</span>
          </a>
          <WalletStatus
            connected={connection.isConnected}
            address={connection.address}
            chainName={connection.chain?.name}
            targetChainName={config.chainName}
            wrongNetwork={wrongNetwork}
            connecting={connect.isPending}
            connectorAvailable={connectors.length > 0}
            error={walletError ? errorMessage(walletError) : undefined}
            onConnect={() => {
              const connector = connectors[0]
              if (connector) connect.mutate({ connector })
            }}
            onDisconnect={() => disconnect.mutate()}
            onSwitchNetwork={() => switchChain.mutate({ chainId: config.chainId })}
          />
        </header>

        <nav className="primary-nav" aria-label="Protocol pages">
          {navigation.map((item) => (
            <button
              key={item.id}
              type="button"
              className={page === item.id ? 'active' : ''}
              aria-current={page === item.id ? 'page' : undefined}
              onClick={() => setPage(item.id)}
            >
              {item.label}
            </button>
          ))}
        </nav>

        {apiConfigurationMismatch ? (
          <div className="configuration-banner" role="alert">
            The API market does not match the configured chain or contract addresses. Wallet transactions are disabled.
          </div>
        ) : null}

        <main id="main-content" className="dashboard-main" tabIndex={-1}>
          {renderPage()}
        </main>
      </div>
    </>
  )
}
