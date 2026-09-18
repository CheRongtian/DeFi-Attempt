import react from '@vitejs/plugin-react'
import { loadEnv } from 'vite'
import { defineConfig } from 'vitest/config'

export default defineConfig(({ mode }) => {
  const environment = loadEnv(mode, '.')
  const apiTarget = environment.VITE_API_PROXY_TARGET
  const optionPricerTarget = environment.VITE_OPTION_PRICER_PROXY_TARGET
  const proxy = {
    ...(apiTarget
      ? {
          '/api': {
            target: apiTarget,
            changeOrigin: true,
            rewrite: (path: string) => path.replace(/^\/api/, ''),
          },
        }
      : {}),
    ...(optionPricerTarget
      ? {
          '/option-pricer': {
            target: optionPricerTarget,
            changeOrigin: true,
            rewrite: (path: string) => path.replace(/^\/option-pricer/, ''),
          },
        }
      : {}),
  }

  return {
    plugins: [react()],
    server: { proxy: Object.keys(proxy).length > 0 ? proxy : undefined },
    preview: { proxy: Object.keys(proxy).length > 0 ? proxy : undefined },
    test: {
      environment: 'jsdom',
      setupFiles: './src/test/setup.ts',
    },
  }
})
