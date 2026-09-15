#ifndef DLP_API_HTTP_SERVER_HPP
#define DLP_API_HTTP_SERVER_HPP

#include <cstdint>
#include <string>

#include "dlp/api/ApiService.hpp"

namespace dlp::api
{

class HttpServer final
{
public:
    HttpServer(ApiService& service, std::string address, std::uint16_t port);
    void Run();

private:
    ApiService& service_;
    std::string address_;
    std::uint16_t port_;
};

}

#endif
