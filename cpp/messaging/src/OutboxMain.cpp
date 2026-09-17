#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "dlp/messaging/JetStream.hpp"
#include "dlp/messaging/Outbox.hpp"
#include "dlp/observability/Metrics.hpp"

namespace
{

std::atomic_bool running{true};

void Stop(int)
{
    running.store(false);
}

[[nodiscard]] std::string EnvironmentOrDefault(const char* name, const char* fallback)
{
    const auto* value = std::getenv(name);
    return value == nullptr || *value == '\0' ? std::string{fallback} : std::string{value};
}

}

int main(int argc, char* argv[])
{
    const bool runOnce = argc == 2 && std::string{argv[1]} == "--once";
    if(argc > 2 || (argc == 2 && !runOnce))
    {
        std::cerr << "usage: dlp_outbox_publisher [--once]\n";
        return 2;
    }

    std::signal(SIGINT, Stop);
    std::signal(SIGTERM, Stop);

    try
    {
        dlp::messaging::PostgresOutboxStore store{EnvironmentOrDefault(
            "DLP_DATABASE_URL",
            "postgresql://dlp:dlp@127.0.0.1:5432/dlp"
        )};
        dlp::messaging::JetStreamPublisher jetStream{
            EnvironmentOrDefault("DLP_NATS_URL", "nats://127.0.0.1:4222")
        };
        jetStream.EnsureEventStream();
        dlp::messaging::OutboxPublisher publisher{store, jetStream};
        dlp::observability::ServiceMetrics metrics{
            "outbox-publisher",
            EnvironmentOrDefault("DLP_METRICS_ADDRESS", "0.0.0.0"),
            static_cast<std::uint16_t>(std::stoul(EnvironmentOrDefault("DLP_METRICS_PORT", "9102")))
        };
        auto& publishedTotal = metrics.AddCounter(
            "outbox_published_total",
            "Total outbox events published to JetStream"
        );
        auto& pending = metrics.AddGauge("outbox_pending", "Current unpublished outbox events");
        auto& errors = metrics.AddCounter("outbox_publish_errors_total", "Total outbox publish failures");
        metrics.SetReady(true);

        do
        {
            try
            {
                const auto published = publisher.RunOnce(100);
                if(published != 0U)
                {
                    std::cout << "published " << published << " outbox event(s)\n";
                    publishedTotal.Increment(static_cast<double>(published));
                }
                pending.Set(static_cast<double>(store.CountUnpublished()));
            }
            catch(const std::exception& exception)
            {
                errors.Increment();
                if(runOnce)
                {
                    throw;
                }
                std::cerr << "outbox publish failed: " << exception.what() << '\n';
            }
            if(!runOnce)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{200});
            }
        }
        while(running.load() && !runOnce);
    }
    catch(const std::exception& exception)
    {
        std::cerr << "outbox publisher stopped: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
