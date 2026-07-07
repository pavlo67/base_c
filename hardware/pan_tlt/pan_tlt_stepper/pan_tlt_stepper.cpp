#include "pan_tlt_stepper.h"

#include "_defines.h"

#include <chrono>
#include <thread>
#include <iostream>

constexpr int STEP_PULSE_US  = 20;     // HIGH, точно з запасом
constexpr int STEP_PERIOD_US = 1000;  // 1 кГц = 1000 мікрокроків/с

constexpr uint8_t PIN_STEP_PAN       = 18; // GPIO18 -> PUL- PAN
constexpr uint8_t PIN_DIR_PAN        = 23; // GPIO23 -> DIR- PAN
constexpr uint8_t PIN_ENA_PAN        = 24; // GPIO24 -> ENA- PAN

constexpr uint8_t PIN_STEP_TLT       = 25; // GPIO18 -> PUL- TLT
constexpr uint8_t PIN_DIR_TLT        = 26; // GPIO23 -> DIR- TLT
constexpr uint8_t PIN_ENA_TLT        = 27; // GPIO24 -> ENA- TLT

#if SYSTEM_IS_RPI4
    #include <pigpio.h>
#else
    #define gpioWrite(pin, value)
    #define gpioSetMode(pin, mode)
    #define gpioInitialise()  1
    #define gpioTerminate()

#endif

// constexpr int STEPS = 800;         // залежить від microstep на DM542

void pulse(uint8_t pin) {
    gpioWrite(pin, 1);
    sleep(STEP_PULSE_US);
    gpioWrite(pin, 0);
    sleep(STEP_PERIOD_US - STEP_PULSE_US);
}

PanTltStepper::PanTltStepper() {
    if (gpioInitialise() < 0) {
        throw std::runtime_error("PanTltStepper: pigpio init failed");
    }

    gpioSetMode(PIN_STEP_PAN, PI_OUTPUT);
    gpioSetMode(PIN_DIR_PAN,  PI_OUTPUT);
    gpioSetMode(PIN_ENA_PAN,  PI_OUTPUT);

    gpioSetMode(PIN_STEP_TLT, PI_OUTPUT);
    gpioSetMode(PIN_DIR_TLT,  PI_OUTPUT);
    gpioSetMode(PIN_ENA_TLT,  PI_OUTPUT);

    // Для більшості DM542: ENA LOW = enabled, але перевір по своєму драйверу.
    gpioWrite(PIN_ENA_PAN, 0);
    gpioWrite(PIN_ENA_TLT, 0);

}



void PanTltStepper::zero() {

}

void PanTltStepper::set (int pan, int tlt, Info& info) {

}

void PanTltStepper::move(int pan, int tlt, Info& info) {
    gpioWrite(PIN_DIR_PAN, (pan > 0) ? 1 : 0);
    std::this_thread::sleep_for(5ms);

    for (int i = 0; i < std::abs(steps); ++i) {
        pulse();
    }
}

#if RUN_PROBE
    int main() {
        return EXIT_SUCCESS;
    }
#endif
