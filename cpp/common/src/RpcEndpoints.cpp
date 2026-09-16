#include "dlp/ethereum/RpcEndpoints.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace dlp::ethereum
{

namespace
{

[[nodiscard]] std::string Trim(std::string_view value)
{
    const auto isSpace = [](char character) {
        return std::isspace(static_cast<unsigned char>(character)) != 0;
    };
    const auto first = std::find_if_not(value.begin(), value.end(), isSpace);
    const auto last = std::find_if_not(value.rbegin(), value.rend(), isSpace).base();
    return first < last ? std::string{first, last} : std::string{};
}

}

std::vector<std::string> ParseRpcEndpoints(std::string_view endpoints)
{
    std::vector<std::string> result;
    while(!endpoints.empty())
    {
        const auto separator = endpoints.find(',');
        auto endpoint = Trim(endpoints.substr(0, separator));
        if(!endpoint.empty())
        {
            result.push_back(std::move(endpoint));
        }
        if(separator == std::string_view::npos)
        {
            break;
        }
        endpoints.remove_prefix(separator + 1U);
    }
    return result;
}

std::vector<std::string> MergeRpcEndpoints(
    std::string primary,
    std::vector<std::string> additional
)
{
    std::vector<std::string> result;
    result.reserve(additional.size() + 1U);
    result.push_back(std::move(primary));
    for(auto& endpoint : additional)
    {
        if(std::find(result.begin(), result.end(), endpoint) == result.end())
        {
            result.push_back(std::move(endpoint));
        }
    }
    return result;
}

}
