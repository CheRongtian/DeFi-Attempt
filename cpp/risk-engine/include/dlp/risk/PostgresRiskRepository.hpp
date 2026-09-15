#ifndef DLP_RISK_POSTGRES_RISK_REPOSITORY_HPP
#define DLP_RISK_POSTGRES_RISK_REPOSITORY_HPP

#include <memory>
#include <string>

#include "dlp/risk/RiskRepository.hpp"

namespace dlp::risk
{

class PostgresRiskRepository final : public RiskRepository
{
public:
    explicit PostgresRiskRepository(std::string connectionString);
    ~PostgresRiskRepository() override;

    PostgresRiskRepository(PostgresRiskRepository&& other) noexcept;
    PostgresRiskRepository& operator=(PostgresRiskRepository&& other) noexcept;

    PostgresRiskRepository(const PostgresRiskRepository&) = delete;
    PostgresRiskRepository& operator=(const PostgresRiskRepository&) = delete;

    [[nodiscard]] MarketSnapshot LoadMarket(const ethereum::Uint256& chainId) const override;
    [[nodiscard]] std::optional<IndexedPosition> LoadPosition(
        const ethereum::Uint256& chainId,
        const ethereum::Address& user
    ) const override;
    [[nodiscard]] std::vector<IndexedPosition> LoadPositions(
        const ethereum::Uint256& chainId,
        std::size_t limit
    ) const override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
