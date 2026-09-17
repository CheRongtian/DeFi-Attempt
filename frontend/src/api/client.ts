import type {
  ApiEnvelope,
  Liquidation,
  Market,
  MarketOverview,
  Position,
  ProtocolStats,
  RiskResult,
  RiskSimulationInput,
} from './types'

export class ApiError extends Error {
  constructor(
    message: string,
    public readonly status: number,
  ) {
    super(message)
  }
}

export class ProtocolApi {
  constructor(private readonly baseUrl: string) {}

  private async request<T>(path: string, init?: RequestInit): Promise<T> {
    const response = await fetch(`${this.baseUrl}${path}`, {
      ...init,
      headers: init?.body ? { 'Content-Type': 'application/json', ...init.headers } : init?.headers,
    })
    const body = (await response.json()) as T & { error?: string }
    if (!response.ok) {
      throw new ApiError(body.error ?? `API request failed with status ${response.status}`, response.status)
    }
    return body
  }

  async marketOverview(): Promise<MarketOverview> {
    const [marketResponse, statsResponse] = await Promise.all([
      this.request<ApiEnvelope<Market[]>>('/markets'),
      this.request<ApiEnvelope<ProtocolStats>>('/protocol/stats'),
    ])
    const market = marketResponse.data[0]
    if (!market || !marketResponse.freshness) {
      throw new Error('Market data is unavailable')
    }
    return { market, stats: statsResponse.data, freshness: marketResponse.freshness }
  }

  async position(address: string): Promise<ApiEnvelope<Position> | null> {
    try {
      return await this.request<ApiEnvelope<Position>>(`/positions/${address}`)
    } catch (error) {
      if (error instanceof ApiError && error.status === 404) return null
      throw error
    }
  }

  async liquidations() {
    return this.request<ApiEnvelope<Liquidation[]>>('/liquidations')
  }

  async readiness() {
    return this.request<{ status: string }>('/ready')
  }

  async simulate(input: RiskSimulationInput) {
    return this.request<ApiEnvelope<RiskResult>>('/risk/simulate', {
      method: 'POST',
      body: JSON.stringify(input),
    })
  }
}
