# 分布式 DeFi 借贷协议

[English](README.md)

一个使用 Solidity 开发的超额抵押借贷协议。智能合约使用 Foundry 编译和测试。

## 项目结构

```text
DeFi/
├── contracts/
│   ├── foundry.toml
│   ├── lib/
│   │   └── openzeppelin-contracts/
│   ├── src/
│   │   ├── LendingPool.sol
│   │   ├── LiquidationManager.sol
│   │   ├── PriceOracle.sol
│   │   ├── RiskManager.sol
│   │   ├── libraries/
│   │   │   └── MathLib.sol
│   │   └── mocks/
│   │       ├── MockUSDC.sol
│   │       └── MockWETH.sol
│   └── test/
│       ├── LendingPool.t.sol
│       ├── LendingPoolIntegration.t.sol
│       ├── LendingMvpFuzz.t.sol
│       ├── LendingMvpInvariant.t.sol
│       ├── LiquidationManager.t.sol
│       ├── MathLibGolden.t.sol
│       ├── MathLib.t.sol
│       ├── MockUSDC.t.sol
│       ├── MockWETH.t.sol
│       ├── PriceOracle.t.sol
│       ├── RiskManagerGolden.t.sol
│       ├── RiskManager.t.sol
│       └── Smoke.t.sol
├── tests/
│   └── golden/
│       └── risk_vectors.json
├── .gitignore
├── README.md
└── README.zh-CN.md
```

## 环境要求

- macOS 或 Linux
- Bash 或 Zsh
- `curl`
- 可用的网络连接，用于安装 Foundry，以及首次运行时下载所需的 Solidity 编译器

## 安装 Foundry

```bash
curl -L https://getfoundry.sh/install | bash
export PATH="$PATH:$HOME/.foundry/bin" # 出现 forge: command not found 时执行
foundryup

# 让后续打开的 Zsh 终端自动加载 Foundry：
echo 'export PATH="$PATH:$HOME/.foundry/bin"' >> ~/.zshrc
source ~/.zshrc # 重新加载配置，使修改立即生效
```

## 验证工具链

```bash
# 检查 Forge 是否安装成功：
forge --version
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

提供统一的 WAD 和 BPS 数学工具，包括高精度乘除、明确的舍入方向，以及 6 位和 18 位代币的 USD 价值换算。

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

支持 USDC 流动性供应、WETH 抵押、USDC 借款与还款，以及安全提款。借贷池会检查可用流动性、借款额度、健康因子、最低债务和价格有效期。

集成测试覆盖完整的供应、借款、还款和抵押物提款闭环。

文件：

```text
contracts/src/LendingPool.sol
contracts/test/LendingPool.t.sol
contracts/test/LendingPoolIntegration.t.sol
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
