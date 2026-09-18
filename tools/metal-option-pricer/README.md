# Apple Metal Option Pricer

This host-only macOS analytics tool is derived from the original `FEformula`
Black–Scholes Monte Carlo example. It remains independent from the lending
protocol's deterministic C++ Risk Engine.

The tool provides:

- Black–Scholes analytic call and put prices using CPU `double` precision.
- Monte Carlo call and put prices computed by an Apple Metal shader.
- Standard error and absolute difference from the analytic result.
- A loopback HTTP endpoint used by the optional Frontend panel.

## Build

```bash
cmake \
  -S tools/metal-option-pricer \
  -B build/metal-option-pricer

cmake --build build/metal-option-pricer --parallel
```

The build needs the macOS Metal toolchain provided by Xcode Command Line Tools,
Boost.System, and nlohmann-json.

## Run the full demo

```bash
./scripts/run-final-demo.sh --clean
```

The final-demo launcher builds and starts the host Metal service, starts the
Frontend with `.env.kind`, and stops the Metal process when the Frontend exits.
Open the Risk page, then enable `Apple Metal option analytics`.

For standalone Metal development:

```bash
./scripts/metal-option-pricer.sh
```

The service binds to `127.0.0.1:18081`.

The service does not connect to a wallet, RPC endpoint, blockchain, Docker, or
Kubernetes.
