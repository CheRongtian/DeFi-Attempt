#include "dlp/api/HttpServer.hpp"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace dlp::api
{

namespace
{

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using Tcp = asio::ip::tcp;
constexpr std::size_t WORKER_COUNT = 4;
constexpr std::size_t MAXIMUM_REQUEST_BODY = 16U * 1024U;
constexpr std::size_t REQUESTS_PER_SECOND = 100;
constexpr auto IO_TIMEOUT = std::chrono::seconds{5};

class RequestRateLimiter final
{
public:
    [[nodiscard]] bool Allow()
    {
        const auto now = std::chrono::steady_clock::now();
        std::scoped_lock lock{mutex_};
        if(now - windowStart_ >= std::chrono::seconds{1})
        {
            windowStart_ = now;
            requestCount_ = 0;
        }
        ++requestCount_;
        return requestCount_ <= REQUESTS_PER_SECOND;
    }

private:
    std::mutex mutex_;
    std::chrono::steady_clock::time_point windowStart_{std::chrono::steady_clock::now()};
    std::size_t requestCount_{0};
};

void WriteJsonResponse(
    beast::tcp_stream& stream,
    unsigned status,
    unsigned version,
    std::string body
)
{
    http::response<http::string_body> response{
        static_cast<http::status>(status),
        version
    };
    response.set(http::field::content_type, "application/json");
    response.set(http::field::server, "dlp-api");
    response.keep_alive(false);
    response.body() = std::move(body);
    response.prepare_payload();
    beast::error_code error;
    stream.expires_after(IO_TIMEOUT);
    http::write(stream, response, error);
}

void ServeConnection(ApiService& service, RequestRateLimiter& limiter, Tcp::socket socket)
{
    beast::tcp_stream stream{std::move(socket)};
    stream.expires_after(IO_TIMEOUT);
    beast::flat_buffer buffer;
    http::request_parser<http::string_body> parser;
    parser.body_limit(MAXIMUM_REQUEST_BODY);
    beast::error_code error;
    http::read(stream, buffer, parser, error);
    if(error == http::error::body_limit)
    {
        WriteJsonResponse(stream, 413, 11, R"({"error":"request body too large"})");
        return;
    }
    if(error)
    {
        return;
    }
    auto request = parser.release();
    if(!limiter.Allow())
    {
        WriteJsonResponse(
            stream,
            429,
            request.version(),
            R"({"error":"request rate limit exceeded"})"
        );
        return;
    }
    const auto method = request.method_string();
    const auto target = request.target();
    const auto result = service.Handle(ApiRequest{
        std::string{method.data(), method.size()},
        std::string{target.data(), target.size()},
        request.body()
    });

    WriteJsonResponse(stream, result.status, request.version(), result.body);
    stream.socket().shutdown(Tcp::socket::shutdown_send, error);
}

}

HttpServer::HttpServer(ApiService& service, std::string address, std::uint16_t port)
    : service_(service), address_(std::move(address)), port_(port)
{
}

void HttpServer::Run()
{
    asio::io_context context;
    Tcp::acceptor acceptor{context, {asio::ip::make_address(address_), port_}};
    asio::thread_pool workers{WORKER_COUNT};
    RequestRateLimiter limiter;
    for(;;)
    {
        Tcp::socket socket{context};
        acceptor.accept(socket);
        asio::post(workers, [this, &limiter, socket = std::move(socket)]() mutable {
            ServeConnection(service_, limiter, std::move(socket));
        });
    }
}

}
