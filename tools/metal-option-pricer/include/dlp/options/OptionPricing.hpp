#ifndef DLP_OPTIONS_OPTION_PRICING_HPP
#define DLP_OPTIONS_OPTION_PRICING_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace dlp::options
{

enum class OptionType
{
    Call,
    Put
};

struct PricingRequest
{
    OptionType optionType{OptionType::Call};
    double spot{};
    double strike{};
    double expiryYears{};
    double volatility{};
    double riskFreeRate{};
    std::size_t paths{};
    std::uint32_t seed{};
};

struct PricingResult
{
    double monteCarloPrice{};
    double analyticPrice{};
    double standardError{};
    double absoluteDifference{};
    std::size_t paths{};
    double elapsedMilliseconds{};
    std::string device;
};

[[nodiscard]] OptionType ParseOptionType(std::string_view value);
[[nodiscard]] std::string_view ToString(OptionType optionType) noexcept;
[[nodiscard]] double Payoff(OptionType optionType, double strike, double spot);
[[nodiscard]] double BlackScholesPrice(const PricingRequest& request);

class MetalMonteCarlo final
{
public:
    MetalMonteCarlo();
    ~MetalMonteCarlo();

    MetalMonteCarlo(const MetalMonteCarlo&) = delete;
    MetalMonteCarlo& operator=(const MetalMonteCarlo&) = delete;
    MetalMonteCarlo(MetalMonteCarlo&&) noexcept;
    MetalMonteCarlo& operator=(MetalMonteCarlo&&) noexcept;

    [[nodiscard]] PricingResult Price(const PricingRequest& request) const;
    [[nodiscard]] std::string DeviceName() const;

private:
    class Implementation;
    std::unique_ptr<Implementation> implementation_;
};

}

#endif
