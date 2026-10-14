#include "component_state_gpio.h"

#include <cstdio>
#include <set>
#include <utility>

namespace {
    constexpr const char* ON_LOAD = "[ComponentStateGPIO.load()]";
    constexpr const char* ON_SET_PIN_MODE = "[ComponentStateGPIO.setPinMode()]";
    constexpr const char* ON_WRITE_PIN = "[ComponentStateGPIO.writePin()]";
    constexpr const char* ON_READ_PIN = "[ComponentStateGPIO.readPin()]";
}

ComponentStateGPIO::ComponentStateGPIO(std::string section, std::set<unsigned> reservedPins)
    : section_(std::move(section)), reservedPins_(std::move(reservedPins)) {}

bool ComponentStateGPIO::load(const Config& cfg) {
    if (!cfg.loadedOk()) {
        printf("%s ERROR: configuration is unavailable\n", ON_LOAD);
        return false;
    }
    std::map<std::string, unsigned> pins;
    std::set<unsigned> usedPins;
    std::set<unsigned> reservedPins;
    {
        std::lock_guard lock(mutex_);
        reservedPins = reservedPins_;
    }
    try {
        const auto node = cfg.get(section_);
        if (!node.IsDefined() || !node.IsMap()) {
            printf("%s ERROR: %s section is missing or is not a map\n", ON_LOAD, section_.c_str());
            return false;
        }
        for (const auto& entry : node) {
            const std::string name = entry.first.as<std::string>();
            const unsigned pin = entry.second.as<unsigned>();
            if (name.empty() || pin >= Gpio::PIN_COUNT || !usedPins.insert(pin).second) {
                printf("%s ERROR: %s must contain unique named BCM pins in 0..%u\n", ON_LOAD,
                    section_.c_str(), Gpio::PIN_COUNT - 1);
                return false;
            }
            if (reservedPins.contains(pin)) {
                printf("%s ERROR: %s pin %u conflicts with a stepper motor\n", ON_LOAD,
                    section_.c_str(), pin);
                return false;
            }
            pins.emplace(name, pin);
        }
    } catch (const YAML::Exception& error) {
        printf("%s ERROR: %s\n", ON_LOAD, error.what());
        return false;
    }
    {
        std::lock_guard lock(mutex_);
        pins_ = std::move(pins);
        loaded_ = true;
    }
    printComponentState(section_, get());
    return true;
}

config_values_t ComponentStateGPIO::get() const {
    std::lock_guard lock(mutex_);
    config_values_t values;
    if (!loaded_) {
        return values;
    }
    for (const auto& [name, pin] : pins_) {
        values[name] = std::to_string(pin);
    }
    return values;
}

void ComponentStateGPIO::test() {
    testSavedInterfaces();
}

int ComponentStateGPIO::getPin(const std::string& name, unsigned& pin) const {
    std::lock_guard lock(mutex_);
    const auto found = pins_.find(name);
    if (!loaded_ || found == pins_.end()) {
        return Gpio::INVALID_ARGUMENT;
    }
    pin = found->second;
    return Gpio::SUCCESS;
}

int ComponentStateGPIO::setPinMode(const std::string& name, GpioMode mode) {
    unsigned pin = 0;
    const int found = getPin(name, pin);
    if (found < 0) {
        printf("%s ERROR: pin %s is not configured\n", ON_SET_PIN_MODE, name.c_str());
        return found;
    }
    return Gpio::instance().setMode(pin, mode);
}

int ComponentStateGPIO::writePin(const std::string& name, unsigned level) {
    unsigned pin = 0;
    const int found = getPin(name, pin);
    if (found < 0) {
        printf("%s ERROR: pin %s is not configured\n", ON_WRITE_PIN, name.c_str());
        return found;
    }
    return Gpio::instance().write(pin, level);
}

int ComponentStateGPIO::readPin(const std::string& name) {
    unsigned pin = 0;
    const int found = getPin(name, pin);
    if (found < 0) {
        printf("%s ERROR: pin %s is not configured\n", ON_READ_PIN, name.c_str());
        return found;
    }
    return Gpio::instance().read(pin);
}

bool ComponentStateGPIO::usesPin(unsigned pin) const {
    std::lock_guard lock(mutex_);
    for (const auto& entry : pins_) {
        if (entry.second == pin) {
            return true;
        }
    }
    return false;
}

