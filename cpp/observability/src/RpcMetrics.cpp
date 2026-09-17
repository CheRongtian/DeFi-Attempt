#include "dlp/observability/RpcMetrics.hpp"

#include <chrono>

namespace dlp::observability
{

RpcMetrics::RpcMetrics(ServiceMetrics& metrics)
    : requests_(metrics.AddCounter("rpc_requests_total", "Total Ethereum RPC requests")),
      errors_(metrics.AddCounter("rpc_errors_total", "Total failed Ethereum RPC requests")),
      duration_(metrics.AddHistogram(
          "rpc_request_duration_seconds",
          "Ethereum RPC request duration in seconds",
          {0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0}
      )),
      blockHeight_(metrics.AddGauge(
          "rpc_block_height",
          "Latest Ethereum block height returned by RPC"
      )),
      failovers_(metrics.AddCounter(
          "rpc_failovers_total",
          "Total successful RPC endpoint failovers"
      )),
      broadcastSuccesses_(metrics.AddCounter(
          "rpc_broadcast_success_total",
          "Total transactions successfully broadcast to at least one RPC endpoint"
      ))
{
    ethereum::SetRpcObserver(this);
}

RpcMetrics::~RpcMetrics()
{
    ethereum::SetRpcObserver(nullptr);
}

void RpcMetrics::Observe(
    std::string_view,
    std::chrono::steady_clock::duration duration,
    bool succeeded
) noexcept
{
    requests_.Increment();
    if(!succeeded)
    {
        errors_.Increment();
    }
    duration_.Observe(std::chrono::duration<double>{duration}.count());
}

void RpcMetrics::ObserveBlockHeight(std::uint64_t blockHeight) noexcept
{
    blockHeight_.Set(static_cast<double>(blockHeight));
}

void RpcMetrics::ObserveFailover() noexcept
{
    failovers_.Increment();
}

void RpcMetrics::ObserveBroadcastSuccess() noexcept
{
    broadcastSuccesses_.Increment();
}

}
