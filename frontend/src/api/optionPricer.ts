import type { OptionPricingInput, OptionPricingResult } from './types'

export class OptionPricerApi {
  constructor(private readonly baseUrl: string) {}

  async price(input: OptionPricingInput): Promise<OptionPricingResult> {
    const response = await fetch(`${this.baseUrl}/price`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(input),
    })
    const body = (await response.json()) as OptionPricingResult & { error?: string }
    if (!response.ok) {
      throw new Error(body.error ?? `Metal option pricing failed with status ${response.status}`)
    }
    return body
  }
}
