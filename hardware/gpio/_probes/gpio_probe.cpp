#include "hardware/hardware.h"

#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <cxxopts.hpp>

constexpr unsigned BLINK_DELAY_US = 5e5;
constexpr int      BLINKS_CNT     = 5;

void blink(int pin) {
    Gpio::instance().write(pin, 1);
    usleep(BLINK_DELAY_US);
    Gpio::instance().write(pin, 0);
    usleep(BLINK_DELAY_US);
}

int main(int argc, char* argv[]) {
    cxxopts::Options options("GPIO", "Блимання пінами по списку");

    options.add_options()
        ("pins", "Список цілих чисел", cxxopts::value<std::vector<int>>());

    options.parse_positional({"pins"});

    std::vector<int>pins;

    try {
        auto result = options.parse(argc, argv);
        pins = result["pins"].as<std::vector<int>>();
        std::cout << "Зчитано номерів пінів: " << pins.size() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Помилка аргументів: " << e.what() << "\n";
        std::cerr << "Будь ласка, задайте список невідʼємних цілих чисел.\n";
        return EXIT_FAILURE;
    }

    if (Gpio::instance().initialize() < 0) {
        std::fprintf(stdout, "ERROR: gpioInitialise() failed\n");
        return EXIT_FAILURE;
    }

    for (uint pin : pins) {
        printf("\ntesting pin: %d...\n", pin);
        Gpio::instance().setMode(pin, GpioMode::output);
        for (int i = 0; i < BLINKS_CNT; i++) {
            printf("%d\n", i);
            blink((int)pin);
        }
    }

    Gpio::instance().terminate();

    printf("\nFinished\n");
    return EXIT_SUCCESS;

}