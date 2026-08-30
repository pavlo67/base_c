#ifndef BASE_CPP_LIB_SERVER_H
#define BASE_CPP_LIB_SERVER_H

#include <cstdint>
#include <functional>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>

enum class HTTP_METHOD {
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

std::string ipV4ToString(uint32_t ipV4Host);

using ServerHTTPHandler = std::function<void(const ServerRequest&, ServerResponse&)>;
using ServerWebSocketHandler = std::function<void(const std::string& message, std::string& response)>;


struct HTTPRoute {
    HTTP_METHOD method;
    std::string        route;
    ServerHTTPHandler  callback;
};

struct WebSocketRoute {
    std::string            route;
    ServerWebSocketHandler callback;
};

#endif // BASE_CPP_LIB_SERVER_H
