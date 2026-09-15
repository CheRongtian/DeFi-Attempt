#include "dlp/ethereum/RpcClient.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <string_view>
#include <utility>

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

namespace dlp::ethereum
{

namespace
{

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using Tcp = asio::ip::tcp;
using Json = nlohmann::json;

struct ParsedEndpoint
{
    std::string host;
    std::string port;
    std::string target;
};

[[nodiscard]] ParsedEndpoint ParseEndpoint(std::string_view endpoint)
{
    static constexpr std::string_view HTTP_PREFIX = "http://";
    if(endpoint.substr(0, HTTP_PREFIX.size()) != HTTP_PREFIX)
    {
        throw RpcException(RpcErrorKind::InvalidEndpoint, "RPC endpoint must use http://");
    }

    endpoint.remove_prefix(HTTP_PREFIX.size());
    const auto pathPosition = endpoint.find('/');
    const auto authority = endpoint.substr(0, pathPosition);
    const auto target = pathPosition == std::string_view::npos ? std::string_view{"/"} : endpoint.substr(pathPosition);

    if(authority.empty())
    {
        throw RpcException(RpcErrorKind::InvalidEndpoint, "RPC endpoint host is empty");
    }

    const auto portPosition = authority.rfind(':');
    const auto hasPort = portPosition != std::string_view::npos;
    const auto host = hasPort ? authority.substr(0, portPosition) : authority;
    const auto port = hasPort ? authority.substr(portPosition + 1) : std::string_view{"80"};

    if(host.empty() || port.empty() || !std::all_of(port.begin(), port.end(), [](char value) {
           return std::isdigit(static_cast<unsigned char>(value)) != 0;
       }))
    {
        throw RpcException(RpcErrorKind::InvalidEndpoint, "RPC endpoint host or port is invalid");
    }

    return ParsedEndpoint{std::string{host}, std::string{port}, std::string{target}};
}

[[nodiscard]] const std::string& RequireString(const Json& object, std::string_view field)
{
    const auto iterator = object.find(std::string{field});
    if(iterator == object.end() || !iterator->is_string())
    {
        throw RpcException(
            RpcErrorKind::InvalidResponse,
            "RPC response field is missing or has the wrong type: " + std::string{field}
        );
    }
    return iterator->get_ref<const std::string&>();
}

[[nodiscard]] Uint256 ParseQuantity(const Json& object, std::string_view field)
{
    try
    {
        return Uint256::FromHex(RequireString(object, field));
    }
    catch(const RpcException&)
    {
        throw;
    }
    catch(const std::exception& exception)
    {
        throw RpcException(
            RpcErrorKind::InvalidResponse,
            "invalid Ethereum quantity in field " + std::string{field} + ": " + exception.what()
        );
    }
}

[[nodiscard]] Hash256 ParseHash(const Json& object, std::string_view field)
{
    try
    {
        const auto bytes = Hex::Decode(RequireString(object, field));
        if(bytes.size() != Hash256{}.size())
        {
            throw std::invalid_argument("hash must contain exactly 32 bytes");
        }

        Hash256 hash{};
        std::copy(bytes.begin(), bytes.end(), hash.begin());
        return hash;
    }
    catch(const RpcException&)
    {
        throw;
    }
    catch(const std::exception& exception)
    {
        throw RpcException(
            RpcErrorKind::InvalidResponse,
            "invalid Ethereum hash in field " + std::string{field} + ": " + exception.what()
        );
    }
}

[[nodiscard]] Address ParseAddress(const Json& object, std::string_view field)
{
    try
    {
        return Address::FromHex(RequireString(object, field));
    }
    catch(const RpcException&)
    {
        throw;
    }
    catch(const std::exception& exception)
    {
        throw RpcException(
            RpcErrorKind::InvalidResponse,
            "invalid Ethereum address in field " + std::string{field} + ": " + exception.what()
        );
    }
}

[[nodiscard]] std::string HashToHex(const Hash256& hash)
{
    return Hex::Encode(hash);
}

[[nodiscard]] Json EncodeCall(const TransactionCall& call)
{
    Json result{{"to", call.to.ToHex()}, {"data", Hex::Encode(call.data)}};
    if(call.from.has_value())
    {
        result["from"] = call.from->ToHex();
    }
    if(!call.value.IsZero())
    {
        result["value"] = call.value.ToQuantity();
    }
    return result;
}

[[nodiscard]] Bytes ParseBytesResult(const Json& result, std::string_view method)
{
    if(!result.is_string())
    {
        throw RpcException(
            RpcErrorKind::InvalidResponse,
            std::string{method} + " result is not a hex string"
        );
    }
    try
    {
        return Hex::Decode(result.get_ref<const std::string&>());
    }
    catch(const std::exception& exception)
    {
        throw RpcException(
            RpcErrorKind::InvalidResponse,
            std::string{method} + " returned invalid hex data: " + exception.what()
        );
    }
}

}

class RpcClient::Impl final
{
public:
    Impl(std::string endpoint, std::chrono::milliseconds timeout)
        : endpoint_(ParseEndpoint(endpoint)), timeout_(timeout)
    {
        if(timeout_ <= std::chrono::milliseconds::zero())
        {
            throw RpcException(RpcErrorKind::InvalidEndpoint, "RPC timeout must be positive");
        }
    }

    [[nodiscard]] Json Call(std::string_view method, Json parameters) const
    {
        const auto requestId = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
        const Json payload{
            {"jsonrpc", "2.0"},
            {"id", requestId},
            {"method", std::string{method}},
            {"params", std::move(parameters)}
        };

        try
        {
            asio::io_context context;
            Tcp::resolver resolver{context};
            beast::tcp_stream stream{context};
            stream.expires_after(timeout_);
            stream.connect(resolver.resolve(endpoint_.host, endpoint_.port));

            http::request<http::string_body> request{http::verb::post, endpoint_.target, 11};
            request.set(http::field::host, endpoint_.host + ":" + endpoint_.port);
            request.set(http::field::content_type, "application/json");
            request.set(http::field::user_agent, "dlp-cpp-rpc/1.0");
            request.body() = payload.dump();
            request.prepare_payload();

            http::write(stream, request);

            beast::flat_buffer buffer;
            http::response<http::string_body> response;
            http::read(stream, buffer, response);

            beast::error_code shutdownError;
            stream.socket().shutdown(Tcp::socket::shutdown_both, shutdownError);

            if(response.result() != http::status::ok)
            {
                throw RpcException(
                    RpcErrorKind::Http,
                    "RPC HTTP request failed with status " + std::to_string(response.result_int())
                );
            }

            auto decoded = Json::parse(response.body(), nullptr, false);
            if(decoded.is_discarded() || !decoded.is_object())
            {
                throw RpcException(RpcErrorKind::InvalidResponse, "RPC response is not a JSON object");
            }
            if(decoded.value("jsonrpc", std::string{}) != "2.0")
            {
                throw RpcException(RpcErrorKind::Protocol, "RPC response has an invalid JSON-RPC version");
            }
            if(!decoded.contains("id") || decoded["id"] != requestId)
            {
                throw RpcException(RpcErrorKind::Protocol, "RPC response ID does not match the request");
            }
            if(const auto error = decoded.find("error"); error != decoded.end() && !error->is_null())
            {
                const auto message = error->value("message", std::string{"unknown RPC error"});
                throw RpcException(RpcErrorKind::Remote, message);
            }
            if(!decoded.contains("result"))
            {
                throw RpcException(RpcErrorKind::InvalidResponse, "RPC response does not contain a result");
            }

            return decoded["result"];
        }
        catch(const RpcException&)
        {
            throw;
        }
        catch(const boost::system::system_error& exception)
        {
            throw RpcException(RpcErrorKind::Transport, "RPC transport failed: " + std::string{exception.what()});
        }
        catch(const Json::exception& exception)
        {
            throw RpcException(RpcErrorKind::InvalidResponse, "RPC JSON handling failed: " + std::string{exception.what()});
        }
    }

private:
    ParsedEndpoint endpoint_;
    std::chrono::milliseconds timeout_;
    mutable std::atomic<std::uint64_t> nextRequestId_{1};
};

RpcException::RpcException(RpcErrorKind kind, std::string message)
    : std::runtime_error(std::move(message)), kind_(kind)
{
}

RpcErrorKind RpcException::GetKind() const noexcept
{
    return kind_;
}

RpcClient::RpcClient(std::string endpoint, std::chrono::milliseconds timeout)
    : implementation_(std::make_unique<Impl>(std::move(endpoint), timeout))
{
}

RpcClient::~RpcClient() = default;
RpcClient::RpcClient(RpcClient&& other) noexcept = default;
RpcClient& RpcClient::operator=(RpcClient&& other) noexcept = default;

Uint256 RpcClient::GetChainId() const
{
    const auto result = implementation_->Call("eth_chainId", Json::array());
    return ParseQuantity(Json{{"result", result}}, "result");
}

Uint256 RpcClient::GetBlockNumber() const
{
    const auto result = implementation_->Call("eth_blockNumber", Json::array());
    return ParseQuantity(Json{{"result", result}}, "result");
}

std::optional<BlockHeader> RpcClient::GetBlockByNumber(const Uint256& number) const
{
    const auto result = implementation_->Call(
        "eth_getBlockByNumber",
        Json::array({number.ToQuantity(), false})
    );

    if(result.is_null())
    {
        return std::nullopt;
    }
    if(!result.is_object())
    {
        throw RpcException(RpcErrorKind::InvalidResponse, "eth_getBlockByNumber result is not an object");
    }

    return BlockHeader{
        ParseQuantity(result, "number"),
        ParseHash(result, "hash"),
        ParseHash(result, "parentHash"),
        ParseQuantity(result, "timestamp"),
        result.contains("baseFeePerGas") && !result["baseFeePerGas"].is_null()
            ? std::optional<Uint256>{ParseQuantity(result, "baseFeePerGas")}
            : std::nullopt
    };
}

std::vector<RpcLog> RpcClient::GetLogs(const LogFilter& filter) const
{
    Json encodedFilter = Json::object();
    if(filter.fromBlock.has_value())
    {
        encodedFilter["fromBlock"] = filter.fromBlock->ToQuantity();
    }
    if(filter.toBlock.has_value())
    {
        encodedFilter["toBlock"] = filter.toBlock->ToQuantity();
    }
    if(filter.address.has_value())
    {
        encodedFilter["address"] = filter.address->ToHex();
    }
    if(!filter.topics.empty())
    {
        auto encodedTopics = Json::array();
        for(const auto& topic : filter.topics)
        {
            encodedTopics.push_back(topic.has_value() ? Json(HashToHex(*topic)) : Json(nullptr));
        }
        encodedFilter["topics"] = std::move(encodedTopics);
    }

    const auto result = implementation_->Call("eth_getLogs", Json::array({std::move(encodedFilter)}));
    if(!result.is_array())
    {
        throw RpcException(RpcErrorKind::InvalidResponse, "eth_getLogs result is not an array");
    }

    std::vector<RpcLog> logs;
    logs.reserve(result.size());

    for(const auto& encodedLog : result)
    {
        if(!encodedLog.is_object())
        {
            throw RpcException(RpcErrorKind::InvalidResponse, "eth_getLogs returned a non-object entry");
        }

        const auto encodedTopics = encodedLog.find("topics");
        if(encodedTopics == encodedLog.end() || !encodedTopics->is_array())
        {
            throw RpcException(RpcErrorKind::InvalidResponse, "RPC log topics are missing or invalid");
        }

        std::vector<Hash256> topics;
        topics.reserve(encodedTopics->size());
        for(const auto& topic : *encodedTopics)
        {
            topics.push_back(ParseHash(Json{{"topic", topic}}, "topic"));
        }

        Bytes data;
        try
        {
            data = Hex::Decode(RequireString(encodedLog, "data"));
        }
        catch(const RpcException&)
        {
            throw;
        }
        catch(const std::exception& exception)
        {
            throw RpcException(
                RpcErrorKind::InvalidResponse,
                "invalid RPC log data: " + std::string{exception.what()}
            );
        }

        const auto removedIterator = encodedLog.find("removed");
        if(removedIterator == encodedLog.end() || !removedIterator->is_boolean())
        {
            throw RpcException(RpcErrorKind::InvalidResponse, "RPC log removed flag is missing or invalid");
        }

        logs.push_back(RpcLog{
            ParseAddress(encodedLog, "address"),
            std::move(topics),
            std::move(data),
            ParseQuantity(encodedLog, "blockNumber"),
            ParseHash(encodedLog, "blockHash"),
            ParseHash(encodedLog, "transactionHash"),
            ParseQuantity(encodedLog, "transactionIndex"),
            ParseQuantity(encodedLog, "logIndex"),
            removedIterator->get<bool>()
        });
    }

    return logs;
}

Bytes RpcClient::EthCall(const TransactionCall& call, std::string_view block) const
{
    const auto result = implementation_->Call(
        "eth_call",
        Json::array({EncodeCall(call), std::string{block}})
    );
    return ParseBytesResult(result, "eth_call");
}

Uint256 RpcClient::EstimateGas(const TransactionCall& call) const
{
    const auto result = implementation_->Call("eth_estimateGas", Json::array({EncodeCall(call)}));
    return ParseQuantity(Json{{"result", result}}, "result");
}

Uint256 RpcClient::GetTransactionCount(const Address& address, std::string_view block) const
{
    const auto result = implementation_->Call(
        "eth_getTransactionCount",
        Json::array({address.ToHex(), std::string{block}})
    );
    return ParseQuantity(Json{{"result", result}}, "result");
}

Uint256 RpcClient::GetMaxPriorityFeePerGas() const
{
    const auto result = implementation_->Call("eth_maxPriorityFeePerGas", Json::array());
    return ParseQuantity(Json{{"result", result}}, "result");
}

Hash256 RpcClient::SendRawTransaction(const Bytes& rawTransaction) const
{
    const auto result = implementation_->Call(
        "eth_sendRawTransaction",
        Json::array({Hex::Encode(rawTransaction)})
    );
    return ParseHash(Json{{"result", result}}, "result");
}

std::optional<TransactionReceipt> RpcClient::GetTransactionReceipt(
    const Hash256& transactionHash
) const
{
    const auto result = implementation_->Call(
        "eth_getTransactionReceipt",
        Json::array({HashToHex(transactionHash)})
    );
    if(result.is_null())
    {
        return std::nullopt;
    }
    if(!result.is_object())
    {
        throw RpcException(RpcErrorKind::InvalidResponse, "eth_getTransactionReceipt result is not an object");
    }

    return TransactionReceipt{
        ParseHash(result, "transactionHash"),
        ParseQuantity(result, "blockNumber"),
        ParseHash(result, "blockHash"),
        !ParseQuantity(result, "status").IsZero(),
        ParseQuantity(result, "gasUsed"),
        ParseQuantity(result, "effectiveGasPrice")
    };
}

}
