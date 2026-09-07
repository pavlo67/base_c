#include "stepper_motor.h"
#include "hardware/gpio/gpio.h"

#include <chrono>
#include <cstdio>
#include <thread>

const std::string ON_MOVE = "on StepperMotorSeriesSequence.move(): ";

int move(const StepperMotorSeriesSequence& sequence, unsigned pinStep, unsigned pinDir, unsigned pinEna,
        duration expecterInterval, const stepper_motor_options_t& stepperOpts, duration pulseHigh) {
    std::string error;
    if (pinStep >= Gpio::PIN_COUNT || pinDir >= Gpio::PIN_COUNT || pinEna >= Gpio::PIN_COUNT ||
            pinStep == pinDir || pinStep == pinEna || pinDir == pinEna || pulseHigh == 0 ||
            !sequence.error.empty() || !optionsIsOk(stepperOpts, error)) {
        printf("ERROR: %sinvalid motor pins, pulse width, options or sequence: %s %s\n",
            ON_MOVE.c_str(), error.c_str(), sequence.error.c_str());
        return Gpio::INVALID_ARGUMENT;
    }
    if (sequence.seq.empty()) { return Gpio::SUCCESS; }

    auto& gpio = Gpio::instance();
    int result = gpio.setMode(pinEna, GpioMode::output);
    const bool enableConfigured = result >= 0;
    if (result >= 0) { result = gpio.write(pinEna, 1); }
    if (result >= 0) { result = gpio.setMode(pinStep, GpioMode::output); }
    const bool stepConfigured = result >= 0;
    if (result >= 0) { result = gpio.setMode(pinDir, GpioMode::output); }
    const bool directionConfigured = result >= 0;
    if (result >= 0) { result = gpio.write(pinEna, 0); }
    if (result >= 0) { std::this_thread::sleep_for(std::chrono::microseconds(500)); }

    for (const auto& series : sequence.seq) {
        if (result < 0) { break; }
        result = gpio.write(pinDir, series.directionForward_ ? 1 : 0);
        if (result < 0) { break; }
        std::this_thread::sleep_for(std::chrono::nanoseconds(pulseHigh));
        const auto origin = std::chrono::steady_clock::now();
        const auto emit = [&](moment at) {
            if (result < 0) { return; }
            std::this_thread::sleep_until(origin + std::chrono::nanoseconds(at - series.startedAt_));
            result = gpio.write(pinStep, 1);
            if (result < 0) { return; }
            std::this_thread::sleep_for(std::chrono::nanoseconds(pulseHigh));
            result = gpio.write(pinStep, 0);
        };
        (void)evaluateSeries(series, expecterInterval, stepperOpts, series.startedAt_, series.pulsesCount_, emit);
        if (result >= 0) {
            std::this_thread::sleep_until(origin + std::chrono::nanoseconds(series.observedAt_ - series.startedAt_));
        }
    }

    // Attempt every applicable cleanup operation, preserving the first error.
    if (stepConfigured) {
        const int cleanup = gpio.write(pinStep, 0);
        if (result >= 0) { result = cleanup; }
    }
    if (enableConfigured) {
        const int cleanup = gpio.write(pinEna, 1);
        if (result >= 0) { result = cleanup; }
    }
    if (directionConfigured) {
        const int cleanup = gpio.write(pinDir, 0);
        if (result >= 0) { result = cleanup; }
    }
    if (result < 0) { printf("ERROR: %sGPIO operation failed: %d\n", ON_MOVE.c_str(), result); }
    return result;
}
