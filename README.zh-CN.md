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
│   │   └── mocks/
│   │       ├── MockUSDC.sol
│   │       └── MockWETH.sol
│   └── test/
│       ├── MockUSDC.t.sol
│       ├── MockWETH.t.sol
│       └── Smoke.t.sol
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
