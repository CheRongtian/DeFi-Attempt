#ifndef DLP_LIQUIDATOR_LIQUIDATION_JOBS_HPP
#define DLP_LIQUIDATOR_LIQUIDATION_JOBS_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "dlp/liquidator/Liquidator.hpp"
#include "dlp/messaging/Event.hpp"

namespace dlp::liquidator
{

struct LiquidationJob
{
    std::string jobId;
    risk::LiquidationCandidate candidate;
    std::string workerId;
    std::uint64_t fencingToken{0};
};

struct LiquidationReconcileResult
{
    std::size_t completed{0};
    std::size_t failed{0};
    std::size_t reorged{0};
};

class PostgresLiquidationJobStore final
{
public:
    explicit PostgresLiquidationJobStore(std::string connectionString);
    ~PostgresLiquidationJobStore();

    PostgresLiquidationJobStore(PostgresLiquidationJobStore&& other) noexcept;
    PostgresLiquidationJobStore& operator=(PostgresLiquidationJobStore&& other) noexcept;

    PostgresLiquidationJobStore(const PostgresLiquidationJobStore&) = delete;
    PostgresLiquidationJobStore& operator=(const PostgresLiquidationJobStore&) = delete;

    [[nodiscard]] bool Enqueue(const messaging::EventEnvelope& event);
    [[nodiscard]] std::optional<LiquidationJob> Claim(
        const ethereum::Uint256& chainId,
        std::string_view workerId,
        std::chrono::seconds leaseDuration
    );
    [[nodiscard]] bool Submit(
        const LiquidationJob& job,
        const LiquidationSubmission& submission,
        const ethereum::Address& wallet
    );
    [[nodiscard]] bool Invalidate(const LiquidationJob& job, std::string_view reason);
    [[nodiscard]] std::size_t InvalidateNonCanonical(const ethereum::Uint256& chainId);
    [[nodiscard]] std::size_t ReconcileSubmitted();
    [[nodiscard]] LiquidationReconcileResult ReconcileSubmittedWithStats();

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
