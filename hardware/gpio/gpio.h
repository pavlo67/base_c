#pragma once

#include <array>

enum class GpioMode { input, output };

struct PwmSettings {
    unsigned range = 100;
    unsigned duty = 0;
    unsigned frequency = 1000; // Requested Hz; backend may quantize.
    bool enabled = false;
};

class Gpio {
public:
    static constexpr unsigned PIN_COUNT = 28; // BCM GPIO0..27, not header positions.
    static constexpr int INVALID_ARGUMENT = -10000;
    static constexpr int NOT_INITIALIZED = -10001;
    static constexpr int WRONG_MODE = -10002;
    static constexpr int PWM_ACTIVE = -10003;
    static constexpr int CHIP_NOT_FOUND = -10004;

    virtual ~Gpio() = default;
    Gpio(const Gpio&) = delete;
    Gpio& operator=(const Gpio&) = delete;
    static Gpio& instance();

    int initialize();
    int terminate();
    int setMode(unsigned pin, GpioMode mode);
    int read(unsigned pin);
    int write(unsigned pin, unsigned level);
    int setRange(unsigned pin, unsigned range);
    int setDuty(unsigned pin, unsigned duty);
    int setFrequency(unsigned pin, unsigned frequency);
    int setEnabled(unsigned pin, bool enabled);
    int getPwmSettings(unsigned pin, PwmSettings& settings) const;

protected:
    Gpio() = default;
    virtual int open() = 0;
    virtual int close() = 0;
    virtual int mode(unsigned pin, GpioMode mode) = 0;
    virtual int readLevel(unsigned pin) = 0;
    virtual int writeLevel(unsigned pin, unsigned level) = 0;
    virtual int pwm(unsigned pin, const PwmSettings& settings) = 0;

private:
    struct PinState {
        bool configured = false;
        GpioMode mode = GpioMode::input;
        PwmSettings pwm;
    };
    bool initialized_ = false;
    std::array<PinState, PIN_COUNT> pins_{};
    int check(unsigned pin, bool output = false) const;
    int update(unsigned pin, const PwmSettings& settings);
};

