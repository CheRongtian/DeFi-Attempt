import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'

import { App } from './App'
import { ConfigurationError } from './components/ConfigurationError'
import { parseAppConfig } from './config'
import { Providers } from './Providers'
import { createWagmiConfig } from './wagmi'
import './styles.css'

const root = document.getElementById('root')

if (!root) {
  throw new Error('Frontend root element is missing')
}

const config = parseAppConfig(import.meta.env)

createRoot(root).render(
  <StrictMode>
    {config.ok ? (
      <Providers wagmiConfig={createWagmiConfig(config.value)}>
        <App config={config.value} />
      </Providers>
    ) : (
      <ConfigurationError issues={config.issues} />
    )}
  </StrictMode>,
)
