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
