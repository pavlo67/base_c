#include "server.h"

std::string ipV4ToString(uint32_t ipV4Host) {
    in_addr address{};
    address.s_addr = htonl(ipV4Host);

    char buffer[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &address, buffer, sizeof(buffer)) == nullptr) {
        return {};
    }

    return buffer;
}

bool ipV4StringToHost(const std::string& value, uint32_t& ipV4Host) {
    in_addr address{};
    if (inet_pton(AF_INET, value.c_str(), &address) != 1) {
        return false;
    }
    ipV4Host = ntohl(address.s_addr);
    return true;
}
