#ifndef BASE_CPP_MONG_H
#define BASE_CPP_MONG_H

#include "lib/server/server.h"

void startServer(uint32_t ipV4Host, uint16_t port);
void stopServer();
void addServerHTTPHandler(SERVER_HTTP_METHOD method, const std::string& route, ServerHTTPHandler callback);
void addServerWebSocketHandler(const std::string& route, ServerWebSocketHandler callback);

#endif // BASE_CPP_MONG_H
