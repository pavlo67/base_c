#include <thread>
#include <iostream>
#include <unistd.h>

#include "lib/timelib.h"

#include "hardware/hardware.h"

constexpr int STEP_DELAY_US = 605;
constexpr int STEPS = 800;

void pulse() {
    Gpio::instance().write(PIN_STEP, 1);
    usleep(PULSE_HIGH_US_MIN);
    Gpio::instance().write(PIN_STEP, 0);
    usleep(STEP_DELAY_US);
}

void moveSeries(int steps) {
    bool dir = steps >= 0;
    Gpio::instance().write(PIN_DIR, dir ? 1 : 0);
    printf("DIR: %d\n", dir);

    usleep(PULSE_HIGH_US_MIN);

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
        std::cerr << ON_MAIN << "GPIO initialization failed\n";
        return 1;
    }

    Gpio::instance().setMode(PIN_STEP, GpioMode::output);
    Gpio::instance().setMode(PIN_DIR,  GpioMode::output);
    Gpio::instance().setMode(PIN_ENA,  GpioMode::output);

    Gpio::instance().write(PIN_ENA, 0); // Для більшості DM542: ENA LOW = enabled
    usleep(500);

    moveSeries(+STEPS);       // вправо
    usleep(500);

    moveSeries(-2 * STEPS);   // вліво
    usleep(500);

    moveSeries(+STEPS);       // назад у вихідну
    usleep(500);

    Gpio::instance().write(PIN_ENA, 1);   // stop / disable
    Gpio::instance().write(PIN_DIR, 0);

    Gpio::instance().terminate();
    return 0;
}