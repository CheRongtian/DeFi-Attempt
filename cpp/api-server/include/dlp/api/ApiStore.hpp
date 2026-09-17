#ifndef DLP_API_API_STORE_HPP
#define DLP_API_API_STORE_HPP

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "dlp/api/ApiTypes.hpp"

namespace dlp::api
{

class ApiStore
{
public:
    virtual ~ApiStore() = default;

    [[nodiscard]] virtual std::vector<LiquidationRecord> LoadLiquidations(
        const ethereum::Uint256& chainId,
        std::size_t limit
    ) const = 0;
    [[nodiscard]] virtual ProtocolStats LoadProtocolStats(
        const ethereum::Uint256& chainId
    ) const = 0;
    [[nodiscard]] virtual ethereum::Uint256 LoadUsdcSupply(
        const ethereum::Uint256& chainId,
        const ethereum::Address& user
    ) const = 0;
};

class PostgresApiStore final : public ApiStore
{
public:
    explicit PostgresApiStore(std::string connectionString);
    ~PostgresApiStore() override;

    PostgresApiStore(PostgresApiStore&& other) noexcept;
    PostgresApiStore& operator=(PostgresApiStore&& other) noexcept;

    PostgresApiStore(const PostgresApiStore&) = delete;
    PostgresApiStore& operator=(const PostgresApiStore&) = delete;

    [[nodiscard]] std::vector<LiquidationRecord> LoadLiquidations(
        const ethereum::Uint256& chainId,
        std::size_t limit
    ) const override;
    [[nodiscard]] ProtocolStats LoadProtocolStats(
        const ethereum::Uint256& chainId
    ) const override;
    [[nodiscard]] ethereum::Uint256 LoadUsdcSupply(
        const ethereum::Uint256& chainId,
        const ethereum::Address& user
    ) const override;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
