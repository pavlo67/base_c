#include "hardware/hardware.h"

#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

// void testOnePin(unsigned pin, const char* name) {
//
//     for (int i = 0; i++ < 50; ) {
//         printf("%s ON\n", name);
//         gpioWrite(pin, 1);
//         usleep(500000);
//
//         printf("%s OFF\n", name);
//         gpioWrite(pin, 0);
//         usleep(500000);
//     }
// }

constexpr unsigned STEP_DELAY_US = 5e5;

void pulse(int pin) {
    gpioWrite(pin, 1);
    usleep(STEP_DELAY_US);
    gpioWrite(pin, 0);
    usleep(STEP_DELAY_US);
}

const int pin = PIN_DIR;

int main() {

    printf("pin: %d\n", pin);

    if (gpioInitialise() < 0) {
        std::fprintf(stderr, "gpioInitialise() failed\n");
        return EXIT_FAILURE;
    }

    gpioSetMode(pin, PI_OUTPUT);

    for (int i = 0; i++ < 5; ) {
        printf("%d\n", i);
        pulse(pin);
    }

    gpioTerminate();

    printf("\nStopped\n");
    return EXIT_SUCCESS;
}