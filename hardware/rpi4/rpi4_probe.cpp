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

    for (int i = 0; i++ < 5; ) {
        printf("%s ON\n", name);
        gpioWrite(pin, 1);
        sleep(1);

        printf("%s OFF\n", name);
        gpioWrite(pin, 0);
        sleep(1);
    }
}

int main() {
    // std::signal(SIGINT, handleSignal);
    // std::signal(SIGTERM, handleSignal);

    if (gpioInitialise() < 0) {
        std::fprintf(stderr, "gpioInitialise() failed\n");
        return EXIT_FAILURE;
    }

    gpioSetMode(PIN_IN1, PI_OUTPUT);

    // gpioSetMode(PIN_IN2, PI_OUTPUT);
    // gpioSetMode(PIN_IN3, PI_OUTPUT);
    // setAll(0);
    // std::printf("Sequential test\n");

    testOnePin(PIN_IN1, "IN1");

    // testOnePin(PIN_IN2, "IN2");
    // testOnePin(PIN_IN3, "IN3");

    gpioTerminate();

    std::printf("\nStopped\n");
    return EXIT_SUCCESS;
}