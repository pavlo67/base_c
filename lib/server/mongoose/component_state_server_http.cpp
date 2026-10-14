#include "component_state_server_http.h"

#include <cstdio>
#include <utility>

#include "lib/server/server.h"

namespace {
    constexpr const char* ON_LOAD = "[ComponentStateServerHTTP.load()]";
}

ComponentStateServerHTTP::ComponentStateServerHTTP(std::string section) : section_(std::move(section)) {}

bool ComponentStateServerHTTP::load(const Config& cfg) {
    if (!cfg.loadedOk()) {
        printf("%s ERROR: configuration is unavailable\n", ON_LOAD);
        return false;
    }

    std::string host;
    uint16_t port = 0;
    try {
        const auto node = cfg.get(section_);
        if (!node.IsDefined() || !node.IsMap()) {
            printf("%s ERROR: %s section is missing or is not a map\n", ON_LOAD, section_.c_str());
            return false;
        }
        if (!node["host"] || !node["port"]) {
            printf("%s ERROR: %s.host and %s.port are required\n", ON_LOAD, section_.c_str(), section_.c_str());
            return false;
        }
        host = node["host"].as<std::string>();
        port = node["port"].as<uint16_t>();
    } catch (const YAML::Exception& error) {
        printf("%s ERROR: %s\n", ON_LOAD, error.what());
        return false;
    }

    uint32_t hostValue = 0;
    if (!ipV4StringToHost(host, hostValue)) {
        printf("%s ERROR: %s.host must be a valid IPv4 address\n", ON_LOAD, section_.c_str());
        return false;
    }
    if (port == 0) {
        printf("%s ERROR: %s.port must be in 1..65535\n", ON_LOAD, section_.c_str());
        return false;
    }

    {
        std::lock_guard lock(mutex_);
        host_ = std::move(host);
        port_ = port;
        loaded_ = true;
    }
    printComponentState(section_, get());
    return true;
}

config_values_t ComponentStateServerHTTP::get() const {
    std::lock_guard lock(mutex_);
    if (!loaded_) {
        return {};
    }
    return {{"host", host_}, {"port", std::to_string(port_)}};
}

void ComponentStateServerHTTP::getServerConfig(uint32_t& host, uint16_t& port) const {
    std::lock_guard lock(mutex_);
    if (!loaded_ || !ipV4StringToHost(host_, host)) {
        host = 0;
        port = 0;
        return;
    }
    port = port_;
}
