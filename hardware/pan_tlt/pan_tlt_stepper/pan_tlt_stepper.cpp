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
    Gpio::instance().write(pin, 1);
    std::this_thread::sleep_for(std::chrono::microseconds(STEP_PULSE_US));
    Gpio::instance().write(pin, 0);
    std::this_thread::sleep_for(std::chrono::microseconds(STEP_PERIOD_US - STEP_PULSE_US));
}

void moveAxis(uint8_t stepPin, uint8_t directionPin, int steps) {
    if (steps == 0) { return; }
    Gpio::instance().write(directionPin, steps > 0 ? 1 : 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    for (int i = 0; i < std::abs(steps); ++i) { pulse(stepPin); }
}

PanTltStepper::PanTltStepper() {
    initialized_ = Gpio::instance().initialize() >= 0;
    if (!initialized_) { return; }

    Gpio::instance().setMode(PIN_STEP_PAN, GpioMode::output);
    Gpio::instance().setMode(PIN_DIR_PAN,  GpioMode::output);
    Gpio::instance().setMode(PIN_ENA_PAN,  GpioMode::output);

    Gpio::instance().setMode(PIN_STEP_TLT, GpioMode::output);
    Gpio::instance().setMode(PIN_DIR_TLT,  GpioMode::output);
    Gpio::instance().setMode(PIN_ENA_TLT,  GpioMode::output);

    // Для більшості DM542: ENA LOW = enabled, але перевір по своєму драйверу.
    Gpio::instance().write(PIN_ENA_PAN, 0);
    Gpio::instance().write(PIN_ENA_TLT, 0);

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
    const std::string ON_MAIN = "on main(): ";

int main() {
        return EXIT_SUCCESS;
    }
#endif
