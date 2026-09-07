#include <thread>
#include <iostream>
#include <unistd.h>

#include "lib/timelib.h"

#include "hardware/hardware.h"

constexpr int STEP_DELAY_US = 605;

void pulse() {
    Gpio::instance().write(PIN_STEP, 1);
    usleep(PULSE_HIGH_US_MIN);
    Gpio::instance().write(PIN_STEP, 0);
    usleep(STEP_DELAY_US);
}

void moveSeries(int steps) {
    bool dir = steps >= 0;
    Gpio::instance().write(PIN_DIR, dir ? 1 : 0);
    usleep(5);

    for (int i = 0; i < std::abs(steps); ++i) {
        if (i % 100 == 0) {
            printf("%d\n", i);
        }
        pulse();
    }
}

const std::string ON_MAIN = "on main(): ";

int main() {
    if (Gpio::instance().initialize() < 0) {
        std::cout << "ERROR: " << ON_MAIN << "GPIO initialization failed\n";
        return 1;
    }

    Gpio::instance().setMode(PIN_STEP, GpioMode::output);
    Gpio::instance().setMode(PIN_DIR,  GpioMode::output);
    Gpio::instance().setMode(PIN_ENA,  GpioMode::output);

    Gpio::instance().write(PIN_ENA, 0); // Для більшості DM542: ENA LOW = enabled
    usleep(500);

    int i = 0;
    while (true) {
        if (i % 100 == 0) {
            printf("%d\n", i);
        }
        pulse();
        i++;
    }


    Gpio::instance().write(PIN_ENA, 1);   // stop / disable

    Gpio::instance().terminate();
    return 0;
}