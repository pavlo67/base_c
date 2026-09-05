#include "gpio.h"

int gpioHardwarePwmChannel(unsigned pin, GpioPwmLayout layout) {
    if (layout != GpioPwmLayout::rpi4 && layout != GpioPwmLayout::rpi5) { return Gpio::NOT_SUPPORTED; }
    if (pin == 12) { return 0; }
    if (pin == 13) { return 1; }
    if (layout == GpioPwmLayout::rpi4) {
        if (pin == 18) { return 0; }
        if (pin == 19) { return 1; }
    } else if (layout == GpioPwmLayout::rpi5) {
        if (pin == 14 || pin == 18) { return 2; }
        if (pin == 15 || pin == 19) { return 3; }
    }
    return Gpio::NOT_SUPPORTED;
}

int Gpio::initialize() {
    if (initialized_) { return 0; }
    const int result = open();
    if (result < 0) { return result; }
    pins_ = {};
    initialized_ = true;
    return 0;
}

int Gpio::terminate() {
    if (!initialized_) { return 0; }
    int result = 0;
    for (unsigned pin = 0; pin < PIN_COUNT; ++pin) {
        if (pins_[pin].pwm.enabled) {
            const int stopped = setEnabled(pin, false);
            if (stopped < 0 && result == 0) { result = stopped; }
        }
    }
    const int closed = close();
    if (closed < 0) { return closed; }
    initialized_ = false;
    pins_ = {};
    return result;
}

int Gpio::check(unsigned pin, bool output) const {
    if (!initialized_) { return NOT_INITIALIZED; }
    if (pin >= PIN_COUNT) { return INVALID_ARGUMENT; }
    if (!pins_[pin].configured || (output && pins_[pin].mode == GpioMode::input)) {
        return WRONG_MODE;
    }
    return 0;
}

int Gpio::setMode(unsigned pin, GpioMode requested) {
    if (!initialized_) { return NOT_INITIALIZED; }
    if (pin >= PIN_COUNT || (requested != GpioMode::input && requested != GpioMode::output && requested != GpioMode::hardwarePwm)) {
        return INVALID_ARGUMENT;
    }
    if (requested == GpioMode::hardwarePwm) {
        const int channel = hardwarePwmChannel(pin);
        if (channel < 0) { return channel; }
        for (unsigned other = 0; other < PIN_COUNT; ++other) {
            if (other != pin && pins_[other].configured && pins_[other].mode == GpioMode::hardwarePwm &&
                hardwarePwmChannel(other) == channel) { return CHANNEL_BUSY; }
        }
    }
    auto& state = pins_[pin];
    if (state.configured && state.mode == requested) { return 0; }
    if (state.pwm.enabled) {
        const int result = setEnabled(pin, false);
        if (result < 0) { return result; }
    }
    // A backend can release its old claim before failing to claim the new mode.
    state.configured = false;
    const int result = mode(pin, requested);
    if (result < 0) { return result; }
    state.configured = true;
    state.mode = requested;
    return 0;
}

int Gpio::read(unsigned pin) {
    const int result = check(pin);
    if (result < 0) { return result; }
    if (pins_[pin].mode == GpioMode::hardwarePwm) { return WRONG_MODE; }
    return readLevel(pin);
}

int Gpio::write(unsigned pin, unsigned level) {
    const int result = check(pin, true);
    if (result < 0) { return result; }
    if (pins_[pin].mode == GpioMode::hardwarePwm) { return WRONG_MODE; }
    if (level > 1) { return INVALID_ARGUMENT; }
    if (pins_[pin].pwm.enabled) { return PWM_ACTIVE; }
    return writeLevel(pin, level);
}

int Gpio::getPwmSettings(unsigned pin, PwmSettings& settings) const {
    const int result = check(pin, true);
    if (result < 0) { return result; }
    settings = pins_[pin].pwm;
    return 0;
}

int Gpio::update(unsigned pin, const PwmSettings& settings) {
    const int result = check(pin, true);
    if (result < 0) { return result; }
    if (settings.range == 0 || settings.range > 40000 || settings.duty > settings.range ||
        settings.frequency == 0 || settings.frequency > 10000) {
        return INVALID_ARGUMENT;
    }
    if (settings.enabled || pins_[pin].pwm.enabled || pins_[pin].mode == GpioMode::hardwarePwm) {
        const int applied = pins_[pin].mode == GpioMode::hardwarePwm ? hardwarePwm(pin, settings) : pwm(pin, settings);
        if (applied < 0) { return applied; }
    }
    pins_[pin].pwm = settings;
    return 0;
}

int Gpio::setRange(unsigned pin, unsigned range) {
    PwmSettings settings;
    const int result = getPwmSettings(pin, settings);
    if (result < 0) { return result; }
    settings.range = range;
    return update(pin, settings);
}

int Gpio::setDuty(unsigned pin, unsigned duty) {
    PwmSettings settings;
    const int result = getPwmSettings(pin, settings);
    if (result < 0) { return result; }
    settings.duty = duty;
    return update(pin, settings);
}

int Gpio::setFrequency(unsigned pin, unsigned frequency) {
    PwmSettings settings;
    const int result = getPwmSettings(pin, settings);
    if (result < 0) { return result; }
    settings.frequency = frequency;
    return update(pin, settings);
}

int Gpio::setEnabled(unsigned pin, bool enabled) {
    PwmSettings settings;
    const int result = getPwmSettings(pin, settings);
    if (result < 0) { return result; }
    settings.enabled = enabled;
    if (!enabled && !pins_[pin].pwm.enabled && pins_[pin].mode != GpioMode::hardwarePwm) { return writeLevel(pin, 0); }
    return update(pin, settings);
}

