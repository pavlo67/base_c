#include "hardware/gpio/gpio.h"

#include <cxxopts.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <thread>

constexpr const char* MAIN_CONTEXT = "[main()]";
constexpr unsigned BLINK_DELAY_US = 500000;
constexpr int BLINKS_CNT = 5;

int main(int argc, char** argv) {

    std::vector<unsigned> pins;
    try {
        cxxopts::Options options("gpio_probe", "Blink BCM GPIO pins in argument order");
        options.add_options()
            ("h,help", "Show usage")
            ("pins", "BCM pin numbers (0..27)", cxxopts::value<std::vector<unsigned>>());
        options.parse_positional({"pins"});
        options.positional_help("PIN [PIN ...]");
        const auto arguments = options.parse(argc, argv);
        if (arguments.count("help") != 0) {
            printf("%s\n", options.help().c_str());
            return EXIT_SUCCESS;
        }
        if (arguments.count("pins") == 0) {
            printf("%s ERROR: expected BCM pins; usage: sudo ./gpio_probe 3 5 11\n", MAIN_CONTEXT);
            return EXIT_FAILURE;
        }
        pins = arguments["pins"].as<std::vector<unsigned>>();
    } catch (const cxxopts::exceptions::exception& error) {
        printf("%s ERROR: %s\n", MAIN_CONTEXT, error.what());
        return EXIT_FAILURE;
    }

    for (const unsigned pin : pins) {
        if (pin >= Gpio::PIN_COUNT) {
            printf("%s ERROR: invalid BCM pin %u; expected an integer in 0..%u\n",
                   MAIN_CONTEXT, pin, Gpio::PIN_COUNT - 1);
            return EXIT_FAILURE;
        }
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
