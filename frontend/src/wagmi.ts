import { createConfig, http } from 'wagmi'
import { injected } from 'wagmi/connectors'
import { defineChain } from 'viem'

import type { AppConfig } from './config'

export function createProtocolChain(config: AppConfig) {
  return defineChain({
    id: config.chainId,
    name: config.chainName,
    nativeCurrency: { name: 'Ether', symbol: 'ETH', decimals: 18 },
    rpcUrls: { default: { http: [config.rpcUrl] } },
    testnet: true,
  })
}

export function createWagmiConfig(config: AppConfig) {
  const chain = createProtocolChain(config)
  return createConfig({
    chains: [chain],
    connectors: [injected()],
    transports: { [chain.id]: http(config.rpcUrl) },
  })
}
