#include "ipv4.h"
#include <charconv>
#include <cstdio>
#include <utility>

#if BASE_USE_GETIFADDRS
    #include <cerrno>
    #include <cstring>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netinet/in.h>
#elif BASE_USE_ADAPTER_ADDRESSES
    #include <winsock2.h>
    #include <iphlpapi.h>
#endif

bool isIPv4Address(std::string_view address) {
    for (unsigned part = 0; part < 4; ++part) {
        const auto dot = address.find('.');
        const auto token = address.substr(0, dot);
        unsigned value = 0;
        if (token.empty() || token.size() > 3 || (token.size() > 1 && token.front() == '0')) { return false; }
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || value > 255) { return false; }
        if (part == 3) { return dot == std::string_view::npos; }
        if (dot == std::string_view::npos) { return false; }
        address.remove_prefix(dot + 1);
    }
    return false;
}

namespace {
    #if BASE_USE_GETIFADDRS || BASE_USE_ADAPTER_ADDRESSES
        std::string formatIPv4Address(const sockaddr_in& address) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(&address.sin_addr);
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
                unsigned(bytes[0]), unsigned(bytes[1]), unsigned(bytes[2]), unsigned(bytes[3]));
            return buffer;
        }
    #endif
}

constexpr const char* ON_LOAD_LOCAL_IPV4_ADDRESSES = "[loadLocalIPv4Addresses()]";
bool loadLocalIPv4Addresses(std::vector<std::string>& addresses) {
    std::vector<std::string> candidate;
    #if BASE_USE_GETIFADDRS
        ifaddrs* interfaces = nullptr;
        if (getifaddrs(&interfaces) != 0) {
            printf("%s ERROR: getifaddrs failed: %s\n", ON_LOAD_LOCAL_IPV4_ADDRESSES, std::strerror(errno));
            return false;
        }
        for (auto* item = interfaces; item != nullptr; item = item->ifa_next) {
            if (item->ifa_addr == nullptr || item->ifa_addr->sa_family != AF_INET || !(item->ifa_flags & IFF_UP)) { continue; }
            candidate.push_back(formatIPv4Address(*reinterpret_cast<const sockaddr_in*>(item->ifa_addr)));
        }
        freeifaddrs(interfaces);
    #elif BASE_USE_ADAPTER_ADDRESSES
        ULONG size = 15000;
        std::vector<unsigned char> buffer(size);
        ULONG result = ERROR_BUFFER_OVERFLOW;
        for (unsigned attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt) {
            buffer.resize(size);
            result = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                GAA_FLAG_SKIP_DNS_SERVER, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()), &size);
        }
        if (result != NO_ERROR) {
            printf("%s ERROR: GetAdaptersAddresses failed: %lu\n", ON_LOAD_LOCAL_IPV4_ADDRESSES, result);
            return false;
        }
        for (auto* item = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); item != nullptr; item = item->Next) {
            if (item->OperStatus != IfOperStatusUp) { continue; }
            for (auto* address = item->FirstUnicastAddress; address != nullptr; address = address->Next) {
                if (address->Address.lpSockaddr == nullptr || address->Address.lpSockaddr->sa_family != AF_INET) { continue; }
                candidate.push_back(formatIPv4Address(*reinterpret_cast<const sockaddr_in*>(address->Address.lpSockaddr)));
            }
        }
    #else
        printf("%s ERROR: no interface enumeration backend configured\n", ON_LOAD_LOCAL_IPV4_ADDRESSES);
        return false;
    #endif
    addresses = std::move(candidate);
    return true;
}
