# Apple Metal Option Pricer

This optional host-native macOS tool prices European call and put options with an Apple Metal Monte Carlo kernel and compares the result with a CPU `double` Black–Scholes reference.

It is an independent analytics feature. It does not connect to a wallet, RPC endpoint, blockchain, PostgreSQL, Docker, or Kubernetes, and it does not participate in lending Health Factor or liquidation decisions.

## Requirements

- macOS with a Metal-capable Apple GPU
- Xcode Command Line Tools with `metal` and `metallib`
- CMake 3.20+
- C++20 / Objective-C++ compiler
- Boost.System 1.74+
- nlohmann/json 3.10+

## Build

From the repository root:

```bash
./scripts/metal-option-pricer.sh build
```

Equivalent CMake commands:

```bash
cmake -S tools/metal-option-pricer -B build/metal-option-pricer
cmake --build build/metal-option-pricer --parallel
```

The build compiles `shaders/MonteCarlo.metal` into `DlpOptionPricer.metallib` and links the HTTP/CLI executable.

## Run

Build and serve:

```bash
./scripts/metal-option-pricer.sh
```

Serve an existing build:

```bash
./scripts/metal-option-pricer.sh serve
```

Default address: `http://127.0.0.1:18081`.

Health endpoint:

```bash
curl -sS http://127.0.0.1:18081/health
```

The root path has no page and returns `route not found` by design.

## HTTP Pricing API

```bash
curl -sS http://127.0.0.1:18081/price \
  -H 'Content-Type: application/json' \
  -d '{
    "optionType": "call",
    "spot": 3000,
    "strike": 3200,
    "expiryYears": 0.5,
    "volatility": 0.6,
    "riskFreeRate": 0.04,
    "paths": 1000000,
    "seed": 42
  }'
```

The response includes:

```text
monteCarloPrice
analyticPrice
standardError
absoluteDifference
paths
elapsedMilliseconds
device
```

`optionType` accepts `call` or `put`. Paths must be between 1 and 10,000,000.

## CLI Pricing

```bash
./build/metal-option-pricer/dlp_metal_option_pricer \
  price call 3000 3200 0.5 0.6 0.04 1000000 42
```

Arguments are option type, spot, strike, expiry in years, volatility, risk-free rate, path count, and optional seed.

## Frontend Integration

The complete macOS demo starts the service automatically:

```bash
./scripts/run-final-demo.sh --clean
```

The Frontend proxies `/option-pricer` to the host service. The Risk page calls it only after the user enables `Apple Metal option analytics`; the switch is disabled by default.

## Numerical Output

The GPU simulation reports the discounted Monte Carlo mean and standard error. The CPU Black–Scholes value is an analytic comparison for the same European option parameters. `absoluteDifference` is diagnostic and has no protocol meaning.
