#ifndef BASE_MONGOOSE_COMPONENT_STATE_SERVER_HTTP_H
#define BASE_MONGOOSE_COMPONENT_STATE_SERVER_HTTP_H

#include <cstdint>
#include <mutex>
#include <string>

#include "lib/config/config_component.h"

class ComponentStateServerHTTP final : public ComponentState {
public:
    explicit ComponentStateServerHTTP(std::string section);

    bool load(const Config& cfg) final;
    [[nodiscard]] config_values_t get() const final;
    void test() final;

    void getServerConfig(uint32_t& host, uint16_t& port) const;

private:
    mutable std::mutex mutex_;
    std::string section_;
    std::string host_;
    uint16_t port_ = 0;
    bool loaded_ = false;
};

#endif // BASE_MONGOOSE_COMPONENT_STATE_SERVER_HTTP_H
