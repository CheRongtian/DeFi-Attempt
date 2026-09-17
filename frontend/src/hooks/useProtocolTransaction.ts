import { useCallback, useState } from 'react'
import { BaseError, parseUnits, type Address, type Hash } from 'viem'
import { usePublicClient, useWriteContract } from 'wagmi'

import type { AppConfig } from '../config'
import { erc20Abi, lendingPoolAbi } from '../contracts'

export type AssetSymbol = 'WETH' | 'USDC'
export type TransactionAction = 'supply' | 'borrow' | 'repay' | 'withdraw'
export type TransactionPhase =
  | 'idle'
  | 'approval-wallet'
  | 'approval-pending'
  | 'transaction-wallet'
  | 'transaction-pending'
  | 'confirmed'
  | 'error'

export interface TransactionRequest {
  action: TransactionAction
  asset: AssetSymbol
  amount: string
}

export interface TransactionProgress {
  phase: TransactionPhase
  hash?: Hash
  receiptBlock?: bigint
  message?: string
}

interface UseProtocolTransactionOptions {
  config: AppConfig
  address?: Address
  correctNetwork: boolean
  onReceipt: () => Promise<void>
}

const idleProgress: TransactionProgress = { phase: 'idle' }

function transactionError(error: unknown) {
  if (error instanceof BaseError) return error.shortMessage
  if (error instanceof Error) return error.message
  return 'Transaction failed'
}

export function useProtocolTransaction({
  config,
  address,
  correctNetwork,
  onReceipt,
}: UseProtocolTransactionOptions) {
  const publicClient = usePublicClient({ chainId: config.chainId })
  const writeContract = useWriteContract()
  const [progress, setProgress] = useState<TransactionProgress>(idleProgress)

  const execute = useCallback(
    async ({ action, asset, amount }: TransactionRequest) => {
      if (!address) {
        setProgress({ phase: 'error', message: 'Connect a wallet before sending a transaction.' })
        return
      }
      if (!correctNetwork) {
        setProgress({ phase: 'error', message: `Switch to ${config.chainName} before continuing.` })
        return
      }
      if (!publicClient) {
        setProgress({ phase: 'error', message: 'The configured RPC client is unavailable.' })
        return
      }

      const decimals = asset === 'WETH' ? 18 : 6
      const assetAddress = asset === 'WETH' ? config.contracts.weth : config.contracts.usdc
      let parsedAmount: bigint
      try {
        parsedAmount = parseUnits(amount, decimals)
      } catch {
        setProgress({ phase: 'error', message: 'Enter a valid token amount.' })
        return
      }
      if (parsedAmount <= 0n) {
        setProgress({ phase: 'error', message: 'Amount must be greater than zero.' })
        return
      }

      try {
        if (action === 'supply' || action === 'repay') {
          setProgress({ phase: 'approval-wallet', message: `Confirm the ${asset} approval in your wallet.` })
          const approvalHash = await writeContract.mutateAsync({
            address: assetAddress,
            abi: erc20Abi,
            functionName: 'approve',
            args: [config.contracts.lendingPool, parsedAmount],
            account: address,
            chainId: config.chainId,
          })
          setProgress({ phase: 'approval-pending', hash: approvalHash })
          const approvalReceipt = await publicClient.waitForTransactionReceipt({ hash: approvalHash })
          if (approvalReceipt.status !== 'success') {
            throw new Error('The token approval reverted.')
          }
        }

        setProgress({ phase: 'transaction-wallet', message: `Confirm ${action} in your wallet.` })
        const hash = await writeContract.mutateAsync({
          address: config.contracts.lendingPool,
          abi: lendingPoolAbi,
          functionName: action,
          args: [assetAddress, parsedAmount],
          account: address,
          chainId: config.chainId,
        })
        setProgress({ phase: 'transaction-pending', hash })
        const receipt = await publicClient.waitForTransactionReceipt({ hash })
        if (receipt.status !== 'success') {
          throw new Error('The transaction receipt reported a reverted transaction.')
        }
        setProgress({ phase: 'confirmed', hash, receiptBlock: receipt.blockNumber })
        try {
          await onReceipt()
        } catch {
          setProgress({
            phase: 'confirmed',
            hash,
            receiptBlock: receipt.blockNumber,
            message: 'Transaction confirmed on-chain. Indexed state is temporarily unavailable.',
          })
        }
      } catch (error) {
        setProgress({ phase: 'error', message: transactionError(error) })
      }
    }, [address, config, correctNetwork, onReceipt, publicClient, writeContract],
  )

  const reset = useCallback(() => setProgress(idleProgress), [])
  const busy = !['idle', 'confirmed', 'error'].includes(progress.phase)
  return { progress, busy, execute, reset }
}
