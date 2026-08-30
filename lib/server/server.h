#ifndef BASE_CPP_LIB_SERVER_H
#define BASE_CPP_LIB_SERVER_H

#include <cstdint>
#include <functional>
#include <string>

enum class SERVER_HTTP_METHOD {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    OPTIONS,
    HEAD
};

struct ServerRequest {
    std::string method;
    std::string uri;
    std::string body;
};

struct ServerResponse {
    int         status = 200;
    std::string contentType = "text/plain";
    std::string body;
};

using ServerHTTPHandler = std::function<void(const ServerRequest&, ServerResponse&)>;
using ServerWebSocketHandler = std::function<void(const std::string& message, std::string& response)>;

#endif // BASE_CPP_LIB_SERVER_H
