#include "lib/server/mongoose/mngs.h"

#include <arpa/inet.h>
#include <cstdio>
#include <string>

constexpr std::string HOST = "127.0.0.1";
constexpr uint16_t    PORT = 8080;
constexpr std::string HTTP_PATH = "/hello";
constexpr std::string WS_PATH = "/ws";

int main() {
    addServerHTTPHandler(HTTP_METHOD::GET, HTTP_PATH, [](const ServerRequest&, ServerResponse& response) {
        response.body = "Hello from Mongoose server\n";
    });

    addServerWebSocketHandler(WS_PATH, [](const std::string& message, std::string& response) {
        response = "server received: " + message;
    });

    if (!startServer(ntohl(inet_addr(HOST.c_str())), PORT)) {
        return EXIT_FAILURE;
    }

    waitForServerStopped();
    return EXIT_SUCCESS;
}
