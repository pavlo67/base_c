#include "lib/http/http.h"

#include <arpa/inet.h>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

namespace {

constexpr std::string HOST = "127.0.0.1";
constexpr uint16_t PORT = 18080;
constexpr std::string PATH = "/test";

}

TEST(HTTP, ServerReturnsExpectedContent) {
    const std::string expected = "HTTP_TEST_KEY";

    addHandler(HTTP_METHOD::GET, PATH, [&expected](const httplib::Request&, httplib::Response& response) {
        response.set_content("server response: " + expected + "\n", "text/plain");
    });

    startServerHTTP(ntohl(inet_addr(HOST.c_str())), PORT);

    httplib::Client client(HOST, PORT);
    const auto result = client.Get(PATH);

    stopServerHTTP();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 200);
    EXPECT_NE(result->body.find(expected), std::string::npos);
}
