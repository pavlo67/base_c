#include "lib/http/http.h"

#include <arpa/inet.h>

constexpr std::string HOST = "127.0.0.1";
constexpr uint16_t    PORT = 8080;
constexpr std::string PATH = "/hello";

int main() {
    addHandler(HTTP_METHOD::GET, PATH, [](const httplib::Request&, httplib::Response& response) {
        response.set_content("Hello from cpp-httplib server\n", "text/plain");
    });

    startServerHTTP(ntohl(inet_addr(HOST.c_str())), PORT);

    printf("\nhello page is active on http://%s:%d%s\npress Enter to stop example server...\n", HOST.c_str(), PORT, PATH.c_str());
    getchar();
    stopServerHTTP();
    return 0;
}
