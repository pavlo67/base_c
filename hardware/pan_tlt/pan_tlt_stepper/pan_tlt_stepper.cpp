#include "pan_tlt_stepper.h"

#include "_base_defines.h"
#include "hardware/hardware.h"

#include <chrono>
#include <thread>
#include <cstdlib>

constexpr int STEP_PULSE_US  = 20;     // HIGH, точно з запасом
constexpr int STEP_PERIOD_US = 1000;  // 1 кГц = 1000 мікрокроків/с

constexpr uint8_t PIN_STEP_PAN       = 18; // GPIO18 -> PUL- PAN
constexpr uint8_t PIN_DIR_PAN        = 23; // GPIO23 -> DIR- PAN
constexpr uint8_t PIN_ENA_PAN        = 24; // GPIO24 -> ENA- PAN

constexpr uint8_t PIN_STEP_TLT       = 25; // GPIO18 -> PUL- TLT
constexpr uint8_t PIN_DIR_TLT        = 26; // GPIO23 -> DIR- TLT
constexpr uint8_t PIN_ENA_TLT        = 27; // GPIO24 -> ENA- TLT


// constexpr int STEPS = 800;         // залежить від microstep на DM542

void pulse(uint8_t pin) {
    gpioWrite(pin, 1);
    std::this_thread::sleep_for(std::chrono::microseconds(STEP_PULSE_US));
    gpioWrite(pin, 0);
    std::this_thread::sleep_for(std::chrono::microseconds(STEP_PERIOD_US - STEP_PULSE_US));
}

void moveAxis(uint8_t stepPin, uint8_t directionPin, int steps) {
    if (steps == 0) return;
    gpioWrite(directionPin, steps > 0 ? 1 : 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    for (int i = 0; i < std::abs(steps); ++i) pulse(stepPin);
}

PanTltStepper::PanTltStepper() {
    initialized_ = gpioInitialise() >= 0;
    if (!initialized_) return;

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
    Info info;
    set(0, 0, info);
}

void PanTltStepper::set (int pan, int tlt, Info& info) {
    move(pan - pan_, tlt - tlt_, info);
}

void PanTltStepper::move(int pan, int tlt, Info& info) {
    if (!initialized_) {
        info.setError("PanTltStepper is not initialized");
        return;
    }
    moveAxis(PIN_STEP_PAN, PIN_DIR_PAN, pan);
    moveAxis(PIN_STEP_TLT, PIN_DIR_TLT, tlt);
    pan_ += pan;
    tlt_ += tlt;
    info.setOk();
}

#if RUN_PROBE
    int main() {
        return EXIT_SUCCESS;
    }
#endif
