#include "hardware/gpio/gpio.h"

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <vector>
#include <thread>

constexpr const char* MAIN_CONTEXT = "[main()]";
constexpr unsigned BLINK_DELAY_US = 500000;
constexpr int BLINKS_CNT = 5;

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("%s ERROR: expected BCM pins; usage: sudo ./gpio_probe 3 5 11\n", MAIN_CONTEXT);
        return EXIT_FAILURE;
    }
    std::vector<unsigned> pins;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        unsigned pin = 0;
        const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), pin);
        if (parsed.ec != std::errc{} || parsed.ptr != argument.data() + argument.size()
            || pin >= Gpio::PIN_COUNT) {
            printf("%s ERROR: invalid BCM pin '%s'; expected an integer in 0..%u\n",
                   MAIN_CONTEXT, argv[index], Gpio::PIN_COUNT - 1);
            return EXIT_FAILURE;
        }
        pins.push_back(pin);
    }

    auto& gpio = Gpio::instance();
    if (gpio.initialize() < 0) {
        printf("%s ERROR: GPIO initialization failed\n", MAIN_CONTEXT);
        return EXIT_FAILURE;
    }
    int result = EXIT_SUCCESS;
    for (const unsigned pin : pins) {
        printf("[GPIO] Blink BCM%u\n", pin);
        int code = gpio.setMode(pin, GpioMode::output);
        for (int i = 0; i < BLINKS_CNT && code >= 0; ++i) {
            code = gpio.write(pin, 1);
            if (code < 0) { break; }
            std::this_thread::sleep_for(std::chrono::microseconds(BLINK_DELAY_US));
            code = gpio.write(pin, 0);
            if (code < 0) { break; }
            std::this_thread::sleep_for(std::chrono::microseconds(BLINK_DELAY_US));
        }
        if (code < 0) {
            printf("%s ERROR: BCM%u blink failed: %d\n", MAIN_CONTEXT, pin, code);
            result = EXIT_FAILURE;
        }
        const int cleanup = gpio.setMode(pin, GpioMode::input);
        if (cleanup < 0) {
            printf("%s ERROR: BCM%u cleanup failed: %d\n", MAIN_CONTEXT, pin, cleanup);
            result = EXIT_FAILURE;
        }
        if (result != EXIT_SUCCESS) { break; }
    }
    const int terminated = gpio.terminate();
    if (terminated < 0) {
        printf("%s ERROR: GPIO termination failed: %d\n", MAIN_CONTEXT, terminated);
        result = EXIT_FAILURE;
    }
    return result;
}
