import { parseAbi } from 'viem'

export const erc20Abi = parseAbi([
  'function approve(address spender, uint256 amount) returns (bool)',
])

export const lendingPoolAbi = parseAbi([
  'function supply(address asset, uint256 amount)',
  'function borrow(address asset, uint256 amount)',
  'function repay(address asset, uint256 amount)',
  'function withdraw(address asset, uint256 amount)',
])
