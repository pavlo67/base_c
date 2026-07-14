#include "hardware/hardware.h"

#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

constexpr unsigned PIN_IN1 = 17;  // BCM GPIO17, фізичний пін 11

// constexpr unsigned PIN_IN2 = 27;  // BCM GPIO27, фізичний пін 13
// constexpr unsigned PIN_IN3 = 22;  // BCM GPIO22, фізичний пін 15
// volatile std::sig_atomic_t stopRequested = 0;
// void handleSignal(int) {
//     stopRequested = 1;
// }
// void setAll(int level) {
//     gpioWrite(PIN_IN1, level);
//     gpioWrite(PIN_IN2, level);
//     gpioWrite(PIN_IN3, level);
// }

void testOnePin(unsigned pin, const char* name) {

    for (int i = 0; i++ < 50; ) {
        printf("%s ON\n", name);
        gpioWrite(pin, 1);
        usleep(500000);

        printf("%s OFF\n", name);
        gpioWrite(pin, 0);
        usleep(500000);
    }
}

constexpr unsigned STEP_DELAY_US = 800;
constexpr unsigned PIN_STEP = PIN_IN1;

void pulse() {
    gpioWrite(PIN_STEP, 1);
    usleep(STEP_DELAY_US);
    gpioWrite(PIN_STEP, 0);
    usleep(STEP_DELAY_US);
}

int main() {
    if (gpioInitialise() < 0) {
        std::fprintf(stderr, "gpioInitialise() failed\n");
        return EXIT_FAILURE;
    }

    gpioSetMode(PIN_IN1, PI_OUTPUT);

    // testOnePin(PIN_IN1, "IN1");

    for (int i = 0; i++ < 10000; ) {
        pulse();
    }

    gpioTerminate();

    std::printf("\nStopped\n");
    return EXIT_SUCCESS;
}