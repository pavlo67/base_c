#include "hardware/stepper_motor/config/platform_config.h"
#include "hardware/gpio/gpio.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

constexpr const char* HARDWARE_CONFIG_PATH = "_env/machina.yaml";

constexpr float SPEED_DEG_S = 20;
constexpr int   STEP_LOW_US = 1e6 / (SPEED_DEG_S / 0.225) - 15;
constexpr int   BETWEEN_SERIES_MS = 10;

int movePulses(int64_t pulses, const StepperMotorRunConfig& pan);

int main(int argc, char** argv) {
    printf("PULSE DELAY IS %.3f ms\n", float(STEP_LOW_US) / 1e3);

    if (argc < 2) {
        printf("[main()] ERROR: supply signed pulse counts, e.g. pulses_probe +800 -1600 +800\n");
        return 1;
    }
    std::vector<int64_t> movements;
    movements.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) {
        std::string_view value(argv[i]);
        const bool explicitPlus = !value.empty() && value.front() == '+';
        if (explicitPlus) { value.remove_prefix(1); }
        int64_t pulses = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), pulses);
        if (value.empty() || (explicitPlus && value.front() == '-') ||
                parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                pulses == std::numeric_limits<int64_t>::min()) {
            printf("[main()] ERROR: argument %d must be a signed integer pulse count\n", i);
            return 1;
        }
        movements.push_back(pulses);
    }

    printf("[PUL] Load pan hardware from %s\n", HARDWARE_CONFIG_PATH);
    std::array<StepperMotorRunConfig, 2> motors;
    if (!loadPlatformMotorConfig(Config(HARDWARE_CONFIG_PATH), motors)) { return 1; }
    const auto& pan = motors[0];
    printf("[PUL] pan pins (BCM): PUL/STEP=%d DIR=%d ENA=%d\n",
        pan.pinStep_, pan.pinDir_, pan.pinEna_);
    fflush(stdout);
    auto& gpio = Gpio::instance();
    if (gpio.initialize() < 0) {
        printf("[main()] ERROR: GPIO initialization failed\n");
        return 1;
    }

    int result = 0;
    int code = gpio.setMode(pan.pinStep_, GpioMode::output);
    if (code >= 0) { code = gpio.setMode(pan.pinDir_, GpioMode::output); }
    if (code >= 0) { code = gpio.setMode(pan.pinEna_, GpioMode::output); }
    if (code >= 0) { code = gpio.write(pan.pinEna_, 0); }
    if (code < 0) {
        printf("[main()] ERROR: GPIO setup failed: %d\n", code);
        result = 1;
    } else {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        for (size_t i = 0; i < movements.size(); ++i) {
            printf("[PUL] Series %zu/%zu: %lld pulses\n", i + 1, movements.size(),
                static_cast<long long>(movements[i]));
            if (movePulses(movements[i], pan) < 0) { result = 1; break; }
            if (i + 1 < movements.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(BETWEEN_SERIES_MS));
            }
        }
    }
    if (gpio.write(pan.pinStep_, 0) < 0) { result = 1; }
    if (gpio.write(pan.pinEna_, 1) < 0) { result = 1; }
    if (gpio.write(pan.pinDir_, 0) < 0) { result = 1; }
    if (gpio.terminate() < 0) { result = 1; }
    return result;
}

constexpr const char* ON_MOVE_PULSES = "[movePulses()]";

int movePulses(int64_t pulses, const StepperMotorRunConfig& pan) {
    auto& gpio = Gpio::instance();
    int code = gpio.write(pan.pinDir_, pulses >= 0 ? 1 : 0);
    if (code < 0) {
        printf("%s ERROR: failed to set direction: %d\n", ON_MOVE_PULSES, code);
        return code;
    }
    std::this_thread::sleep_for(std::chrono::nanoseconds(pan.pulseHigh_));
    const uint64_t count = pulses >= 0 ? static_cast<uint64_t>(pulses) : static_cast<uint64_t>(-pulses);
    for (uint64_t i = 0; i < count; ++i) {
        code = gpio.write(pan.pinStep_, 1);
        if (code >= 0) { std::this_thread::sleep_for(std::chrono::nanoseconds(pan.pulseHigh_)); }
        if (code >= 0) { code = gpio.write(pan.pinStep_, 0); }
        if (code < 0) {
            printf("%s ERROR: failed at pulse %llu: %d\n", ON_MOVE_PULSES,
                static_cast<unsigned long long>(i + 1), code);
            return code;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(STEP_LOW_US));
    }
    return Gpio::SUCCESS;
}
