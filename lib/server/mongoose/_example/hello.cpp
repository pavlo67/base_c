#include "lib/server/mongoose/mong.h"

#include <arpa/inet.h>
#include <cstdio>
#include <string>

constexpr std::string HOST = "127.0.0.1";
constexpr uint16_t    PORT = 8080;
constexpr std::string HTTP_PATH = "/hello";
constexpr std::string WS_PATH = "/ws";

int main() {
    addServerHTTPHandler(SERVER_HTTP_METHOD::GET, HTTP_PATH, [](const ServerRequest&, ServerResponse& response) {
        response.body = "Hello from Mongoose server\n";
    });

    addServerWebSocketHandler(WS_PATH, [](const std::string& message, std::string& response) {
        response = "server received: " + message;
    });

    startServer(ntohl(inet_addr(HOST.c_str())), PORT);

    printf("\nHTTP page: http://%s:%d%s\nWebSocket: ws://%s:%d%s\npress Enter to stop example server...\n",
           HOST.c_str(), PORT, HTTP_PATH.c_str(), HOST.c_str(), PORT, WS_PATH.c_str());
    getchar();
    stopServer();
    return 0;
}
