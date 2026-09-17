#ifndef DLP_OBSERVABILITY_METRICS_HPP
#define DLP_OBSERVABILITY_METRICS_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>

namespace dlp::observability
{

class ServiceMetrics final
{
public:
    ServiceMetrics(std::string service, std::string address, std::uint16_t port);
    ~ServiceMetrics();

    ServiceMetrics(const ServiceMetrics&) = delete;
    ServiceMetrics& operator=(const ServiceMetrics&) = delete;
    ServiceMetrics(ServiceMetrics&&) = delete;
    ServiceMetrics& operator=(ServiceMetrics&&) = delete;

    [[nodiscard]] prometheus::Counter& AddCounter(std::string name, std::string help);
    [[nodiscard]] prometheus::Gauge& AddGauge(std::string name, std::string help);
    [[nodiscard]] prometheus::Histogram& AddHistogram(
        std::string name,
        std::string help,
        std::vector<double> buckets
    );
    void SetReady(bool ready) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> implementation_;
};

}

#endif
