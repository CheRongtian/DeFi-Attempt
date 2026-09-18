# 分布式 DeFi 借贷协议

[English](README.md)

一个使用 Solidity、C++20、Go、PostgreSQL、NATS JetStream、Kubernetes、Prometheus、Grafana 和 React 构建的分布式超额抵押借贷协议。

项目将链上权威借贷协议与容错链下基础设施结合，用于索引、确定性风险分析、自动清算、交易管理、Oracle 协调、故障恢复和可观测性。

> 本项目是通过 Anvil 和 kind 进行本地演示的作品集原型。Sepolia 仅支持只读 RPC 连通性检查。协议未经审计，不得用于真实资金。

## 系统架构

```text
Frontend ── 查询 ──> C++ API ── 读取 ──> PostgreSQL
    │                     │                    ▲
    │                     └── 链头状态 ──> Ethereum RPC
    │                                          ▲
    └── 用户钱包 ── 用户交易 ──> Solidity Protocol
                                           │
                                           └── 日志/区块 ──> Ethereum RPC
                                                                 │
                                                                 ▼
                                                              Indexer
                                                                 │
                     PostgreSQL <────────────────────────────────┘
                          │
                          └──> Risk Engine ──> Outbox ──> NATS JetStream
                                                                  │
                                                                  ▼
Liquidator ── 租约任务 ──> tx_jobs ──> Tx Manager ──> Ethereum RPC

Go Oracle Coordinator ── fenced publication ──> tx_jobs ──> Tx Manager
```

用户交易始终遵循：

```text
Frontend → User Wallet → Solidity
```

Solidity 是金融状态的唯一权威来源。PostgreSQL 保存可重建的索引状态和运行状态；链下服务无法授权非法借款、提款或清算。

## 核心亮点

- 使用兼容 WETH/USDC 精度的 MockWETH / MockUSDC 实现超额抵押借贷
- Supply、Borrow、Repay、Withdraw、指数化利息、清算和坏账核算
- 具有明确舍入方向的 WAD、RAY 和 BPS 定点数计算
- Solidity 与 C++ 确定性风险计算共用 Golden Vector 验证
- 支持 Reorg 和确定性状态重建的 C++20 区块链索引器
- 支持 Nonce 恢复和替换交易的持久化 EIP-1559 Transaction Manager
- Transactional Outbox 与 NATS JetStream 至少一次投递
- 幂等消费者、数据库租约和 Fencing Token
- 可水平扩展的 Liquidator Worker
- 对可配置 Demo Provider 进行中位数聚合的 Go Oracle Coordinator
- Kubernetes Leader Election、RPC Failover 和签名交易广播
- Prometheus 指标、Grafana Dashboard 和自动恢复测试
- 基于用户钱包的 React 协议控制台
- 可选的 Apple Metal 原生欧式期权分析

## 协议

本地 MVP 市场使用 `MockWETH` 和 `MockUSDC`：

| 资产 | 存入 | 抵押品 | 借出 | 精度 |
|---|---:|---:|---:|---:|
| MockWETH | 是 | 是 | 否 | 18 |
| MockUSDC | 是 | 否 | 是 | 6 |

核心参数固化在合约中：

| 参数 | 数值 |
|---|---:|
| WETH LTV | 75% |
| WETH 清算阈值 | 80% |
| 清算 Close Factor | 50% |
| 清算奖励 | 5% |
| 协议 Reserve Factor | 10% |
| 最小 USDC 债务 | 1 USDC |

USDC 存款和债务由不可转让的缩放凭证表示。Borrow Index 和 Liquidity Index 在市场状态变更前惰性累计。如果清算耗尽全部抵押品，剩余债务会被明确记录为坏账。

## 分布式系统

链下系统在处理故障的同时，不建立第二套金融状态权威来源：

```text
Ethereum Reorg 检测与共同祖先恢复
Canonical Log 重放与状态重建
Transactional Outbox 与 JetStream 投递
事件幂等消费
清算任务租约与 Fencing Token
Oracle Leader Election 与数据库 Fencing
持久化 Nonce 分配与替换交易
RPC 读取故障切换与广播
进程崩溃恢复与 Kubernetes 调谐
```

Ethereum 提供共识。C++ 和 Go 在 Solidity 状态机周围提供容错运行基础设施。

## 演示

完整本地环境运行于 kind 和 Anvil。全新状态下的自动清算流程为：

```text
提供 USDC 流动性
→ Alice 存入 MockWETH
→ Alice 借出 MockUSDC
→ WETH 价格下降
→ Risk Engine 产生清算候选事件
→ Liquidator 获取带 Fencing 的租约任务
→ Tx Manager 签名并广播获准调用
→ Solidity 验证并执行清算
→ Indexer 重建派生状态
→ API、Frontend、Prometheus 和 Grafana 显示结果
```

Frontend 还可以交互式演示钱包签名的 Supply、Borrow、Repay 和 Withdraw。恢复工具覆盖 Pod 崩溃、PostgreSQL 与 JetStream 重启、RPC 故障、Leader 与租约接管、交易 Reorg 和深度链重组。

## 快速开始

完整启动脚本会运行可选的 Apple Metal 服务，因此当前面向 macOS。先按照[开发环境](docs/DEVELOPMENT.md)安装依赖，然后创建只读 Sepolia RPC 配置：

```bash
cp .env.sepolia.example .env.sepolia
chmod 600 .env.sepolia
```

填写 `.env.sepolia` 中的三个 RPC Endpoint 后，启动全新的本地演示：

```bash
./scripts/run-final-demo.sh --clean
```

后续运行可以复用已有集群：

```bash
./scripts/run-final-demo.sh
```

启动脚本会验证 Sepolia RPC 连通性、启动本地 kind/Anvil 系统、准备 Demo 账户、启动 Metal 服务并运行 Frontend。它不会向 Sepolia 部署合约或发送 Sepolia 交易。

| 服务 | 本地地址 |
|---|---|
| Frontend | `http://127.0.0.1:4173` |
| API | `http://127.0.0.1:18080` |
| Metal 健康检查 | `http://127.0.0.1:18081/health` |
| Prometheus | `http://127.0.0.1:19090` |
| Grafana | `http://127.0.0.1:13000/d/dlp-overview` |

原生进程、Docker Compose、kind、独立 Frontend 和清理操作参见[运行说明](docs/OPERATIONS.md)。

## 验证体系

仓库包含：

- Foundry 单元测试、集成测试、Fuzz Test 和 Stateful Invariant Test
- Solidity/C++ 共用的风险、清算和利息 Golden Vector 测试
- C++20 单元测试、PostgreSQL 集成测试、RPC 集成测试和 Reorg 测试
- Go 聚合、发布、幂等性与 Fencing 测试
- React 组件、配置、钱包状态和 API 状态测试
- 自动化 Kubernetes 故障恢复和可观测性验收测试

测试命令及所需服务参见[测试说明](docs/TESTING.md)。

## 文档

- [系统架构](docs/ARCHITECTURE.md)
- [协议与金融规则](docs/PROTOCOL.md)
- [分布式系统设计](docs/DISTRIBUTED_SYSTEM.md)
- [开发环境](docs/DEVELOPMENT.md)
- [运行说明](docs/OPERATIONS.md)
- [测试说明](docs/TESTING.md)
- [安全边界](docs/SECURITY.md)
- [演示指南](docs/DEMO.md)

模块文档：

- [Solidity 合约](contracts/README.md)
- [C++20 服务](cpp/README.md)
- [Go Oracle Coordinator](go/oracle-coordinator/README.md)
- [Frontend](frontend/README.md)
- [Kubernetes](k8s/README.md)
- [可观测性](observability/README.md)
- [Apple Metal 期权定价工具](tools/metal-option-pricer/README.md)

## 仓库结构

```text
contracts/       Solidity 协议和 Foundry 测试
cpp/             C++20 公共库和链下服务
go/              Go Oracle Coordinator
frontend/        React 协议控制台
database/        PostgreSQL Migration
infrastructure/  本地 RPC Proxy 配置
k8s/             kind 和 Kubernetes Manifest
observability/   Prometheus 和 Grafana 配置
scripts/         构建、启动、演示和验收自动化脚本
tests/           跨语言共用 Golden Vector
tools/           可选的宿主机原生分析工具
docs/            项目详细文档
```

## 安全边界

- Solidity 执行最终的风险、提款、利息、清算和坏账检查。
- Frontend 和 API 不持有用户私钥，也不替用户签名交易。
- Tx Manager 只签署零原生价值且经过批准的 Oracle 和 Liquidation Manager 调用。
- Operational Signer 与不需要签名的服务隔离。
- PostgreSQL 是可重建的索引状态，不是金融权威状态。
- LendingPool 要求精确转账语义，不支持 Fee-on-transfer 和 Rebasing Token。
- 生成的环境文件和 Secret 不进入版本控制。
- 仓库中的 Anvil 账户和本地数据库凭据仅用于可丢弃的开发环境。

完整信任模型与生产限制参见[安全说明](docs/SECURITY.md)。

## 项目范围

本项目展示协议核算、区块链索引、确定性风险分析、事件驱动处理、交易管理、Reorg 恢复、租约、Fencing、Leader Election、Kubernetes 运行和可观测性。

本项目不声明 Mainnet Ready、外部审计、形式化验证、生产级 Oracle 保证、机构级密钥托管或真实资金安全性。
