#include "hardware/hardware.h"

#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

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

constexpr unsigned STEP_DELAY_US = 5e5;

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

    gpioSetMode(PIN_STEP, PI_OUTPUT);

    for (int i = 0; i++ < 5; ) {
        printf("%d\n", i);
        pulse();
    }

    gpioTerminate();

    printf("\nStopped\n");
    return EXIT_SUCCESS;
}