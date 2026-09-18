# Development Environment

## Prerequisites

- macOS or Linux
- Bash or Zsh, `curl`, `jq`, and Python 3
- Foundry (`forge`, `cast`, and `anvil`)
- CMake 3.20 or newer and a C++20 compiler
- Boost 1.74+, nlohmann/json 3.10+, GoogleTest, libpq, libpqxx 8, and libsecp256k1
- Go 1.25+
- Node.js 22+ and npm
- Docker with Docker Compose
- `kubectl` and kind for Kubernetes workflows
- Xcode Command Line Tools and the Metal compiler for the optional macOS option pricer

The first CMake or Docker build downloads pinned source releases for Ethereum Keccak, prometheus-cpp, and nats.c.

## macOS Packages

With Homebrew:

```bash
brew install cmake boost nlohmann-json googletest libpq libpqxx libsecp256k1 pkg-config
brew install kind kubectl
```

Install Foundry using its official installer:

```bash
curl -L https://getfoundry.sh/install | bash
foundryup
```

Ensure the Foundry binaries are on `PATH` in new shells.

## Solidity

```bash
cd contracts
forge build
forge test
forge fmt --check
```

Foundry installs the configured Solc version on first use. Generated `contracts/out/`, `contracts/cache/`, and broadcast artifacts are ignored.

## C++20

Configure from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

The root build includes:

```text
cpp/common
cpp/observability
cpp/messaging
cpp/indexer
cpp/risk-engine
cpp/tx-manager
cpp/liquidator
cpp/api-server
```

`cmake/HomebrewPostgreSQL.cmake` helps CMake locate Homebrew's keg-only PostgreSQL client files on macOS.

## Go

```bash
cd go/oracle-coordinator
go mod download
go test ./...
```

The checked-in `go.mod` and `go.sum` define the dependency set. `go mod tidy` is only needed when module imports change.

## Frontend

```bash
cd frontend
npm install
npm run build
npm test
```

For interactive development, generate the correct environment first and then use `npm run dev`. See [frontend/README.md](../frontend/README.md).

## Apple Metal Tool

On macOS:

```bash
./scripts/metal-option-pricer.sh build
./scripts/metal-option-pricer.sh serve
```

The Metal tool has its own CMake project under `tools/metal-option-pricer/` and is not part of the root C++ build.

## Environment Files

Generated files are shell-compatible exports:

| File | Generated or created by | Use |
|---|---|---|
| `.env.local` | `deploy-local.sh` / `run-local.sh` | Native services |
| `.env.containers` | `run-containers.sh` | Docker Compose services |
| `.env.kind` | `run-kind.sh` | kind services and Frontend |
| `.env.sepolia` | copied from `.env.sepolia.example` | Read-only Sepolia endpoints |
| `frontend/.env.local` | `configure-frontend.sh` | Vite runtime configuration |

Deployment scripts create generated environment files with owner-only permissions. For the manually created Sepolia file:

```bash
cp .env.sepolia.example .env.sepolia
chmod 600 .env.sepolia
```

Never put private keys, seed phrases, or private API credentials in a `VITE_` variable; Vite embeds those values in browser assets.

## Development Credentials

The repository uses public Anvil development accounts and local `dlp` database credentials in disposable environments. These values are not suitable for any public network or shared production system.

## Next Steps

- [Operations](OPERATIONS.md) explains each runtime mode.
- [Testing](TESTING.md) maps tests to their required dependencies.
- [Security](SECURITY.md) documents key and trust boundaries.
