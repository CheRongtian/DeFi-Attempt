#include "dlp/options/OptionPricing.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

namespace
{

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using Tcp = asio::ip::tcp;
using Json = nlohmann::json;
using dlp::options::MetalMonteCarlo;
using dlp::options::PricingRequest;
using dlp::options::PricingResult;

PricingRequest ParseRequest(const Json& body)
{
    return PricingRequest{
        .optionType = dlp::options::ParseOptionType(body.at("optionType").get<std::string>()),
        .spot = body.at("spot").get<double>(),
        .strike = body.at("strike").get<double>(),
        .expiryYears = body.at("expiryYears").get<double>(),
        .volatility = body.at("volatility").get<double>(),
        .riskFreeRate = body.at("riskFreeRate").get<double>(),
        .paths = body.at("paths").get<std::size_t>(),
        .seed = body.value("seed", 42U)
    };
}

Json ToJson(const PricingRequest& request, const PricingResult& result)
{
    return Json{
        {"optionType", std::string{dlp::options::ToString(request.optionType)}},
        {"monteCarloPrice", result.monteCarloPrice},
        {"analyticPrice", result.analyticPrice},
        {"standardError", result.standardError},
        {"absoluteDifference", result.absoluteDifference},
        {"paths", result.paths},
        {"elapsedMilliseconds", result.elapsedMilliseconds},
        {"device", result.device}
    };
}

http::response<http::string_body> JsonResponse(
    http::status status,
    unsigned version,
    const Json& body
)
{
    http::response<http::string_body> response{status, version};
    response.set(http::field::content_type, "application/json");
    response.set(http::field::server, "dlp-metal-option-pricer");
    response.keep_alive(false);
    response.body() = body.dump();
    response.prepare_payload();
    return response;
}

http::response<http::string_body> HandleRequest(
    const MetalMonteCarlo& pricer,
    const http::request<http::string_body>& request
)
{
    if(request.method() == http::verb::get && request.target() == "/health")
    {
        return JsonResponse(http::status::ok, request.version(), Json{
            {"status", "ready"},
            {"backend", "Apple Metal"},
            {"device", pricer.DeviceName()}
        });
    }
    if(request.method() != http::verb::post || request.target() != "/price")
    {
        return JsonResponse(http::status::not_found, request.version(), Json{
            {"error", "route not found"}
        });
    }

    try
    {
        const PricingRequest pricingRequest = ParseRequest(Json::parse(request.body()));
        return JsonResponse(
            http::status::ok,
            request.version(),
            ToJson(pricingRequest, pricer.Price(pricingRequest))
        );
    }
    catch(const std::invalid_argument& error)
    {
        return JsonResponse(http::status::bad_request, request.version(), Json{{"error", error.what()}});
    }
    catch(const nlohmann::json::exception& error)
    {
        return JsonResponse(http::status::bad_request, request.version(), Json{{"error", error.what()}});
    }
    catch(const std::exception& error)
    {
        return JsonResponse(
            http::status::internal_server_error,
            request.version(),
            Json{{"error", error.what()}}
        );
    }
}

void Serve(const MetalMonteCarlo& pricer, const std::string& host, std::uint16_t port)
{
    asio::io_context context;
    Tcp::acceptor acceptor{context, {asio::ip::make_address(host), port}};
    std::cout << "Metal option pricer listening on http://" << host << ':' << port << '\n';

    for(;;)
    {
        Tcp::socket socket{context};
        acceptor.accept(socket);

        beast::flat_buffer buffer;
        http::request<http::string_body> request;
        beast::error_code error;
        http::read(socket, buffer, request, error);
        if(error)
        {
            continue;
        }

        auto response = HandleRequest(pricer, request);
        http::write(socket, response, error);
        socket.shutdown(Tcp::socket::shutdown_send, error);
    }
}

PricingRequest ParseCliRequest(int argc, char* argv[])
{
    if(argc != 9 && argc != 10)
    {
        throw std::invalid_argument{
            "usage: dlp_metal_option_pricer price <call|put> <spot> <strike> "
            "<expiry-years> <volatility> <rate> <paths> [seed]"
        };
    }

    return PricingRequest{
        .optionType = dlp::options::ParseOptionType(argv[2]),
        .spot = std::stod(argv[3]),
        .strike = std::stod(argv[4]),
        .expiryYears = std::stod(argv[5]),
        .volatility = std::stod(argv[6]),
        .riskFreeRate = std::stod(argv[7]),
        .paths = static_cast<std::size_t>(std::stoull(argv[8])),
        .seed = argc == 10 ? static_cast<std::uint32_t>(std::stoul(argv[9])) : 42U
    };
}

std::uint16_t ParsePort(const char* value)
{
    const auto port = std::stoul(value);
    if(port > std::numeric_limits<std::uint16_t>::max())
    {
        throw std::invalid_argument{"port must be between 0 and 65535"};
    }
    return static_cast<std::uint16_t>(port);
}

}

int main(int argc, char* argv[])
{
    try
    {
        const MetalMonteCarlo pricer;
        const std::string_view command = argc > 1 ? argv[1] : "serve";
        if(command == "price")
        {
            const PricingRequest request = ParseCliRequest(argc, argv);
            std::cout << ToJson(request, pricer.Price(request)).dump(2) << '\n';
            return EXIT_SUCCESS;
        }
        if(command != "serve")
        {
            throw std::invalid_argument{"command must be serve or price"};
        }

        const std::string host = argc > 2 ? argv[2] : "127.0.0.1";
        const std::uint16_t port = argc > 3 ? ParsePort(argv[3]) : 18081U;
        Serve(pricer, host, port);
        return EXIT_SUCCESS;
    }
    catch(const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
