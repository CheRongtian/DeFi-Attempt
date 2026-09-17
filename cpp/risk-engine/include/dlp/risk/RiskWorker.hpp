#ifndef DLP_RISK_RISK_WORKER_HPP
#define DLP_RISK_RISK_WORKER_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "dlp/risk/RiskEngine.hpp"

namespace dlp::risk
{

class RiskEventStore
{
public:
    virtual ~RiskEventStore() = default;
    [[nodiscard]] virtual bool CommitScan(
        std::string_view sourceEventId,
        const std::vector<LiquidationCandidate>& candidates
    ) = 0;
};

class PostgresRiskEventStore final : public RiskEventStore
{
public:
    explicit PostgresRiskEventStore(std::string connectionString);
    ~PostgresRiskEventStore() override;

    PostgresRiskEventStore(PostgresRiskEventStore&& other) noexcept;
    PostgresRiskEventStore& operator=(PostgresRiskEventStore&& other) noexcept;

    PostgresRiskEventStore(const PostgresRiskEventStore&) = delete;
    PostgresRiskEventStore& operator=(const PostgresRiskEventStore&) = delete;

    [[nodiscard]] bool CommitScan(
        std::string_view sourceEventId,
        const std::vector<LiquidationCandidate>& candidates
    ) override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

struct RiskWorkResult
{
    bool committed{false};
    std::size_t positionsScanned{0};
    std::size_t liquidationCandidates{0};
};

class RiskWorker final
{
public:
    RiskWorker(RiskEngine& engine, RiskEventStore& store, std::size_t scanLimit);

    [[nodiscard]] bool Process(std::string_view sourceEventId, std::uint64_t evaluatedAt);
    [[nodiscard]] bool Rescan(std::uint64_t evaluatedAt);
    [[nodiscard]] RiskWorkResult ProcessWithStats(
        std::string_view sourceEventId,
        std::uint64_t evaluatedAt
    );
    [[nodiscard]] RiskWorkResult RescanWithStats(std::uint64_t evaluatedAt);

private:
    RiskEngine& engine_;
    RiskEventStore& store_;
    std::size_t scanLimit_;
};

}

#endif
