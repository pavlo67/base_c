#include "hardware/hardware.h"

#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <unistd.h>

constexpr unsigned BLINK_DELAY_US = 5e5;

void blink(int pin) {
    gpioWrite(pin, 1);
    usleep(BLINK_DELAY_US);
    gpioWrite(pin, 0);
    usleep(BLINK_DELAY_US);
}

constexpr int PINS[]     = {PIN_STEP, PIN_DIR, PIN_ENA};
constexpr int PINS_CNT   = sizeof(PINS) / sizeof(PINS[0]);
constexpr int BLINKS_CNT = 5;

int main() {

    if (gpioInitialise() < 0) {
        std::fprintf(stderr, "gpioInitialise() failed\n");
        return EXIT_FAILURE;
    }

    for (int pinI = 0; pinI < PINS_CNT; pinI++) {
        int pin = PINS[pinI];
        printf("\npin: %d\n", pin);

        gpioSetMode(pin, PI_OUTPUT);

        for (int i = 0; i < BLINKS_CNT; i++) {
            printf("%d\n", i);
            blink(pin);
        }
    }

    gpioTerminate();

    printf("\nStopped\n");
    return EXIT_SUCCESS;

}