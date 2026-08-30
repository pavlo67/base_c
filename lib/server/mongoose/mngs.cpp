#include "mngs.h"

#include <arpa/inet.h>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <mongoose.h>

namespace {
    std::vector<HTTPRoute>      httpRoutes;
    std::vector<WebSocketRoute> webSocketRoutes;
    std::thread                 serverThread;
    std::mutex                  serverMutex;
    std::condition_variable     serverStartedCondition;
    bool                        serverStarted = false;
    bool                        serverStartFailed = false;
    bool                        serverStopRequested = false;

    bool sameMethod(HTTP_METHOD method, const mg_str& actualMethod) {
        const char* expected = nullptr;

        switch (method) {
            case HTTP_METHOD::GET:     expected = "GET"; break;
            case HTTP_METHOD::POST:    expected = "POST"; break;
            case HTTP_METHOD::PUT:     expected = "PUT"; break;
            case HTTP_METHOD::DELETE:  expected = "DELETE"; break;
            case HTTP_METHOD::PATCH:   expected = "PATCH"; break;
            case HTTP_METHOD::OPTIONS: expected = "OPTIONS"; break;
            case HTTP_METHOD::HEAD:    expected = "HEAD"; break;
        }

        return expected != nullptr && mg_strcmp(actualMethod, mg_str(expected)) == 0;
    }

    bool sameRoute(const mg_str& actualUri, const std::string& route) {
        return actualUri.len == route.size() && std::string(actualUri.buf, actualUri.len) == route;
    }

    void handleHTTP(mg_connection* connection, mg_http_message* message) {
        for (WebSocketRoute& route : webSocketRoutes) {
            if (sameRoute(message->uri, route.route)) {
                mg_ws_upgrade(connection, message, nullptr);
                connection->fn_data = &route;
                return;
            }
        }

        for (const HTTPRoute& route : httpRoutes) {
            if (!sameMethod(route.method, message->method) || !sameRoute(message->uri, route.route)) {
                continue;
            }

            ServerRequest request;
            request.method.assign(message->method.buf, message->method.len);
            request.uri.assign(message->uri.buf, message->uri.len);
            request.body.assign(message->body.buf, message->body.len);

            ServerResponse response;
            route.callback(request, response);

            const std::string headers = "Content-Type: " + response.contentType + "\r\n";
            mg_http_reply(connection, response.status, headers.c_str(), "%.*s",
                          static_cast<int>(response.body.size()), response.body.data());
            return;
        }

        mg_http_reply(connection, 404, "Content-Type: text/plain\r\n", "Not found\n");
    }

    void eventHandler(mg_connection* connection, int event, void* eventData) {
        if (event == MG_EV_HTTP_MSG) {
            handleHTTP(connection, static_cast<mg_http_message*>(eventData));
            return;
        }

        if (event == MG_EV_WS_MSG) {
            auto* route = static_cast<WebSocketRoute*>(connection->fn_data);
            if (route == nullptr) {
                return;
            }

            auto* message = static_cast<mg_ws_message*>(eventData);
            std::string request(message->data.buf, message->data.len);
            std::string response;
            route->callback(request, response);

            if (!response.empty()) {
                mg_ws_send(connection, response.data(), response.size(), WEBSOCKET_OP_TEXT);
            }
        }
    }

} // namespace

const std::string ON_START_SERVER = "on startServer(): ";

bool startServer(uint32_t ipV4Host, uint16_t port) {
    std::unique_lock<std::mutex> lock(serverMutex);

    if (serverThread.joinable()) {
        fprintf(stderr, "%sserver is already running\n", ON_START_SERVER.c_str());
        return false;
    }

    const std::string host = ipV4ToString(ipV4Host);
    if (host.empty()) {
        fprintf(stderr, "%scan't convert IPv4 address\n", ON_START_SERVER.c_str());
        return false;
    }

    serverStarted = false;
    serverStartFailed = false;
    serverStopRequested = false;

    serverThread = std::thread([host, port]() {
        printf("STARTING HTTP/WS SERVER ON %s:%d...\n", host.c_str(), port);

        mg_mgr manager;
        mg_mgr_init(&manager);
        const std::string address = "http://" + host + ":" + std::to_string(port);
        mg_connection* listener = mg_http_listen(&manager, address.c_str(), eventHandler, nullptr);
        {
            std::lock_guard<std::mutex> threadLock(serverMutex);
            serverStartFailed = listener == nullptr;
            serverStarted = listener != nullptr;
        }
        serverStartedCondition.notify_one();

        if (listener == nullptr) {
            mg_mgr_free(&manager);
            return;
        }

        while (true) {
            {
                std::lock_guard<std::mutex> threadLock(serverMutex);
                if (serverStopRequested) {
                    break;
                }
            }
            mg_mgr_poll(&manager, 1000);
        }

        mg_mgr_free(&manager);
    });

    serverStartedCondition.wait(lock, []() {
        return serverStarted || serverStartFailed;
    });

    if (serverStartFailed) {
        std::thread failedThread = std::move(serverThread);
        lock.unlock();
        failedThread.join();
        fprintf(stderr, "%scan't bind to %s:%u\n", ON_START_SERVER.c_str(), host.c_str(), static_cast<unsigned>(port));
        return false;
    }

    fprintf(stderr, "started on http://%s:%u\n", host.c_str(), static_cast<unsigned>(port));
    return true;
}

void stopServer() {
    std::thread threadToJoin;

    {
        std::lock_guard<std::mutex> lock(serverMutex);
        if (!serverThread.joinable()) {
            return;
        }

        serverStopRequested = true;
        threadToJoin = std::move(serverThread);
    }

    threadToJoin.join();

    std::lock_guard<std::mutex> lock(serverMutex);
    serverStarted = false;
    serverStartFailed = false;
    serverStopRequested = false;
}

const std::string ON_ADD_SERVER_HTTP_HANDLER = "on addServerHTTPHandler(): ";

void addServerHTTPHandler(HTTP_METHOD method, const std::string& route, ServerHTTPHandler callback) {
    std::lock_guard<std::mutex> lock(serverMutex);
    if (serverThread.joinable()) {
        fprintf(stderr, "%shandlers must be added before server start\n", ON_ADD_SERVER_HTTP_HANDLER.c_str());
        return;
    }
    httpRoutes.push_back({method, route, std::move(callback)});
    printf("added HTTP handler: %d %s\n", method, route.c_str());
}

const std::string ON_ADD_SERVER_WEBSOCKET_HANDLER = "on addServerWebSocketHandler(): ";

void addServerWebSocketHandler(const std::string& route, ServerWebSocketHandler callback) {
    std::lock_guard<std::mutex> lock(serverMutex);
    if (serverThread.joinable()) {
        fprintf(stderr, "%shandlers must be added before server start\n", ON_ADD_SERVER_WEBSOCKET_HANDLER.c_str());
        return;
    }
    webSocketRoutes.push_back({route, std::move(callback)});
    printf("added WS handler: %s\n", route.c_str());
}
