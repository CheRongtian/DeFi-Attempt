#include "dlp/api/HttpServer.hpp"

#include <cstddef>
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

void ServeConnection(ApiService& service, Tcp::socket socket)
{
    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    beast::error_code error;
    http::read(socket, buffer, request, error);
    if(error)
    {
        return;
    }
    const auto method = request.method_string();
    const auto target = request.target();
    const auto result = service.Handle(ApiRequest{
        std::string{method.data(), method.size()},
        std::string{target.data(), target.size()},
        request.body()
    });

    http::response<http::string_body> response{
        static_cast<http::status>(result.status),
        request.version()
    };
    response.set(http::field::content_type, "application/json");
    response.set(http::field::server, "dlp-api");
    response.keep_alive(false);
    response.body() = result.body;
    response.prepare_payload();
    http::write(socket, response, error);
    if(error)
    {
        return;
    }

    socket.shutdown(Tcp::socket::shutdown_send, error);
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
    for(;;)
    {
        Tcp::socket socket{context};
        acceptor.accept(socket);
        asio::post(workers, [this, socket = std::move(socket)]() mutable {
            ServeConnection(service_, std::move(socket));
        });
    }
}

}
