#ifndef DLP_RISK_RISK_REPOSITORY_HPP
#define DLP_RISK_RISK_REPOSITORY_HPP

#include <cstddef>
#include <optional>
#include <vector>

#include "dlp/risk/RiskTypes.hpp"

namespace dlp::risk
{

class RiskRepository
{
public:
    virtual ~RiskRepository() = default;

    [[nodiscard]] virtual MarketSnapshot LoadMarket(const ethereum::Uint256& chainId) const = 0;
    [[nodiscard]] virtual std::optional<IndexedPosition> LoadPosition(
        const ethereum::Uint256& chainId,
        const ethereum::Address& user
    ) const = 0;
    [[nodiscard]] virtual std::vector<IndexedPosition> LoadPositions(
        const ethereum::Uint256& chainId,
        std::size_t limit
    ) const = 0;
};

}

#endif
