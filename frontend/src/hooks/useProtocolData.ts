import { useQuery, useQueryClient } from '@tanstack/react-query'
import { useCallback } from 'react'

import { ProtocolApi } from '../api/client'

const REFRESH_INTERVAL_MS = 3_000

export function useProtocolData(api: ProtocolApi, address?: string) {
  const queryClient = useQueryClient()
  const market = useQuery({
    queryKey: ['market'],
    queryFn: () => api.marketOverview(),
    refetchInterval: REFRESH_INTERVAL_MS,
  })
  const position = useQuery({
    queryKey: ['position', address],
    queryFn: () => api.position(address!),
    enabled: Boolean(address),
    refetchInterval: address ? REFRESH_INTERVAL_MS : false,
  })
  const liquidations = useQuery({
    queryKey: ['liquidations'],
    queryFn: () => api.liquidations(),
    refetchInterval: REFRESH_INTERVAL_MS,
  })
  const readiness = useQuery({
    queryKey: ['readiness'],
    queryFn: () => api.readiness(),
    refetchInterval: REFRESH_INTERVAL_MS,
  })

  const refreshProtocolState = useCallback(async () => {
    await queryClient.invalidateQueries({
      predicate: ({ queryKey }) =>
        ['market', 'position', 'liquidations', 'readiness'].includes(String(queryKey[0])),
    })
  }, [queryClient])

  return { market, position, liquidations, readiness, refreshProtocolState }
}
