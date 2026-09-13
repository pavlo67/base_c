#pragma once

#include <array>
#include <algorithm>
#include <chrono>
#include <optional>

#include "stepper_motor.h"
#include "hardware/gpio/gpio.h"

// One motor's preparation and incremental real-time scheduling. No sleeps.
// Call prepare for every motor before the first update; hold execution ownership
// until these objects are destroyed. Neither method owns GPIO lifecycle.
class StepperMotorRunner {
public:
    using Clock = std::chrono::steady_clock;
    int prepare(const StepperMotorRunConfig& config, float angle,
        std::array<bool, Gpio::PIN_COUNT>& usedPins);
    int update();
    [[nodiscard]] Clock::time_point nextWake() const { return std::min(next_, deadline_); }

private:
    std::optional<StepperMotorAction> motor_;
    Clock::time_point next_{};
    Clock::time_point deadline_{};
    std::chrono::nanoseconds tick_{};
    duration timeLimit_ = 0;
    bool started_ = false;
    int status_ = StepperMotorAction::COMPLETE;
};
