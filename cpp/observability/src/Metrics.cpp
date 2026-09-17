#include "dlp/observability/Metrics.hpp"

#include <atomic>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>

namespace dlp::observability
{

namespace
{

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using Tcp = asio::ip::tcp;

[[nodiscard]] std::string Serialize(const std::shared_ptr<prometheus::Registry>& registry)
{
    std::ostringstream output;
    prometheus::TextSerializer{}.Serialize(output, registry->Collect());
    return output.str();
}

}

class ServiceMetrics::Impl final
{
public:
    Impl(std::string service, std::string address, std::uint16_t port)
        : service_(std::move(service)),
          registry_(std::make_shared<prometheus::Registry>()),
          acceptor_(context_, {asio::ip::make_address(address), port}),
          healthy_(prometheus::BuildGauge()
              .Name("service_health")
              .Help("Whether the service process is healthy")
              .Register(*registry_)
              .Add({{"service", service_}})),
          ready_(prometheus::BuildGauge()
              .Name("service_ready")
              .Help("Whether the service has completed initialization")
              .Register(*registry_)
              .Add({{"service", service_}}))
    {
        healthy_.Set(1.0);
        Accept();
        thread_ = std::jthread([this] { context_.run(); });
    }

    ~Impl()
    {
        context_.stop();
    }

    [[nodiscard]] prometheus::Counter& AddCounter(std::string name, std::string help)
    {
        return prometheus::BuildCounter()
            .Name(std::move(name))
            .Help(std::move(help))
            .Register(*registry_)
            .Add({{"service", service_}});
    }

    [[nodiscard]] prometheus::Gauge& AddGauge(std::string name, std::string help)
    {
        return prometheus::BuildGauge()
            .Name(std::move(name))
            .Help(std::move(help))
            .Register(*registry_)
            .Add({{"service", service_}});
    }

    [[nodiscard]] prometheus::Histogram& AddHistogram(
        std::string name,
        std::string help,
        std::vector<double> buckets
    )
    {
        return prometheus::BuildHistogram()
            .Name(std::move(name))
            .Help(std::move(help))
            .Register(*registry_)
            .Add({{"service", service_}}, std::move(buckets));
    }

    void SetReady(bool ready) noexcept
    {
        ready_.Set(ready ? 1.0 : 0.0);
    }

private:
    void Accept()
    {
        acceptor_.async_accept([this](beast::error_code error, Tcp::socket socket) {
            if(!error)
            {
                Serve(std::move(socket));
            }
            if(acceptor_.is_open())
            {
                Accept();
            }
        });
    }

    void Serve(Tcp::socket socket)
    {
        beast::flat_buffer buffer;
        http::request<http::empty_body> request;
        beast::error_code error;
        http::read(socket, buffer, request, error);
        if(error)
        {
            return;
        }

        const auto target = std::string_view{request.target().data(), request.target().size()};
        http::response<http::string_body> response{http::status::not_found, request.version()};
        response.keep_alive(false);
        if(request.method() != http::verb::get)
        {
            response.result(http::status::method_not_allowed);
            response.body() = "method not allowed\n";
            response.set(http::field::content_type, "text/plain");
        }
        else if(target == "/metrics")
        {
            response.result(http::status::ok);
            response.body() = Serialize(registry_);
            response.set(http::field::content_type, "text/plain; version=0.0.4; charset=utf-8");
        }
        else if(target == "/health")
        {
            response.result(http::status::ok);
            response.body() = R"({"status":"healthy"})";
            response.set(http::field::content_type, "application/json");
        }
        else if(target == "/ready")
        {
            const auto ready = ready_.Value() == 1.0;
            response.result(ready ? http::status::ok : http::status::service_unavailable);
            response.body() = ready ? R"({"status":"ready"})" : R"({"status":"starting"})";
            response.set(http::field::content_type, "application/json");
        }
        else
        {
            response.body() = "not found\n";
            response.set(http::field::content_type, "text/plain");
        }
        response.prepare_payload();
        http::write(socket, response, error);
        socket.shutdown(Tcp::socket::shutdown_send, error);
    }

    std::string service_;
    std::shared_ptr<prometheus::Registry> registry_;
    asio::io_context context_;
    Tcp::acceptor acceptor_;
    prometheus::Gauge& healthy_;
    prometheus::Gauge& ready_;
    std::jthread thread_;
};

ServiceMetrics::ServiceMetrics(std::string service, std::string address, std::uint16_t port)
    : implementation_(std::make_unique<Impl>(std::move(service), std::move(address), port))
{
}

ServiceMetrics::~ServiceMetrics() = default;

prometheus::Counter& ServiceMetrics::AddCounter(std::string name, std::string help)
{
    return implementation_->AddCounter(std::move(name), std::move(help));
}

prometheus::Gauge& ServiceMetrics::AddGauge(std::string name, std::string help)
{
    return implementation_->AddGauge(std::move(name), std::move(help));
}

prometheus::Histogram& ServiceMetrics::AddHistogram(
    std::string name,
    std::string help,
    std::vector<double> buckets
)
{
    return implementation_->AddHistogram(std::move(name), std::move(help), std::move(buckets));
}

void ServiceMetrics::SetReady(bool ready) noexcept
{
    implementation_->SetReady(ready);
}

}
