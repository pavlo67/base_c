#if (defined(SYSTEM_IS_DESKTOP) && SYSTEM_IS_DESKTOP) || defined(GPIO_DESKTOP_TEST)

#include "../gpio.h"

class DesktopGpio final : public Gpio {
public:
    int hardwarePwmChannel(unsigned pin) const override { return gpioHardwarePwmChannel(pin, GpioPwmLayout::rpi4); }
    ~DesktopGpio() override { terminate(); }
protected:
    std::array<unsigned, PIN_COUNT> levels_{};
    int open() override { levels_ = {}; return 0; }
    int close() override { return 0; }
    int mode(unsigned pin, GpioMode) override { levels_[pin] = 0; return 0; }
    int readLevel(unsigned pin) override { return static_cast<int>(levels_[pin]); }
    int writeLevel(unsigned pin, unsigned level) override { levels_[pin] = level; return 0; }
    int hardwarePwm(unsigned pin, const PwmSettings& settings) override { return pwm(pin, settings); }
    int pwm(unsigned pin, const PwmSettings& settings) override {
        // No waveform simulation: only 100% duty is represented as HIGH.
        levels_[pin] = settings.enabled && settings.duty == settings.range ? 1 : 0;
        return 0;
    }
};

Gpio& Gpio::instance() {
    static DesktopGpio gpio;
    return gpio;
}

#endif // DESKTOP
