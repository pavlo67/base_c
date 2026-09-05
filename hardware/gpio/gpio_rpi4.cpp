#if defined(SYSTEM_IS_RPI4) && SYSTEM_IS_RPI4

#include "gpio.h"
#include <pigpio.h>
#include <cstdint>

class Rpi4Gpio final : public Gpio {
public:
    ~Rpi4Gpio() override { terminate(); }
protected:
    int open() override { return gpioInitialise(); }
    int close() override { gpioTerminate(); return 0; }
    int mode(unsigned pin, GpioMode requested) override {
        if (requested == GpioMode::output) {
            const int result = gpioWrite(pin, 0);
            if (result < 0) { return result; }
        }
        return gpioSetMode(pin, requested == GpioMode::output ? PI_OUTPUT : PI_INPUT);
    }
    int readLevel(unsigned pin) override { return gpioRead(pin); }
    int writeLevel(unsigned pin, unsigned level) override { return gpioWrite(pin, level); }
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
