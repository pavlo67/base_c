#include "lib/server/mongoose/mngs.h"
#include "lib/server/mongoose/component_state_server_http.h"
#include "lib/gtest_stdout.h"

#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>
#include <mongoose.h>

namespace {

    constexpr std::string HOST = "127.0.0.1";
    constexpr uint16_t PORT = 18081;
    constexpr std::string HTTP_PATH = "/test";
    constexpr std::string WS_PATH = "/ws";

    struct ClientState {
        bool        done = false;
        bool        failed = false;
        bool        sendHTTPRequest = false;
        int         status = 0;
        std::string body;
    };

    void clientHandler(mg_connection* connection, int event, void* eventData) {
        auto* state = static_cast<ClientState*>(connection->fn_data);
        if (state == nullptr) {
            return;
        }

        if (event == MG_EV_CONNECT && state->sendHTTPRequest) {
            mg_printf(connection, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
                      HTTP_PATH.c_str(), HOST.c_str());
            return;
        }

        if (event == MG_EV_ERROR) {
            state->failed = true;
            state->done = true;
            return;
        }

        if (event == MG_EV_HTTP_MSG) {
            auto* message = static_cast<mg_http_message*>(eventData);
            state->status = mg_http_status(message);
            state->body.assign(message->body.buf, message->body.len);
            state->done = true;
            connection->is_closing = 1;
            return;
        }

        if (event == MG_EV_WS_OPEN) {
            static constexpr char REQUEST[] = "WS_TEST_KEY";
            mg_ws_send(connection, REQUEST, sizeof(REQUEST) - 1, WEBSOCKET_OP_TEXT);
            return;
        }

        if (event == MG_EV_WS_MSG) {
            auto* message = static_cast<mg_ws_message*>(eventData);
            state->body.assign(message->data.buf, message->data.len);
            state->done = true;
            connection->is_closing = 1;
        }
    }

    bool pollUntilDone(mg_mgr& manager, ClientState& state) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!state.done && std::chrono::steady_clock::now() < deadline) {
            mg_mgr_poll(&manager, 10);
        }
        return state.done && !state.failed;
    }

    struct ServerGuard {
        ~ServerGuard() {
            stopServer();
            waitForServerStopped();
        }
    };

} // namespace

TEST(Server, HTTPAndWebSocket) {
    addServerHTTPHandler(HTTP_METHOD::GET, HTTP_PATH, [](const ServerRequest&, ServerResponse& response) {
        response.body = "server response: HTTP_TEST_KEY\n";
    });

    addServerWebSocketHandler(WS_PATH, [](const std::string& message, std::string& response) {
        response = "server response: " + message;
    });

    const bool started = startServer(ntohl(inet_addr(HOST.c_str())), PORT);
    ServerGuard serverGuard;
    ASSERT_TRUE(started);

    mg_mgr manager;
    mg_mgr_init(&manager);

    ClientState httpState;
    httpState.sendHTTPRequest = true;
    const std::string httpUrl = "http://" + HOST + ":" + std::to_string(PORT) + HTTP_PATH;
    mg_connection* httpConnection = mg_http_connect(&manager, httpUrl.c_str(), clientHandler, &httpState);
    ASSERT_NE(httpConnection, nullptr);
    httpConnection->fn_data = &httpState;

    ASSERT_TRUE(pollUntilDone(manager, httpState));
    EXPECT_EQ(httpState.status, 200);
    EXPECT_NE(httpState.body.find("HTTP_TEST_KEY"), std::string::npos);

    ClientState wsState;
    const std::string wsUrl = "ws://" + HOST + ":" + std::to_string(PORT) + WS_PATH;
    mg_connection* wsConnection = mg_ws_connect(&manager, wsUrl.c_str(), clientHandler, &wsState, nullptr);
    ASSERT_NE(wsConnection, nullptr);
    wsConnection->fn_data = &wsState;

    ASSERT_TRUE(pollUntilDone(manager, wsState));
    EXPECT_NE(wsState.body.find("WS_TEST_KEY"), std::string::npos);

    mg_mgr_free(&manager);
}

TEST(Server, LoadsHTTPConfiguration) {
    printf("[HTTP_CFG] Load a configured IPv4 listener endpoint\n");
    YAML::Node document;
    document["serverHTTP"]["host"] = "127.0.0.1";
    document["serverHTTP"]["port"] = 18082;
    const Config config(document);
    ComponentStateServerHTTP section("serverHTTP");

    ASSERT_TRUE(section.load(config));
    uint32_t host = 0;
    uint16_t port = 0;
    section.getServerConfig(host, port);
    ASSERT_EQ(host, 0x7f000001U);
    ASSERT_EQ(port, 18082);
    ASSERT_EQ(section.get().at("host"), "127.0.0.1");
    ASSERT_EQ(section.get().at("port"), "18082");
}

TEST(Server, RejectsInvalidHTTPConfiguration) {
    printf("[HTTP_CFG] Reject invalid listener address and port\n");
    ComponentStateServerHTTP section("serverHTTP");
    YAML::Node invalidHost;
    invalidHost["serverHTTP"]["host"] = "localhost";
    invalidHost["serverHTTP"]["port"] = 8080;

    YAML::Node invalidPort;
    invalidPort["serverHTTP"]["host"] = "0.0.0.0";
    invalidPort["serverHTTP"]["port"] = 0;
    const auto captured = test_support::captureStdout([&] {
        return std::array{
            section.load(Config(invalidHost)),
            section.load(Config(invalidPort))
        };
    });
    for (const bool result : captured.result_) {
        ASSERT_FALSE(result) << captured.output_;
    }
    ASSERT_NE(captured.output_.find("[ComponentStateServerHTTP.load()] ERROR: "), std::string::npos)
        << captured.output_;
}
