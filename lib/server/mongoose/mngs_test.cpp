#include "lib/server/mongoose/mngs.h"

#include <arpa/inet.h>
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

} // namespace

TEST(Server, HTTPAndWebSocket) {
    addServerHTTPHandler(HTTP_METHOD::GET, HTTP_PATH, [](const ServerRequest&, ServerResponse& response) {
        response.body = "server response: HTTP_TEST_KEY\n";
    });

    addServerWebSocketHandler(WS_PATH, [](const std::string& message, std::string& response) {
        response = "server response: " + message;
    });

    startServer(ntohl(inet_addr(HOST.c_str())), PORT);

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
    stopServer();
}
