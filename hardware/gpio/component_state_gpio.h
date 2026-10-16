#ifndef BASE_HARDWARE_GPIO_COMPONENT_STATE_GPIO_H
#define BASE_HARDWARE_GPIO_COMPONENT_STATE_GPIO_H

#include <map>
#include <mutex>
#include <set>
#include <string>

#include "gpio.h"
#include "lib/config/config_component.h"

class ComponentStateGPIO : public ComponentState {
public:
    ComponentStateGPIO(std::string section, std::set<unsigned> reservedPins);

    virtual bool load(const Config& cfg);
    [[nodiscard]] config_values_t get() const final;
    void test() final;

    int setPinMode(const std::string& name, GpioMode mode);
    int writePin(const std::string& name, unsigned level);
    int readPin(const std::string& name);
    int getPin(const std::string& name, unsigned& pin) const;
    [[nodiscard]] bool usesPin(unsigned pin) const;

protected:
    mutable std::mutex mutex_;
    std::string section_;

private:
    std::map<std::string, unsigned> pins_;
    std::set<unsigned> reservedPins_;
    bool loaded_ = false;
};

#endif // BASE_HARDWARE_GPIO_COMPONENT_STATE_GPIO_H
