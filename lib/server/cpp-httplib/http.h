#ifndef BASE_CPP_HTTP_H
#define BASE_CPP_HTTP_H

#include <cstdint>
#include <functional>
#include <string>

#include <httplib.h>

#include "../server.h"

using HTTPHandler = std::function<void(const httplib::Request&, httplib::Response&)>;

void startServerHTTP(uint32_t ipV4Host, uint16_t port);
void stopServerHTTP();
void addHandler(HTTP_METHOD method, const std::string& route, HTTPHandler callback);

#endif // BASE_CPP_HTTP_H
