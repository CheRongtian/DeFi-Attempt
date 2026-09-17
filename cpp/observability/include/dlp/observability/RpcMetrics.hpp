#ifndef DLP_OBSERVABILITY_RPC_METRICS_HPP
#define DLP_OBSERVABILITY_RPC_METRICS_HPP

#include <cstdint>

#include "dlp/ethereum/RpcClient.hpp"
#include "dlp/observability/Metrics.hpp"

namespace dlp::observability
{

class RpcMetrics final : public ethereum::RpcObserver
{
public:
    explicit RpcMetrics(ServiceMetrics& metrics);
    ~RpcMetrics() override;

    RpcMetrics(const RpcMetrics&) = delete;
    RpcMetrics& operator=(const RpcMetrics&) = delete;

    void Observe(
        std::string_view method,
        std::chrono::steady_clock::duration duration,
        bool succeeded
    ) noexcept override;
    void ObserveBlockHeight(std::uint64_t blockHeight) noexcept override;
    void ObserveFailover() noexcept override;
    void ObserveBroadcastSuccess() noexcept override;

private:
    prometheus::Counter& requests_;
    prometheus::Counter& errors_;
    prometheus::Histogram& duration_;
    prometheus::Gauge& blockHeight_;
    prometheus::Counter& failovers_;
    prometheus::Counter& broadcastSuccesses_;
};

}

#endif
