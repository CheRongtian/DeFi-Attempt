#ifndef DLP_ETHEREUM_RPC_ENDPOINTS_HPP
#define DLP_ETHEREUM_RPC_ENDPOINTS_HPP

#include <string>
#include <string_view>
#include <vector>

namespace dlp::ethereum
{

[[nodiscard]] std::vector<std::string> ParseRpcEndpoints(std::string_view endpoints);

[[nodiscard]] std::vector<std::string> MergeRpcEndpoints(
    std::string primary,
    std::vector<std::string> additional
);

}

#endif
