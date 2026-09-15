#include "dlp/api/HttpServer.hpp"

#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace dlp::api
{

HttpServer::HttpServer(ApiService& service, std::string address, std::uint16_t port)
    : service_(service), address_(std::move(address)), port_(port)
{
}

void HttpServer::Run()
{
    namespace asio = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    using Tcp = asio::ip::tcp;

    asio::io_context context;
    Tcp::acceptor acceptor{context, {asio::ip::make_address(address_), port_}};
    for(;;)
    {
        Tcp::socket socket{context};
        acceptor.accept(socket);

        beast::flat_buffer buffer;
        http::request<http::string_body> request;
        http::read(socket, buffer, request);
        const auto method = request.method_string();
        const auto target = request.target();
        const auto result = service_.Handle(ApiRequest{
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
        http::write(socket, response);

        beast::error_code error;
        socket.shutdown(Tcp::socket::shutdown_send, error);
    }
}

}
