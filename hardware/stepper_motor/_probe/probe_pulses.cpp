#include "hardware/hardware.h"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string_view>
#include <thread>
#include <vector>

constexpr int STEP_LOW_US = 605;
constexpr int BETWEEN_SERIES_MS = 10;

int movePulses(int64_t pulses);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("[probe_pulses.main()] ERROR: supply signed pulse counts, e.g. probe_pulses +800 -1600 +800\n");
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
            printf("[probe_pulses.main()] ERROR: argument %d must be a signed integer pulse count\n", i);
            return 1;
        }
        movements.push_back(pulses);
    }
    auto& gpio = Gpio::instance();
    if (gpio.initialize() < 0) {
        printf("[probe_pulses.main()] ERROR: GPIO initialization failed\n");
        return 1;
    }
    int result = 0;
    int code = gpio.setMode(PIN_STEP, GpioMode::output);
    if (code >= 0) { code = gpio.setMode(PIN_DIR, GpioMode::output); }
    if (code >= 0) { code = gpio.setMode(PIN_ENA, GpioMode::output); }
    if (code >= 0) { code = gpio.write(PIN_ENA, 0); }
    if (code < 0) {
        printf("[probe_pulses.main()] ERROR: GPIO setup failed: %d\n", code);
        result = 1;
    } else {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        for (size_t i = 0; i < movements.size(); ++i) {
            printf("[PUL] Series %zu/%zu: %lld pulses\n", i + 1, movements.size(),
                static_cast<long long>(movements[i]));
            if (movePulses(movements[i]) < 0) { result = 1; break; }
            if (i + 1 < movements.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(BETWEEN_SERIES_MS));
            }
        }
    }
    if (gpio.write(PIN_STEP, 0) < 0) { result = 1; }
    if (gpio.write(PIN_ENA, 1) < 0) { result = 1; }
    if (gpio.write(PIN_DIR, 0) < 0) { result = 1; }
    if (gpio.terminate() < 0) { result = 1; }
    return result;
}

constexpr const char* ON_MOVE_PULSES = "[movePulses()]";

int movePulses(int64_t pulses) {
    auto& gpio = Gpio::instance();
    int code = gpio.write(PIN_DIR, pulses >= 0 ? 1 : 0);
    if (code < 0) {
        printf("%s ERROR: failed to set direction: %d\n", ON_MOVE_PULSES, code);
        return code;
    }
    std::this_thread::sleep_for(std::chrono::microseconds(PULSE_HIGH_US_MIN));
    const uint64_t count = pulses >= 0 ? static_cast<uint64_t>(pulses) : static_cast<uint64_t>(-pulses);
    for (uint64_t i = 0; i < count; ++i) {
        code = gpio.write(PIN_STEP, 1);
        if (code >= 0) { std::this_thread::sleep_for(std::chrono::microseconds(PULSE_HIGH_US_MIN)); }
        if (code >= 0) { code = gpio.write(PIN_STEP, 0); }
        if (code < 0) {
            printf("%s ERROR: failed at pulse %llu: %d\n", ON_MOVE_PULSES,
                static_cast<unsigned long long>(i + 1), code);
            return code;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(STEP_LOW_US));
    }
    return Gpio::SUCCESS;
}
