# 分布式 DeFi 借贷协议

[English](README.md)

一个使用 Solidity 开发的超额抵押借贷协议，包含 Ethereum RPC、Reorg-aware Indexer、风险扫描、交易管理、自动清算，以及由 PostgreSQL 提供数据的 REST API 等 C++ 服务。智能合约使用 Foundry，C++ 组件使用 CMake 和 CTest 编译与验证。

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
│   │   │   ├── RpcClient.hpp
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
│   │   │   ├── RpcClient.cpp
│   │   │   ├── Uint256.cpp
│   │   │   └── Uint256Math.cpp
│   │   ├── tests/
│   │   │   ├── AbiTests.cpp
│   │   │   ├── AddressTests.cpp
│   │   │   ├── HexTests.cpp
│   │   │   ├── KeccakTests.cpp
│   │   │   ├── ProtocolAbiTests.cpp
│   │   │   ├── RpcClientIntegrationTests.cpp
│   │   │   ├── Uint256Tests.cpp
│   │   │   └── Uint256MathTests.cpp
│   │   └── CMakeLists.txt
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
│       └── 007_create_tx_jobs.sql
├── scripts/
│   ├── create-liquidation-scenario.sh
│   ├── deploy-local.sh
│   └── run-local.sh
├── tests/
│   └── golden/
│       └── risk_vectors.json
├── .gitignore
├── CMakeLists.txt
├── compose.yaml
├── README.md
└── README.zh-CN.md
```

## 环境要求

- macOS 或 Linux
- Bash 或 Zsh
- `curl`
- 支持 C++20 的编译器和 CMake 3.20 或更高版本
- Boost 1.74 或更高版本、nlohmann/json 3.10 或更高版本、GoogleTest、libpq 和 libpqxx 8
- Docker 和 Docker Compose
- Foundry 与 Anvil，用于本地部署和 RPC 集成测试
- 可用的网络连接，用于安装 Foundry、下载 Solc，以及首次配置 CMake 时获取固定版本的 Ethereum Keccak 依赖

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

## 本地运行

完成 C++ 编译后，通过一条命令启动 PostgreSQL、Anvil、部署合约，并运行 Indexer、Liquidator 和 API Server：

```bash
./scripts/run-local.sh
```

本地脚本默认使用 PostgreSQL `5433`、Anvil `8546` 和 API `8081` 端口，合约地址写入已被忽略的 `.env.local`。当前 RPC 和已部署合约可以复用时，脚本会保留对应的索引数据；需要部署新链时，脚本会自动删除旧 PostgreSQL volume，避免旧索引状态和交易 nonce 混入新链。

如需在启动前强制删除本地 PostgreSQL volume：

```bash
./scripts/run-local.sh --clean
```

保持启动脚本运行，并在另一个终端创建完整清算场景：

```bash
./scripts/create-liquidation-scenario.sh

curl -sS http://127.0.0.1:8081/markets
curl -sS http://127.0.0.1:8081/liquidations
curl -sS http://127.0.0.1:8081/protocol/stats
```

Indexer 追上链头后，该场景会产生五次部分清算、耗尽借款人的抵押物，并将剩余债务记录为坏账。按 `Ctrl+C` 会停止 C++ 服务及脚本启动的 Anvil；PostgreSQL 容器会保留到下一次重置。

## 已实现功能

### Mock USDC

一个使用 OpenZeppelin Contracts 实现的测试用 ERC-20 代币，用于模拟 USDC。

```text
名称：Mock USDC
符号：mUSDC
精度：6
铸造：本地和测试网环境可自由铸造
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
铸造：本地和测试网环境可自由铸造
```

文件：

```text
contracts/src/mocks/MockWETH.sol
contracts/test/MockWETH.t.sol
```

### 价格预言机

由管理员维护的价格预言机，价格使用 8 位小数，并支持资产注册、更新时间记录和过期价格校验。

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

现代 C++17 公共层提供强类型 Ethereum 地址、带溢出检查的 256 位整数、严格的十六进制转换、Ethereum 兼容 Keccak-256，以及当前协议所需的固定类型 ABI 编码和事件解码。

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

### 交易管理与自动清算

持久化 Tx Manager 负责签名 EIP-1559 交易、结合 RPC 与 PostgreSQL 状态分配 nonce、跟踪提交和 receipt、替换停滞交易，并记录最终确认或 Reorg。单实例 Liquidator 会使用最新链上状态重新验证候选仓位、检查收益，并通过 Tx Manager 提交受限清算交易。

文件：

```text
cpp/tx-manager/
cpp/liquidator/
database/migrations/007_create_tx_jobs.sql
scripts/create-liquidation-scenario.sh
```

### REST API

基于 Boost.Beast 的 API Server 提供市场、仓位、健康因子、清算历史、协议统计和确定性风险模拟接口。读取响应包含已索引区块、当前链头和索引延迟。

接口：

```text
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
