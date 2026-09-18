# 分布式 DeFi 借贷协议

[English](README.md)

一个使用 Solidity 开发的超额抵押借贷协议，包含索引、风险扫描、交易管理、自动清算和 PostgreSQL REST API 等 C++20 服务。Go Oracle Coordinator 通过 Leader Election 和数据库 Fencing 聚合并发布价格。完整系统可通过 Docker Compose 或本地 kind 集群运行，并按请求用途进行 RPC 故障切换。在 macOS 上，Frontend 可以选择调用宿主机原生 Apple Metal 期权定价工具。

## 项目结构

```text
DeFi/
├── contracts/
│   ├── foundry.toml
│   ├── lib/
│   │   └── openzeppelin-contracts/
│   ├── src/
│   │   ├── InterestRateModel.sol
│   │   ├── LendingPool.sol
│   │   ├── LiquidationManager.sol
│   │   ├── PriceOracle.sol
│   │   ├── RiskManager.sol
│   │   ├── interfaces/
│   │   │   └── IIndexProvider.sol
│   │   ├── libraries/
│   │   │   └── MathLib.sol
│   │   ├── mocks/
│   │   │   ├── MockUSDC.sol
│   │   │   └── MockWETH.sol
│   │   └── tokens/
│   │       ├── DebtToken.sol
│   │       └── DepositToken.sol
│   └── test/
│       ├── DebtToken.t.sol
│       ├── DepositToken.t.sol
│       ├── InterestAccounting.t.sol
│       ├── InterestFuzz.t.sol
│       ├── InterestGolden.t.sol
│       ├── InterestRateModel.t.sol
│       ├── LendingPool.t.sol
│       ├── LendingPoolIntegration.t.sol
│       ├── LendingMvpFuzz.t.sol
│       ├── LendingMvpInvariant.t.sol
│       ├── LiquidationGolden.t.sol
│       ├── LiquidationManager.t.sol
│       ├── MathLibGolden.t.sol
│       ├── MathLib.t.sol
│       ├── MockUSDC.t.sol
│       ├── MockWETH.t.sol
│       ├── PriceOracle.t.sol
│       ├── RiskManagerGolden.t.sol
│       ├── RiskManager.t.sol
│       └── Smoke.t.sol
├── cpp/
│   ├── common/
│   │   ├── include/dlp/ethereum/
│   │   │   ├── Abi.hpp
│   │   │   ├── Address.hpp
│   │   │   ├── Hex.hpp
│   │   │   ├── Keccak.hpp
│   │   │   ├── ProtocolAbi.hpp
│   │   │   ├── Rlp.hpp
│   │   │   ├── RpcClient.hpp
│   │   │   ├── RpcEndpoints.hpp
│   │   │   ├── Transaction.hpp
│   │   │   ├── Uint256.hpp
│   │   │   └── Uint256Math.hpp
│   │   ├── smoke/
│   │   │   └── Smoke.cpp
│   │   ├── src/
│   │   │   ├── Abi.cpp
│   │   │   ├── Address.cpp
│   │   │   ├── Hex.cpp
│   │   │   ├── Keccak.cpp
│   │   │   ├── ProtocolAbi.cpp
│   │   │   ├── Rlp.cpp
│   │   │   ├── RpcClient.cpp
│   │   │   ├── RpcEndpoints.cpp
│   │   │   ├── Transaction.cpp
│   │   │   ├── Uint256.cpp
│   │   │   └── Uint256Math.cpp
│   │   ├── tests/
│   │   │   ├── AbiTests.cpp
│   │   │   ├── AddressTests.cpp
│   │   │   ├── HexTests.cpp
│   │   │   ├── KeccakTests.cpp
│   │   │   ├── ProtocolAbiTests.cpp
│   │   │   ├── RlpTests.cpp
│   │   │   ├── RpcClientIntegrationTests.cpp
│   │   │   ├── RpcEndpointsTests.cpp
│   │   │   ├── TransactionTests.cpp
│   │   │   ├── Uint256Tests.cpp
│   │   │   └── Uint256MathTests.cpp
│   │   └── CMakeLists.txt
│   ├── messaging/
│   │   ├── include/dlp/messaging/
│   │   ├── src/
│   │   └── tests/
│   ├── observability/
│   │   ├── include/dlp/observability/
│   │   └── src/
│   ├── indexer/
│   │   ├── include/dlp/indexer/
│   │   ├── src/
│   │   ├── tests/
│   │   └── CMakeLists.txt
│   ├── risk-engine/
│   │   ├── include/dlp/risk/
│   │   ├── src/
│   │   └── tests/
│   ├── tx-manager/
│   │   ├── include/dlp/tx/
│   │   ├── src/
│   │   └── tests/
│   ├── liquidator/
│   │   ├── include/dlp/liquidator/
│   │   ├── src/
│   │   └── tests/
│   └── api-server/
│       ├── include/dlp/api/
│       ├── src/
│       └── tests/
├── database/
│   └── migrations/
│       ├── 001_create_blocks.sql
│       ├── 002_create_raw_logs.sql
│       ├── 003_create_sync_state.sql
│       ├── 004_create_positions.sql
│       ├── 005_create_markets.sql
│       ├── 006_create_liquidations.sql
│       ├── 007_create_tx_jobs.sql
│       ├── 008_create_outbox_events.sql
│       ├── 009_create_liquidation_jobs.sql
│       └── 010_create_oracle_publications.sql
├── frontend/
│   ├── src/
│   │   ├── api/
│   │   ├── components/
│   │   └── hooks/
│   ├── .env.example
│   ├── package.json
│   └── vite.config.ts
├── go/
│   └── oracle-coordinator/
│       ├── cmd/oracle-coordinator/
│       ├── internal/oracle/
│       ├── go.mod
│       └── go.sum
├── infrastructure/
│   └── rpc-proxy/
├── k8s/
│   ├── applications.yaml
│   ├── infrastructure.yaml
│   ├── kind-config.yaml
│   ├── migrations-job.yaml
│   └── observability.yaml
├── observability/
│   ├── grafana/
│   └── prometheus/
├── scripts/
│   ├── check-sepolia-rpc.sh
│   ├── create-liquidation-scenario.sh
│   ├── configure-frontend.sh
│   ├── deploy-local.sh
│   ├── frontend.sh
│   ├── metal-option-pricer.sh
│   ├── prepare-frontend-demo.sh
│   ├── run-containers.sh
│   ├── run-final-demo.sh
│   ├── run-kind.sh
│   ├── run-local.sh
│   ├── run-observability-tests.sh
│   ├── run-recovery-tests.sh
│   └── scale-kind.sh
├── tools/
│   └── metal-option-pricer/
│       ├── include/dlp/options/
│       ├── shaders/
│       ├── src/
│       └── CMakeLists.txt
├── tests/
│   └── golden/
│       └── risk_vectors.json
├── .dockerignore
├── .gitignore
├── CMakeLists.txt
├── Dockerfile
├── compose.apps.yaml
├── compose.yaml
├── README.md
└── README.zh-CN.md
```

## 环境要求

- macOS 或 Linux
- Bash 或 Zsh
- `curl`
- `jq`
- Python 3，用于解析可观测性验收结果
- 支持 C++20 的编译器和 CMake 3.20 或更高版本
- Node.js 22 或更高版本及 npm
- Go 1.25 或更高版本
- Boost 1.74 或更高版本、nlohmann/json 3.10 或更高版本、GoogleTest、libpq 和 libpqxx 8
- Docker 和 Docker Compose
- 用于 Kubernetes 部署的 `kubectl` 与 kind
- Foundry 与 Anvil，用于本地部署和 RPC 集成测试
- Xcode Command Line Tools 及 Metal 编译器，用于可选的 macOS 期权定价功能
- 可用的网络连接，用于安装 Foundry、下载 Solc，以及首次配置 CMake 时获取固定版本的 Ethereum Keccak、Prometheus C++ 与 NATS C 依赖

## 安装 Foundry

```bash
curl -L https://getfoundry.sh/install | bash
export PATH="$PATH:$HOME/.foundry/bin" # 出现 forge: command not found 时执行
foundryup

# 让后续打开的 Zsh 终端自动加载 Foundry：
echo 'export PATH="$PATH:$HOME/.foundry/bin"' >> ~/.zshrc
source ~/.zshrc # 重新加载配置，使修改立即生效
```

## 安装 C++ 依赖

在使用 Homebrew 的 macOS 上执行：

```bash
brew install cmake boost nlohmann-json googletest libpq libpqxx
```

## 验证工具链

```bash
# 检查 Forge 是否安装成功：
forge --version
# 检查 C++ 工具链：
cmake --version
c++ --version
# 运行 RPC 集成测试前检查 Anvil：
anvil --version
```

## 验证开发环境

```bash
# 进入 Solidity 项目目录：
cd contracts
# 验证本地 Foundry 环境：
forge test --match-contract SmokeTest
```

当前预期结果：

```text
1 test passed
0 failed
0 skipped
```

首次运行时，Foundry 可能会自动下载 `Smoke.t.sol` 所需的 Solc 0.8.36。下载完成后会继续编译和测试。

## 编译和测试

### Solidity

```bash
# 进入 Solidity 项目目录：
cd contracts
# 编译合约：
forge build
# 运行全部测试：
forge test
```

```bash
# 检查 Solidity 代码格式：
forge fmt --check
# 删除 Foundry 生成的 `out/` 和 `cache/`：
forge clean
```

### C++

在仓库根目录执行：

```bash
# 配置并编译 C++ 目标：
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
# 运行本地 C++ 测试：
ctest --test-dir build --output-on-failure
```

PostgreSQL 集成测试需要启动本地数据库：

```bash
DLP_POSTGRES_PORT=5433 docker compose up -d postgres

DLP_TEST_DATABASE_URL=postgresql://dlp:dlp@127.0.0.1:5433/dlp \
ctest --test-dir build --output-on-failure \
-R PostgresStoreIntegrationTests
```

协议 RPC 集成测试需要 Anvil 和已部署的合约。运行本地服务后，在另一个终端加载自动生成的环境变量：

```bash
source .env.local

ctest --test-dir build --output-on-failure \
-R RpcChainClientIntegrationTests
```

### Go

```bash
cd go/oracle-coordinator
go mod tidy
go test ./...
```

### Frontend

```bash
cd frontend
npm install
npm run build
npm test
```

## 本地运行

完成 C++ 编译后，通过一条命令启动 PostgreSQL、Anvil、部署合约，并运行 Indexer、Liquidator 和 API Server：

```bash
./scripts/run-local.sh
```

本地脚本默认使用 PostgreSQL `5433`、Anvil `8546` 和 API `18080` 端口，合约地址写入已被忽略的 `.env.local`。当前 RPC 和已部署合约可以复用时，脚本会保留对应的索引数据；需要部署新链时，脚本会自动删除旧 PostgreSQL volume，避免旧索引状态和交易 nonce 混入新链。

如需在启动前强制删除本地 PostgreSQL volume：

```bash
./scripts/run-local.sh --clean
```

保持启动脚本运行，并在另一个终端创建完整清算场景：

```bash
./scripts/create-liquidation-scenario.sh

curl -sS http://127.0.0.1:18080/markets
curl -sS http://127.0.0.1:18080/liquidations
curl -sS http://127.0.0.1:18080/protocol/stats
```

Indexer 追上链头后，该场景会产生五次部分清算、耗尽借款人的抵押物，并将剩余债务记录为坏账。按 `Ctrl+C` 会停止 C++ 服务及脚本启动的 Anvil；PostgreSQL 容器会保留到下一次重置。

### 运行 Frontend Demo

保持 `./scripts/run-local.sh` 运行。在第二个终端中为本地演示账户准备资产，根据当前部署地址生成 Vite 配置，然后启动 Dashboard：

```bash
./scripts/prepare-frontend-demo.sh

cd frontend
npm install
npm run dev
```

打开 `http://127.0.0.1:5173`。在钱包中添加 RPC URL 为 `http://127.0.0.1:8546`、Chain ID 为 `31337` 的本地 Anvil 网络。准备脚本会向 Alice（`0x7099…79C8`）发放 20 WETH，向 Charlie（`0x90F7…b906`）发放 100,000 USDC。两个账户使用 Anvil 公开开发密钥，只能用于可随时丢弃的本地链。

将以下公开且仅限 Anvil 本地开发的密钥导入钱包：

```text
Alice:   0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d
Charlie: 0x7c852118294b873bd7ebd81f49d5e1ac554b1f4a4392e31f5eac68e54b70
```

完整演示流程为：

```text
Charlie 连接钱包并供应 50,000 USDC
→ Alice 连接钱包并供应 10 WETH
→ Alice 借出 20,000 USDC
→ Position 显示抵押物、存款、债务与 Health Factor
→ Oracle 价格变化后，页面更新索引后的 Health Factor
→ Liquidator 执行并记录清算
→ Alice 偿还剩余 USDC 债务（输入 10,001 可覆盖累计利息）
→ Alice 最多提取 5 WETH
```

在另一个终端中将 WETH 价格降至 `$2,400`：

```bash
source .env.local
cast send "$DLP_ORACLE_ADDRESS" "setPrice(address,uint256)" \
  "$DLP_WETH_ADDRESS" 240000000000 \
  --rpc-url "$DLP_RPC_URL" \
  --private-key "$DLP_OPERATOR_PRIVATE_KEY"
```

Market、Position、Liquidations、System 和 Risk 页面通过 C++ API 读取索引状态。Supply、Borrow、Repay 和 Withdraw 交易均由钱包签名后直接发送给 Solidity 协议。页面会分别显示链上 Receipt 已确认和 Indexer 已同步两种状态。

## 使用 kind 运行

构建服务镜像、创建全新 kind 集群、部署合约、执行数据库迁移并启动完整系统：

```bash
./scripts/run-kind.sh --clean
```

API 地址为 `http://127.0.0.1:18080`，Prometheus 地址为 `http://127.0.0.1:19090`，自动配置的 Grafana Dashboard 地址为 `http://127.0.0.1:13000/d/dlp-overview`。如果首次拉取镜像较慢导致启动中断，可以保留当前集群继续执行：

```bash
./scripts/run-kind.sh
```

Docker 会在复制项目源码之前建立固定版本的 C++ 依赖层，因此普通源码改动可以直接复用这些依赖。

查看部署状态和当前 Oracle Leader：

```bash
kubectl --context kind-dlp get pods -n dlp
kubectl --context kind-dlp get lease oracle-coordinator -n dlp
```

一键执行可观测性验收，并将结果写入 `artifacts/observability/`：

```bash
./scripts/run-observability-tests.sh --clean
```

脚本会把 kind 启动结果和每项验收结果写入 `summary.log`，等待全部十个 Prometheus Target 完成首次成功抓取后，再保存 Target 与指标快照。生成的 artifacts 已排除在 Docker build context 之外，写入日志不会再使 C++ 镜像缓存失效。

## Sepolia RPC 接入与最终演示

Sepolia 范围只包含只读 RPC 连通。只需在已忽略的 `.env.sepolia` 中填写三个 Endpoint；无需钱包、私钥、测试 ETH、合约地址、部署或测试网交易。

```bash
./scripts/check-sepolia-rpc.sh
```

脚本确认主 RPC、备用 RPC 和浏览器 RPC 均返回 Sepolia Chain ID `11155111`，不会发送交易。

完整协议演示继续运行在已经验收的本地 kind 与 Anvil 环境。日常重新启动使用：

```bash
./scripts/run-final-demo.sh
```

首次完整运行、部署或 Kubernetes 配置变化，以及已有集群状态异常时使用 `--clean`：

```bash
./scripts/run-final-demo.sh --clean
```

该命令会检查 Sepolia RPC 接入，启动本地分布式系统，准备确定性的本地 Demo 账户，构建并启动宿主机原生 Metal 期权服务，然后使用 `.env.kind` 启动 Frontend。Frontend 退出时，Metal 子进程会同步停止。

本地 API 地址为 `http://127.0.0.1:18080`，Metal 健康检查地址为 `http://127.0.0.1:18081/health`，Prometheus 地址为 `http://127.0.0.1:19090`，Grafana 地址为 `http://127.0.0.1:13000/d/dlp-overview`，Frontend 地址为 `http://127.0.0.1:4173`。Metal 服务根路径不提供网页，访问 `GET /` 会按设计返回 `route not found`。

正常使用只需运行 `run-final-demo.sh`。需要单独启动 Frontend 时，必须传入与当前后端对应的环境文件：

```text
run-local.sh       → ./scripts/frontend.sh .env.local
run-containers.sh  → ./scripts/frontend.sh .env.containers
run-kind.sh        → ./scripts/frontend.sh .env.kind
```

混用环境文件会触发 Frontend 的 API 市场与合约地址不匹配提示。

在 Frontend 中完成本地钱包流程，并使用现有本地脚本演示清算、可观测性和故障恢复：

```bash
./scripts/create-liquidation-scenario.sh .env.kind
./scripts/run-observability-tests.sh --clean
./scripts/run-recovery-tests.sh --clean
```

## 已实现功能

### Mock USDC

一个使用 OpenZeppelin Contracts 实现的测试用 ERC-20 代币，用于模拟 USDC。

```text
名称：Mock USDC
符号：mUSDC
精度：6
铸造：本地演示和测试环境可自由铸造
```

文件：

```text
contracts/src/mocks/MockUSDC.sol
contracts/test/MockUSDC.t.sol
```

### Mock WETH

一个作为 WETH 抵押物使用的测试用 ERC-20 代币，不包含 ETH 包装和解包功能。

```text
名称：Mock WETH
符号：mWETH
精度：18
铸造：本地演示和测试环境可自由铸造
```

文件：

```text
contracts/src/mocks/MockWETH.sol
contracts/test/MockWETH.t.sol
```

### 价格预言机

价格使用 8 位小数，支持资产注册、过期价格校验、管理员更新和受角色控制的聚合价格发布。聚合报告必须使用有效时间戳和严格递增的 roundId。

文件：

```text
contracts/src/PriceOracle.sol
contracts/test/PriceOracle.t.sol
```

### 定点数计算

提供统一的 WAD、RAY 和 BPS 数学工具，包括高精度乘除、明确的舍入方向，以及 6 位和 18 位代币的 USD 价值换算。

Golden vectors 保存可复用的预期结果，供 Solidity 和未来的链下实现共同验证。

文件：

```text
contracts/src/libraries/MathLib.sol
contracts/test/MathLib.t.sol
contracts/test/MathLibGolden.t.sol
tests/golden/risk_vectors.json
```

### 风险管理

使用 75% LTV 和 80% 清算阈值计算 WETH 抵押价值与 USDC 债务价值。健康因子采用保守舍入，无债务仓位始终视为健康。

文件：

```text
contracts/src/RiskManager.sol
contracts/test/RiskManager.t.sol
```

### 借贷池

支持 USDC 流动性供应、WETH 抵押、USDC 借款与还款，以及安全提款。借贷池会检查可用流动性、借款额度、健康因子、最低债务和价格有效期，并维护指数化存款、债务和协议储备账目。

集成测试覆盖完整的供应、借款、还款和抵押物提款闭环。

文件：

```text
contracts/src/LendingPool.sol
contracts/test/LendingPool.t.sol
contracts/test/LendingPoolIntegration.t.sol
```

### 利率与指数化凭证

USDC 存款和债务使用不可转让、仅由借贷池控制的缩放凭证表示。借贷池在市场状态变化前，通过借款指数与流动性指数惰性计息。

分段利率模型采用 80% 最优资金利用率、2% 基础利率、8% 第一段斜率、100% 第二段斜率，以及 10% 协议储备因子。存款利息以实际可分配的借款利息为上限，避免整数舍入破坏借贷池的会计恒等式。

Golden Vector、Fuzz 测试、Stateful Invariant 和舍入边界回归测试覆盖利率、指数、协议储备和计息操作。

文件：

```text
contracts/src/InterestRateModel.sol
contracts/src/interfaces/IIndexProvider.sol
contracts/src/tokens/DepositToken.sol
contracts/src/tokens/DebtToken.sol
contracts/test/DepositToken.t.sol
contracts/test/DebtToken.t.sol
contracts/test/InterestRateModel.t.sol
contracts/test/InterestAccounting.t.sol
contracts/test/InterestFuzz.t.sol
contracts/test/InterestGolden.t.sol
contracts/test/LendingMvpInvariant.t.sol
```

### 清算与坏账

健康因子低于 1 的仓位可以进行部分清算，Close Factor 为 50%，清算奖励为 5%。清算流程明确处理还款上限、最小抵押物输出、精度舍入、债务碎片、抵押物耗尽和坏账单次确认。

Fuzz 测试和 Stateful Invariant 用于验证借款与清算边界、代币余额，以及借贷池的 USDC 会计恒等式。

文件：

```text
contracts/src/LiquidationManager.sol
contracts/test/LiquidationManager.t.sol
contracts/test/LendingMvpFuzz.t.sol
contracts/test/LendingMvpInvariant.t.sol
contracts/test/RiskManagerGolden.t.sol
```

### C++ Ethereum 公共层

现代 C++20 公共层提供强类型 Ethereum 地址、带溢出检查的 256 位整数、严格的十六进制转换、Ethereum 兼容 Keccak-256，以及当前协议所需的固定类型 ABI 编码和事件解码。

同步 HTTP JSON-RPC Client 使用 Boost.Asio 和 Boost.Beast，提供结构化错误与强类型返回值。当前支持 `eth_chainId`、`eth_blockNumber`、`eth_getBlockByNumber` 和 `eth_getLogs`，完整调用路径已通过 Anvil 验证。

文件：

```text
CMakeLists.txt
cpp/common/CMakeLists.txt
cpp/common/include/dlp/ethereum/
cpp/common/src/
cpp/common/smoke/
cpp/common/tests/
```

### PostgreSQL 与 Reorg-aware Indexer

C++20 Indexer 通过轮询读取 Ethereum 区块和协议日志，解码已注册的 ABI 事件，并在 PostgreSQL 中重建仓位、市场账目、价格、清算和坏账状态。每个区块及其原始日志、派生状态和同步游标均在同一个数据库事务中提交。

重启恢复会校验已保存的 canonical block hash。Parent Hash 不匹配时，Indexer 会查找共同祖先、标记孤块，并从 canonical logs 确定性重建状态，同时保留孤块历史。

文件：

```text
compose.yaml
database/migrations/
cpp/indexer/
scripts/deploy-local.sh
scripts/run-local.sh
```

### C++ 风险引擎

Risk Engine 从 PostgreSQL 读取已索引的仓位和市场状态，通过带检查的整数计算复现 Solidity 的抵押价值、债务价值、健康因子和清算计算，并批量扫描可执行的清算候选仓位。

文件：

```text
cpp/risk-engine/
tests/golden/risk_vectors.json
```

### 事务消息与租约清算

PostgreSQL Transactional Outbox 记录由 Publisher 发送到 NATS JetStream，并由消费者幂等处理。清算任务使用数据库 Lease 和 Fencing Token，使多个 Liquidator 副本能够安全领取任务。持久化 Tx Manager 负责签名 EIP-1559 交易、恢复 nonce、替换停滞交易，并记录最终确认或 Reorg。

文件：

```text
cpp/tx-manager/
cpp/liquidator/
cpp/messaging/
database/migrations/007_create_tx_jobs.sql
database/migrations/008_create_outbox_events.sql
database/migrations/009_create_liquidation_jobs.sql
scripts/create-liquidation-scenario.sh
```

### REST API

基于 Boost.Beast 的 API Server 提供市场、仓位、健康因子、清算历史、协议统计和确定性风险模拟接口。读取响应包含已索引区块、当前链头和索引延迟。

接口：

```text
GET  /health
GET  /ready
GET  /markets
GET  /positions/:address
GET  /positions/:address/health
GET  /liquidations
GET  /protocol/stats
POST /risk/simulate
```

文件：

```text
cpp/api-server/
```

### 容器与 Kubernetes

多阶段镜像用于打包 C++ 和 Go 服务。Docker Compose 提供容器化本地环境，kind 通过 Kubernetes manifests 运行 PostgreSQL、JetStream、Anvil、两个 RPC Proxy、全部应用服务和多副本 Worker。

文件：

```text
Dockerfile
compose.apps.yaml
k8s/
scripts/run-containers.sh
scripts/run-kind.sh
scripts/scale-kind.sh
```

### 高可用 Oracle 与 RPC 路由

三个模拟价格 Provider 通过中位数聚合。三个 Go Oracle Coordinator 副本使用 Kubernetes Lease 选举唯一发布者，并通过 PostgreSQL Fencing、单调递增 roundId 和幂等交易任务保护发布流程。私钥仍只由 C++ Tx Manager 持有。

RPC 根据请求用途分别路由：Indexer 使用 Active/Failover 并校验 chainId 与 canonical block hash；API 保持一个已验证的活动端点，并在其不可用时切换；Tx Manager 优先从主 RPC 获取 pending nonce，并向两个端点广播完全相同的已签名交易。

文件：

```text
go/oracle-coordinator/
database/migrations/010_create_oracle_publications.sql
infrastructure/rpc-proxy/
cpp/common/include/dlp/ethereum/RpcEndpoints.hpp
```

### Prometheus 与 Grafana 可观测性

所有 C++ 和 Go 服务统一提供 `/health`、`/ready` 和 `/metrics`。Prometheus 自动发现应用 Pod 和 kube-state-metrics；预置的 `DLP Overview` Dashboard 展示链与索引进度、风险扫描、清算结果、交易生命周期、RPC 延迟、区块高度、成功切换与广播、Oracle 新鲜度和 Kubernetes 可用性。

文件：

```text
cpp/observability/
observability/
k8s/observability.yaml
scripts/run-observability-tests.sh
```

### Frontend 协议控制台

React 与 TypeScript Dashboard 通过 wagmi 和 viem 连接浏览器钱包，展示已索引的市场、仓位、清算历史、后端新鲜度和 C++ 确定性风险模拟结果。用户交易先在钱包中批准和签名，再直接发送到 Solidity 协议。Risk 页面还提供默认关闭的 Apple Metal 期权分析开关。

文件：

```text
frontend/
scripts/configure-frontend.sh
scripts/prepare-frontend-demo.sh
```

### Apple Metal 期权分析

该 macOS 宿主机原生工具使用 Metal Monte Carlo Kernel 计算欧式看涨与看跌期权价格，并与 CPU `double` 精度的 Black–Scholes 解析价对比。它独立于协议 Risk Engine、钱包、RPC、Docker 和 Kubernetes。只有用户在 Frontend 的 Risk 页面开启开关后，Frontend 才会调用该服务。

文件：

```text
tools/metal-option-pricer/
scripts/metal-option-pricer.sh
```

### Sepolia RPC 接入与本地最终演示

Sepolia 边界只验证主 RPC、备用 RPC 和浏览器 RPC 的连通性，不加载私钥，也不发送交易。最终分布式演示复用已经验收的本地 kind 与 Anvil 系统。

文件：

```text
.env.sepolia.example
scripts/check-sepolia-rpc.sh
scripts/run-final-demo.sh
scripts/run-kind.sh
scripts/prepare-frontend-demo.sh
```
