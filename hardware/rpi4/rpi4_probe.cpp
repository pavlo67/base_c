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

constexpr int PINS[] = {PIN_STEP, PIN_DIR, PIN_ENA};
constexpr int PINS_CNT = sizeof(PINS) / sizeof(PINS[0]);

int main() {

    if (gpioInitialise() < 0) {
        std::fprintf(stderr, "gpioInitialise() failed\n");
        return EXIT_FAILURE;
    }

    for (int pinI = 0; pinI < PINS_CNT; pinI++) {
        int pin = PINS[pinI];
        printf("\npin: %d\n", pin);

        gpioSetMode(pin, PI_OUTPUT);

        for (int i = 0; i < 5; i++) {
            printf("%d\n", i);
            pulse(pin);
        }
    }

    gpioTerminate();

    printf("\nStopped\n");
    return EXIT_SUCCESS;

}