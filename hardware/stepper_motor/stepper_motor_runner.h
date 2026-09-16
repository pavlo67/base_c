#ifndef BASE_CPP_HARDWARE_STEPPER_MOTOR_STEPPER_MOTOR_RUNNER_H
#define BASE_CPP_HARDWARE_STEPPER_MOTOR_STEPPER_MOTOR_RUNNER_H

#include <array>
#include <algorithm>
#include <chrono>
#include <optional>

#include "lib/timelib.h"
#include "smart/stepper_motor_smart.h"
#include "hardware/gpio/gpio.h"

// One motor's preparation and incremental real-time scheduling. No sleeps.
// Call prepare for every motor before the first update; hold execution ownership
// until these objects are destroyed. Neither method owns GPIO lifecycle.
class StepperMotorRunner {
public:
    int prepare(const StepperMotorRunConfig& config, float angle, std::array<bool, Gpio::PIN_COUNT>& usedPins);
    int update();
    [[nodiscard]] Clock::time_point nextWake() const { return std::min(next_, deadline_); }

private:
    std::optional<StepperMotorSmart> motor_;
    Clock::time_point next_{};
    Clock::time_point deadline_{};
    std::chrono::nanoseconds tick_{};
    duration timeLimit_ = 0;
    bool started_ = false;
    int status_ = StepperMotor::COMPLETE;
};

#endif // BASE_CPP_HARDWARE_STEPPER_MOTOR_STEPPER_MOTOR_RUNNER_H
