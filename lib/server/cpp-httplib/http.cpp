#include "http.h"

#include <arpa/inet.h>
#include <cstdio>
#include <mutex>
#include <thread>
#include <utility>

namespace {
    httplib::Server server;
    std::thread     serverThread;
    std::mutex      serverMutex;
}

void startServerHTTP(uint32_t ipV4Host, uint16_t port) {
    std::lock_guard<std::mutex> lock(serverMutex);

    if (serverThread.joinable()) {
        fprintf(stderr, "on startServerHTTP(): server is already running\n");
        return;
    }

    const std::string host = ipV4ToString(ipV4Host);
    if (host.empty()) {
        return;
    }

    if (!server.bind_to_port(host, port)) {
        fprintf(stderr, "on startServerHTTP(): can't bind to %s:%u\n", host.c_str(), static_cast<unsigned>(port));
        return;
    }

    fprintf(stderr, "on startServerHTTP(): started on %s:%u\n", host.c_str(), static_cast<unsigned>(port));

    serverThread = std::thread([]() {
        if (!server.listen_after_bind()) {
            fprintf(stderr, "on startServerHTTP(): server listen failed\n");
        }
    });
}

void stopServerHTTP() {
    std::thread threadToJoin;

    {
        std::lock_guard<std::mutex> lock(serverMutex);
        if (!serverThread.joinable()) {
            return;
        }

        server.stop();
        threadToJoin = std::move(serverThread);
    }

    threadToJoin.join();
}

void addHandler(HTTP_METHOD method, const std::string& route, HTTPHandler callback) {
    switch (method) {
        case HTTP_METHOD::GET:
            server.Get(route, std::move(callback));
            break;
        case HTTP_METHOD::POST:
            server.Post(route, std::move(callback));
            break;
        case HTTP_METHOD::PUT:
            server.Put(route, std::move(callback));
            break;
        case HTTP_METHOD::DELETE:
            server.Delete(route, std::move(callback));
            break;
        case HTTP_METHOD::PATCH:
            server.Patch(route, std::move(callback));
            break;
        case HTTP_METHOD::OPTIONS:
            server.Options(route, std::move(callback));
            break;
        case HTTP_METHOD::HEAD:
            // cpp-httplib handles HEAD through the corresponding GET route.
            server.Get(route, std::move(callback));
            break;
    }
}
