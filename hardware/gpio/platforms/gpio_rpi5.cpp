#if defined(SYSTEM_IS_RPI5) && SYSTEM_IS_RPI5

#include "gpio.h"
#include "pwm_sysfs.h"
#include <lgpio.h>
#include <cstring>
#include <filesystem>
#include <charconv>

class Rpi5Gpio final : public Gpio {
public:
    int hardwarePwmChannel(unsigned pin) const override { return gpioHardwarePwmChannel(pin, GpioPwmLayout::rpi5); }
    ~Rpi5Gpio() override { terminate(); }
protected:
    Rpi5PwmSysfsIo pwmIo_;
    PwmSysfs hardwarePwm_{pwmIo_};
    std::array<bool, PIN_COUNT> hardwarePins_{};
    int handle_ = -1;
    std::array<bool, PIN_COUNT> claimed_{};
    int open() override {
        // gpiochip numbering changed between Pi 5 kernels; identify the RP1 by label.
        std::error_code error;
        std::filesystem::directory_iterator it("/dev", error), end;
        if (error) { return CHIP_NOT_FOUND; }
        int result = CHIP_NOT_FOUND;
        for (; it != end; it.increment(error)) {
            if (error) { return CHIP_NOT_FOUND; }
            const auto name = it->path().filename().string();
            if (name.compare(0, 8, "gpiochip") != 0) { continue; }
            int chip = -1;
            const auto parsed = std::from_chars(name.data() + 8, name.data() + name.size(), chip);
            if (parsed.ec != std::errc{} || parsed.ptr != name.data() + name.size() || chip < 0) { continue; }
            const int candidate = lgGpiochipOpen(chip);
            if (candidate < 0) { result = candidate; continue; }
            lgChipInfo_t info{};
            const int status = lgGpioGetChipInfo(candidate, &info);
            if (status >= 0 && std::strcmp(info.label, "pinctrl-rp1") == 0) {
                handle_ = candidate;
                claimed_ = {};
                return 0;
            }
            lgGpiochipClose(candidate);
        }
        return result;
    }
    int close() override {
        const int released = hardwarePwm_.close();
        if (released < 0) { return released; }
        hardwarePins_ = {};
        const int result = lgGpiochipClose(handle_);
        if (result >= 0) { handle_ = -1; claimed_ = {}; }
        return result;
    }
    int mode(unsigned pin, GpioMode requested) override {
        if (hardwarePins_[pin]) {
            const int result = hardwarePwm_.release(hardwarePwmChannel(pin));
            if (result < 0) { return result; }
            hardwarePins_[pin] = false;
        }
        if (claimed_[pin]) {
            const int result = lgGpioFree(handle_, pin);
            if (result < 0) { return result; }
            claimed_[pin] = false;
        }
        if (requested == GpioMode::hardwarePwm) {
            const int result = hardwarePwm_.claim(pin, hardwarePwmChannel(pin));
            if (result >= 0) { hardwarePins_[pin] = true; }
            return result;
        }
        const int result = requested == GpioMode::output
            ? lgGpioClaimOutput(handle_, 0, pin, 0) : lgGpioClaimInput(handle_, 0, pin);
        if (result >= 0) { claimed_[pin] = true; }
        return result;
    }
    int readLevel(unsigned pin) override { return lgGpioRead(handle_, pin); }
    int writeLevel(unsigned pin, unsigned level) override { return lgGpioWrite(handle_, pin, level); }
    int hardwarePwm(unsigned pin, const PwmSettings& settings) override {
        return hardwarePwm_.apply(hardwarePwmChannel(pin), settings);
    }
    int pwm(unsigned pin, const PwmSettings& settings) override {
        if (!settings.enabled || settings.duty == 0 || settings.duty == settings.range) {
            const int busy = lgTxBusy(handle_, pin, LG_TX_PWM);
            if (busy < 0) { return busy; }
            if (busy != 0) {
                const int result = lgTxPwm(handle_, pin, 0, 0, 0, 0);
                if (result < 0) { return result; }
            }
            return lgGpioWrite(handle_, pin, settings.enabled && settings.duty == settings.range ? 1 : 0);
        }
        const int result = lgTxPwm(handle_, pin, settings.frequency,
                                  100.0 * settings.duty / settings.range, 0, 0);
        return result < 0 ? result : 0;
    }
};

Gpio& Gpio::instance() {
    static Rpi5Gpio gpio;
    return gpio;
}

#endif // RPI5
