// test_dm542.cpp
#include <chrono>
#include <thread>
#include <iostream>
#include "hardware/hardware.h"

using namespace std::chrono_literals;

constexpr int PIN_STEP = 18; // GPIO18 -> PUL-
constexpr int PIN_DIR  = 23; // GPIO23 -> DIR-
constexpr int PIN_ENA  = 24; // GPIO24 -> ENA-

constexpr int STEP_DELAY_US = 800; // повільно і безпечно
constexpr int STEPS = 800;         // залежить від microstep на DM542

void pulse() {
    gpioWrite(PIN_STEP, 1);
    std::this_thread::sleep_for(std::chrono::microseconds(STEP_DELAY_US));
    gpioWrite(PIN_STEP, 0);
    std::this_thread::sleep_for(std::chrono::microseconds(STEP_DELAY_US));
}

void moveSteps(int steps) {
    bool dir = steps >= 0;
    gpioWrite(PIN_DIR, dir ? 1 : 0);
    std::this_thread::sleep_for(5ms);

    for (int i = 0; i < std::abs(steps); ++i) {
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
    std::this_thread::sleep_for(500ms);

    moveSteps(+STEPS);       // вправо
    std::this_thread::sleep_for(500ms);

    moveSteps(-2 * STEPS);   // вліво
    std::this_thread::sleep_for(500ms);

    moveSteps(+STEPS);       // назад у вихідну
    std::this_thread::sleep_for(500ms);

    gpioWrite(PIN_ENA, 1);   // stop / disable

    gpioTerminate();
    return 0;
}