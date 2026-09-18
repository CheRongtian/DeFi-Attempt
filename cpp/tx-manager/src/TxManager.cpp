#include "dlp/tx/TxManager.hpp"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "dlp/ethereum/Uint256Math.hpp"

namespace dlp::tx
{

namespace
{

[[nodiscard]] ethereum::Uint256 BumpFee(const ethereum::Uint256& value)
{
    return ethereum::Uint256Math::MulDivUp(
        value,
        ethereum::Uint256{1'125},
        ethereum::Uint256{1'000}
    );
}

void Audit(const TxJob& job, std::string_view action)
{
    nlohmann::json record{
        {"event", "transaction_audit"},
        {"action", std::string{action}},
        {"jobId", job.jobId},
        {"chainId", job.chainId.ToDecimal()},
        {"wallet", job.wallet.ToHex()},
        {"destination", job.to.ToHex()},
        {"value", job.value.ToDecimal()},
        {"status", std::string{ToString(job.status)}},
        {"retryCount", job.retryCount}
    };
    if(job.transactionHash.has_value())
    {
        record["transactionHash"] = ethereum::Hex::Encode(*job.transactionHash);
    }
    if(!job.errorMessage.empty())
    {
        record["error"] = job.errorMessage;
    }
    std::clog << record.dump() << '\n';
}

}

TxManager::TxManager(
    TransactionStore& store,
    TransactionRpc& rpc,
    ethereum::Secp256k1Signer& signer,
    TxManagerConfig config
)
    : store_(store),
      rpc_(rpc),
      signer_(signer),
      config_(config),
      chainId_(rpc_.GetChainId())
{
    if(config_.confirmationDepth == 0U)
    {
        throw std::invalid_argument("transaction confirmation depth must be positive");
    }
}

bool TxManager::Queue(
    std::string jobId,
    const ethereum::Address& to,
    ethereum::Bytes data,
    ethereum::Uint256 value
)
{
    TxJob job;
    job.jobId = std::move(jobId);
    job.chainId = chainId_;
    job.wallet = signer_.GetAddress();
    job.to = to;
    job.value = std::move(value);
    job.data = std::move(data);
    return store_.Insert(job);
}

ethereum::Uint256 TxManager::AllocateNonce() const
{
    const auto rpcNonce = rpc_.GetPendingNonce(signer_.GetAddress());
    const auto storedNonce = store_.LoadHighestNonce(chainId_, signer_.GetAddress());
    if(!storedNonce.has_value())
    {
        return rpcNonce;
    }
    const auto nextStored = *storedNonce + ethereum::Uint256{1};
    return nextStored > rpcNonce ? nextStored : rpcNonce;
}

bool TxManager::IsApproved(const TxJob& job) const
{
    if(job.value != ethereum::Uint256{})
    {
        return false;
    }
    return std::any_of(
        config_.approvedCalls.begin(),
        config_.approvedCalls.end(),
        [&job](const ApprovedCall& approved) {
            return job.to == approved.destination
                && job.data.size() == approved.calldataSize
                && job.data.size() >= approved.selector.size()
                && std::equal(approved.selector.begin(), approved.selector.end(), job.data.begin());
        }
    );
}

void TxManager::Submit(TxJob& job, bool retry)
{
    if(!IsApproved(job))
    {
        job.status = TxStatus::Failed;
        job.errorMessage = "transaction rejected by approval policy";
        store_.Save(job);
        Audit(job, "rejected");
        return;
    }

    bool broadcastAttempted = false;
    try
    {
        if(retry)
        {
            if(job.retryCount >= config_.maximumRetries)
            {
                job.status = TxStatus::Failed;
                job.errorMessage = "transaction retry limit reached";
                store_.Save(job);
                Audit(job, "failed");
                return;
            }
            ++job.retryCount;
        }

        if(!job.nonce.has_value())
        {
            job.nonce = AllocateNonce();
        }

        const ethereum::TransactionCall call{
            signer_.GetAddress(),
            job.to,
            job.data,
            job.value
        };
        if(!job.gasLimit.has_value())
        {
            job.gasLimit = rpc_.EstimateGas(call);
        }

        const bool replacement = retry
            && job.maxPriorityFeePerGas.has_value()
            && job.maxFeePerGas.has_value();
        if(replacement)
        {
            job.status = TxStatus::Replaced;
            store_.Save(job);
            Audit(job, "replaced");
            job.maxPriorityFeePerGas = BumpFee(*job.maxPriorityFeePerGas);
            job.maxFeePerGas = BumpFee(*job.maxFeePerGas);
        }
        else
        {
            const auto fees = rpc_.GetFeeQuote();
            job.maxPriorityFeePerGas = fees.maxPriorityFeePerGas;
            job.maxFeePerGas = fees.maxFeePerGas;
        }

        const ethereum::Eip1559Transaction transaction{
            chainId_,
            *job.nonce,
            *job.maxPriorityFeePerGas,
            *job.maxFeePerGas,
            *job.gasLimit,
            job.to,
            job.value,
            job.data
        };
        job.rawTransaction = signer_.SignTransaction(transaction);
        job.status = TxStatus::Submitted;
        job.submittedBlockNumber = rpc_.GetBlockNumber();
        job.transactionHash = ethereum::Eip1559Encoder::TransactionHash(job.rawTransaction);
        job.includedBlockNumber.reset();
        job.includedBlockHash.reset();
        job.confirmationCount = 0;
        job.errorMessage.clear();
        store_.Save(job);
        Audit(job, "approved");
        broadcastAttempted = true;
        const auto submittedHash = rpc_.SendRawTransaction(job.rawTransaction);
        job.transactionHash = submittedHash;
        store_.Save(job);
        Audit(job, "broadcast");
    }
    catch(const ethereum::RpcException& exception)
    {
        job.errorMessage = exception.what();
        if(broadcastAttempted
            && (exception.GetKind() == ethereum::RpcErrorKind::Transport
                || exception.GetKind() == ethereum::RpcErrorKind::Http))
        {
            job.status = TxStatus::Submitted;
            store_.Save(job);
            Audit(job, "broadcast_uncertain");
            return;
        }

        job.status = job.retryCount >= config_.maximumRetries ? TxStatus::Failed : TxStatus::Pending;
        store_.Save(job);
        Audit(job, job.status == TxStatus::Failed ? "failed" : "retry_pending");
    }
}

void TxManager::Poll(TxJob& job)
{
    if(job.status == TxStatus::Submitted)
    {
        const auto receipt = rpc_.GetReceipt(*job.transactionHash);
        if(receipt.has_value())
        {
            job.includedBlockNumber = receipt->blockNumber.ToUint64();
            job.includedBlockHash = receipt->blockHash;
            job.status = receipt->succeeded ? TxStatus::Included : TxStatus::Failed;
            if(!receipt->succeeded)
            {
                job.errorMessage = "transaction execution reverted";
            }
            store_.Save(job);
            Audit(job, receipt->succeeded ? "included" : "execution_failed");
            return;
        }

        const auto head = rpc_.GetBlockNumber();
        if(job.submittedBlockNumber.has_value()
            && head >= *job.submittedBlockNumber + config_.replacementAfterBlocks)
        {
            if(job.retryCount >= config_.maximumRetries)
            {
                job.status = TxStatus::Failed;
                job.errorMessage = "transaction was not included before the replacement limit";
                store_.Save(job);
                Audit(job, "failed");
            }
            else
            {
                Submit(job, true);
            }
        }
        return;
    }

    if(job.status != TxStatus::Included)
    {
        return;
    }

    const auto canonicalBlock = rpc_.GetBlock(*job.includedBlockNumber);
    if(!canonicalBlock.has_value() || canonicalBlock->hash != *job.includedBlockHash)
    {
        job.status = TxStatus::Reorged;
        job.errorMessage = "included block is no longer canonical";
        store_.Save(job);
        Audit(job, "reorged");
        return;
    }

    const auto head = rpc_.GetBlockNumber();
    job.confirmationCount = head >= *job.includedBlockNumber
        ? head - *job.includedBlockNumber + 1U
        : 0U;
    if(job.confirmationCount >= config_.confirmationDepth)
    {
        job.status = TxStatus::Finalized;
    }
    store_.Save(job);
    if(job.status == TxStatus::Finalized)
    {
        Audit(job, "finalized");
    }
}

TxManagerRunResult TxManager::RunOnce()
{
    auto jobs = store_.LoadActive(chainId_, signer_.GetAddress());
    TxManagerRunResult result;
    for(auto& job : jobs)
    {
        const auto previousStatus = job.status;
        if(job.status == TxStatus::Pending)
        {
            Submit(job, !job.errorMessage.empty());
        }
        else
        {
            Poll(job);
        }

        if(job.status == TxStatus::Pending || job.status == TxStatus::Submitted)
        {
            ++result.active;
        }
        if(previousStatus != job.status)
        {
            if(job.status == TxStatus::Included)
            {
                ++result.included;
            }
            else if(job.status == TxStatus::Finalized)
            {
                ++result.finalized;
                if(job.submittedBlockNumber.has_value() && job.includedBlockNumber.has_value())
                {
                    result.confirmationBlocks.push_back(
                        *job.includedBlockNumber - *job.submittedBlockNumber + 1U
                    );
                }
            }
            else if(job.status == TxStatus::Reorged)
            {
                ++result.reorged;
            }
            else if(job.status == TxStatus::Failed)
            {
                ++result.failed;
            }
        }
    }
    return result;
}

}
