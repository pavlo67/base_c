#if defined(SYSTEM_IS_RPI4) && SYSTEM_IS_RPI4

#include "gpio.h"
#include <pigpio.h>
#include <cstdint>

class Rpi4Gpio final : public Gpio {
public:
    int hardwarePwmChannel(unsigned pin) const override { return gpioHardwarePwmChannel(pin, GpioPwmLayout::rpi4); }
    ~Rpi4Gpio() override { terminate(); }
protected:
    int open() override { return gpioInitialise(); }
    std::array<bool, PIN_COUNT> hardwarePins_{};
    int close() override {
        int result = 0;
        for (unsigned pin = 0; pin < PIN_COUNT; ++pin) {
            if (hardwarePins_[pin]) {
                const int stopped = gpioHardwarePWM(pin, 0, 0);
                if (stopped < 0 && result == 0) { result = stopped; }
            }
        }
        if (result < 0) { return result; }
        gpioTerminate();
        hardwarePins_ = {};
        return result;
    }
    int mode(unsigned pin, GpioMode requested) override {
        if (hardwarePins_[pin]) {
            const int result = gpioHardwarePWM(pin, 0, 0);
            if (result < 0) { return result; }
            hardwarePins_[pin] = false;
        }
        if (requested == GpioMode::hardwarePwm) {
            const int result = gpioHardwarePWM(pin, 1000, 0);
            if (result >= 0) { hardwarePins_[pin] = true; }
            return result;
        }
        if (requested == GpioMode::output) {
            const int result = gpioWrite(pin, 0);
            if (result < 0) { return result; }
        }
        return gpioSetMode(pin, requested == GpioMode::output ? PI_OUTPUT : PI_INPUT);
    }
    int readLevel(unsigned pin) override { return gpioRead(pin); }
    int writeLevel(unsigned pin, unsigned level) override { return gpioWrite(pin, level); }
    int hardwarePwm(unsigned pin, const PwmSettings& settings) override {
        const unsigned duty = settings.enabled
            ? static_cast<unsigned>(uint64_t(settings.duty) * 1000000 / settings.range) : 0;
        return gpioHardwarePWM(pin, settings.frequency, duty);
    }
    int pwm(unsigned pin, const PwmSettings& settings) override {
        if (!settings.enabled) { return gpioWrite(pin, 0); }
        // A fixed native scale also permits public ranges below pigpio's minimum of 25.
        int result = gpioSetPWMrange(pin, 40000);
        if (result < 0) { return result; }
        result = gpioSetPWMfrequency(pin, settings.frequency);
        if (result < 0) { return result; }
        const auto duty = static_cast<unsigned>(uint64_t(settings.duty) * 40000 / settings.range);
        return gpioPWM(pin, duty);
    }
};

Gpio& Gpio::instance() {
    static Rpi4Gpio gpio;
    return gpio;
}

#endif // RPI4
