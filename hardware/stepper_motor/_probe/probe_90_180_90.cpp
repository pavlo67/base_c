#include <thread>
#include <iostream>
#include <unistd.h>

#include "lib/time.h"

#include "hardware/hardware.h"

constexpr int PIN_STEP = 17; // GPIO17 -> PUL-
constexpr int PIN_DIR  = 23; // GPIO23 -> DIR-
constexpr int PIN_ENA  = 24; // GPIO24 -> ENA-

constexpr int PULSE_HIGH_US =  20;
constexpr int STEP_DELAY_US = 605;

void pulse() {
    gpioWrite(PIN_STEP, 1);
    usleep(PULSE_HIGH_US);
    gpioWrite(PIN_STEP, 0);
    usleep(STEP_DELAY_US);
}

void moveSteps(int steps) {
    bool dir = steps >= 0;
    gpioWrite(PIN_DIR, dir ? 1 : 0);
    usleep(5);

    for (int i = 0; i < std::abs(steps); ++i) {
        if (i % 100 == 0) {
            printf("%d\n", i);
        }
        pulse();
    }
}

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        return 1;
    }

    gpioSetMode(PIN_STEP, PI_OUTPUT);
    gpioSetMode(PIN_DIR,  PI_OUTPUT);
    gpioSetMode(PIN_ENA,  PI_OUTPUT);

    // Для більшості DM542: ENA LOW = enabled, але перевір по своєму драйверу.
    gpioWrite(PIN_ENA, 0);
    usleep(500);

    moveSteps(+STEPS);       // вправо
    usleep(500);

    moveSteps(-2 * STEPS);   // вліво
    usleep(500);

    moveSteps(+STEPS);       // назад у вихідну
    usleep(500);

    gpioWrite(PIN_ENA, 1);   // stop / disable

    gpioTerminate();
    return 0;
}