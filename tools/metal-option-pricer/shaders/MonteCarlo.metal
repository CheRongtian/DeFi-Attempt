#include <metal_stdlib>

using namespace metal;

struct PricingParameters
{
    float expiryYears;
    float strike;
    float spot;
    float volatility;
    float riskFreeRate;
    uint paths;
    uint seed;
    uint optionType;
};

uint Mix(uint value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

float Uniform(uint value)
{
    return (float(Mix(value) & 0x00ffffffU) + 1.0F) / 16777218.0F;
}

kernel void priceOptionPaths(
    constant PricingParameters& parameters [[buffer(0)]],
    device float* payoffs [[buffer(1)]],
    uint index [[thread_position_in_grid]]
)
{
    if(index >= parameters.paths)
    {
        return;
    }

    const uint state = parameters.seed ^ (index * 747796405U + 2891336453U);
    const float uniformA = Uniform(state);
    const float uniformB = Uniform(state + 277803737U);
    const float gaussian = sqrt(-2.0F * log(uniformA)) * cos(6.28318530718F * uniformB);
    const float variance = parameters.volatility * parameters.volatility * parameters.expiryYears;
    const float terminalSpot = parameters.spot * exp(
        (parameters.riskFreeRate * parameters.expiryYears - 0.5F * variance) +
        sqrt(variance) * gaussian
    );
    const float payoff = parameters.optionType == 0U
        ? max(terminalSpot - parameters.strike, 0.0F)
        : max(parameters.strike - terminalSpot, 0.0F);
    payoffs[index] = payoff * exp(-parameters.riskFreeRate * parameters.expiryYears);
}
