import { fireEvent, render, screen } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'

import type { ApiEnvelope, MarketOverview, Position, RiskResult } from './api/types'
import { LiquidationsPage } from './components/LiquidationsPage'
import { MarketPage } from './components/MarketPage'
import { PositionPage } from './components/PositionPage'
import { RiskPage } from './components/RiskPage'
import { SystemPage } from './components/SystemPage'
import { TransactionPage } from './components/TransactionPage'
import { WalletStatus } from './components/WalletStatus'
import { parseAppConfig } from './config'

const address = '0x70997970C51812dc3A010C7d01b50e0d17dc79C8'
const pool = '0xDc64a140Aa3E981100a9becA4E685f962f0cF6C9'
const oracle = '0x9fE46736679d2D9a65F0992F2272dE9f3c7fa6e0'
const transactionHash = `0x${'2'.repeat(64)}` as `0x${string}`

const market: MarketOverview = {
  market: {
    chainId: '31337',
    pool,
    oracle,
    weth: '0x5FbDB2315678afecb367f032d93F642f64180aa3',
    usdc: '0xe7f1725E7734CE288F8367e1Bb143E90bb3F0512',
    borrowIndex: '1000000000000000000000000000',
    wethDecimals: 18,
    usdcDecimals: 6,
    priceDecimals: 8,
    ltvBps: '7500',
    liquidationThresholdBps: '8000',
    wethPrice: '300000000000',
    usdcPrice: '100000000',
  },
  stats: {
    positionCount: 2,
    liquidationCount: 0,
    totalWethCollateral: '10000000000000000000',
    totalScaledUsdcDebt: '10000000000',
    availableUsdcLiquidity: '50000000000',
    protocolReserve: '0',
    badDebt: '0',
  },
  freshness: {
    indexedBlock: 24,
    indexedBlockHash: `0x${'1'.repeat(64)}`,
    chainHead: 25,
    indexLag: 1,
  },
}

const position: ApiEnvelope<Position> = {
  data: {
    address,
    wethCollateral: '10000000000000000000',
    usdcDebt: '10000000000',
    usdcSupply: '7000000000',
    collateralValueUsdWad: '30000000000000000000000',
    debtValueUsdWad: '10000000000000000000000',
    maxBorrowUsdWad: '22500000000000000000000',
    healthFactorWad: '2400000000000000000',
    liquidatable: false,
  },
  freshness: market.freshness,
}

const riskResult: RiskResult = {
  address,
  wethCollateral: '10000000000000000000',
  usdcDebt: '10000000000',
  collateralValueUsdWad: '30000000000000000000000',
  debtValueUsdWad: '10000000000000000000000',
  maxBorrowUsdWad: '22500000000000000000000',
  healthFactorWad: '2400000000000000000',
  liquidatable: false,
}

describe('frontend configuration', () => {
  it('accepts the complete Vite environment', () => {
    const result = parseAppConfig({
      VITE_API_BASE_URL: '/api',
      VITE_RPC_URL: 'http://127.0.0.1:8546',
      VITE_CHAIN_ID: '31337',
      VITE_CHAIN_NAME: 'Anvil Local',
      VITE_LENDING_POOL_ADDRESS: pool,
      VITE_PRICE_ORACLE_ADDRESS: oracle,
      VITE_RISK_MANAGER_ADDRESS: '0xCf7Ed3AccA5a467e9e704C703E8D87F634fB0Fc9',
      VITE_WETH_ADDRESS: market.market.weth,
      VITE_USDC_ADDRESS: market.market.usdc,
    })

    expect(result.ok).toBe(true)
  })

  it('reports missing API and contract configuration', () => {
    const result = parseAppConfig({
      VITE_RPC_URL: 'http://127.0.0.1:8546',
      VITE_CHAIN_ID: '31337',
      VITE_CHAIN_NAME: 'Anvil Local',
    })

    expect(result.ok).toBe(false)
    if (!result.ok) {
      expect(result.issues).toContain('VITE_API_BASE_URL is missing')
      expect(result.issues).toContain('VITE_LENDING_POOL_ADDRESS is missing')
    }
  })
})

describe('dashboard states', () => {
  it('renders loading and API error states', () => {
    const { rerender } = render(<MarketPage state={{ loading: true }} />)
    expect(screen.getByText('Loading indexed market data…')).toBeInTheDocument()

    rerender(<MarketPage state={{ loading: false, error: 'API unavailable' }} />)
    expect(screen.getByRole('alert')).toHaveTextContent('API unavailable')
  })

  it('renders indexed market data and risk configuration', () => {
    render(<MarketPage state={{ data: market, loading: false }} />)

    expect(screen.getByRole('heading', { name: 'WETH / USDC Market' })).toBeInTheDocument()
    expect(screen.getByText('$3,000')).toBeInTheDocument()
    expect(screen.getByText('75%')).toBeInTheDocument()
    expect(screen.getByText('80%')).toBeInTheDocument()
    expect(screen.getByText('1 block behind')).toBeInTheDocument()
  })

  it('renders the disconnected position state and a complete indexed position', () => {
    const { rerender } = render(
      <PositionPage connected={false} wrongNetwork={false} state={{ loading: false }} />,
    )
    expect(screen.getByText('Wallet disconnected')).toBeInTheDocument()

    rerender(
      <PositionPage
        connected
        wrongNetwork={false}
        address={address}
        state={{ data: position, loading: false }}
      />,
    )
    expect(screen.getByText('10 WETH')).toBeInTheDocument()
    expect(screen.getByText('10,000 USDC')).toBeInTheDocument()
    expect(screen.getByText('7,000 USDC')).toBeInTheDocument()
    expect(screen.getByText('2.4')).toBeInTheDocument()
  })

  it('renders wrong-network wallet controls', () => {
    const switchNetwork = vi.fn()
    render(
      <WalletStatus
        connected
        address={address}
        chainName="Ethereum"
        targetChainName="Anvil Local"
        wrongNetwork
        connecting={false}
        connectorAvailable
        onConnect={vi.fn()}
        onDisconnect={vi.fn()}
        onSwitchNetwork={switchNetwork}
      />,
    )

    fireEvent.click(screen.getByRole('button', { name: 'Switch to Anvil Local' }))
    expect(switchNetwork).toHaveBeenCalledOnce()
  })

  it('distinguishes an on-chain receipt from indexed completion', () => {
    render(
      <TransactionPage
        connected
        wrongNetwork={false}
        configurationMismatch={false}
        busy={false}
        progress={{
          phase: 'confirmed',
          hash: transactionHash,
          receiptBlock: 25n,
        }}
        indexSynced={false}
        borrowPreview={{ loading: false }}
        onSubmit={vi.fn()}
        onPreviewBorrow={vi.fn()}
        onReset={vi.fn()}
      />,
    )

    expect(screen.getByText('Transaction confirmed on-chain. The Indexer is still catching up.')).toBeInTheDocument()
  })

  it('renders indexed, observed, and transaction execution state', () => {
    render(
      <SystemPage
        market={{ data: market, loading: false }}
        backendReady={{ data: { status: 'ready' }, loading: false }}
        transaction={{ phase: 'idle' }}
      />,
    )

    expect(screen.getByText('Indexed State')).toBeInTheDocument()
    expect(screen.getByText('Observed Chain State')).toBeInTheDocument()
    expect(screen.getByText('Transaction Execution State')).toBeInTheDocument()
    expect(screen.getByText('1 block')).toBeInTheDocument()
  })

  it('renders indexed liquidation history', () => {
    render(
      <LiquidationsPage
        state={{
          loading: false,
          data: {
            data: [{
              blockNumber: 25,
              transactionHash,
              liquidator: pool,
              borrower: address,
              repaidAmount: '10000000000',
              collateralSeized: '5250000000000000000',
            }],
            freshness: market.freshness,
          },
        }}
      />,
    )

    expect(screen.getByRole('heading', { name: 'Liquidations' })).toBeInTheDocument()
    expect(screen.getByText('10,000 USDC')).toBeInTheDocument()
    expect(screen.getByText('5.25 WETH')).toBeInTheDocument()
  })

  it('renders deterministic risk results returned by the API', () => {
    render(
      <RiskPage
        walletAddress={address}
        result={{ data: riskResult, loading: false }}
        onSimulate={vi.fn()}
      />,
    )

    expect(screen.getByText('Simulation result')).toBeInTheDocument()
    expect(screen.getByText('Healthy')).toBeInTheDocument()
    expect(screen.getByText('22,500 USD')).toBeInTheDocument()
  })
})
