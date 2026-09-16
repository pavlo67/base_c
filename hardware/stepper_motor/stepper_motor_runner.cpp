#include "stepper_motor_runner.h"

#include <algorithm>
#include <cstdio>
#include <limits>

constexpr const char* ON_RUNNER_PREPARE = "[StepperMotorRunner.prepare()]";

int StepperMotorRunner::prepare(const StepperMotorRunConfig& config, float angle,
        std::array<bool, Gpio::PIN_COUNT>& usedPins) {
    motor_.reset();
    status_ = StepperMotor::COMPLETE;
    started_ = false;
    std::string error;
    const duration maximum = std::numeric_limits<int64_t>::max() / 2;
    if (!optionsIsOk(config.options_, error) || !std::isfinite(angle) ||
            config.timeLimit_ == 0 || config.timeLimit_ > maximum ||
            config.expecterInterval_ > maximum || config.pulseHigh_ == 0 || config.pulseHigh_ > SECOND / 2) {
        printf("%s ERROR: invalid motor options, angle or timing: %s\n", ON_RUNNER_PREPARE, error.c_str());
        return Gpio::INVALID_ARGUMENT;
    }
    for (const auto pin : {config.pinStep_, config.pinDir_, config.pinEna_}) {
        if (pin >= Gpio::PIN_COUNT || usedPins[pin]) {
            printf("%s ERROR: pins must be valid and distinct across both motors\n", ON_RUNNER_PREPARE);
            return Gpio::INVALID_ARGUMENT;
        }
        usedPins[pin] = true;
    }
    if (angle == 0) { return status_; }
    const auto plan = getSeriesSequence(0, angle, 0, config.expecterInterval_, config.options_);
    if (!plan.error.empty()) {
        printf("%s ERROR: planning failed: %s\n", ON_RUNNER_PREPARE, plan.error.c_str());
        return Gpio::INVALID_ARGUMENT;
    }
    motor_.emplace(plan, config.pinStep_, config.pinDir_, config.pinEna_, config.options_,
        config.pulseHigh_, config.hardwarePwm_);
    tick_ = std::chrono::nanoseconds(std::max<duration>(MICROSECOND, config.expecterInterval_));
    timeLimit_ = config.timeLimit_;
    status_ = StepperMotor::RUNNING;
    return status_;
}

constexpr const char* ON_RUNNER_UPDATE = "[StepperMotorRunner.update()]";

int StepperMotorRunner::update() {
    if (status_ != StepperMotor::RUNNING) { return status_; }
    const auto current = Clock::now();
    if (!started_) {
        next_ = current;
        deadline_ = current + std::chrono::nanoseconds(timeLimit_);
        started_ = true;
    }
    if (current >= deadline_) {
        const int cleanup = motor_->stop();
        status_ = cleanup < 0 ? cleanup : StepperMotor::TIME_LIMIT;
        printf("%s ERROR: motion failed (code %d)\n", ON_RUNNER_UPDATE, status_);
        return status_;
    }
    if (current < next_) { return status_; }
    const moment at = std::chrono::duration_cast<std::chrono::nanoseconds>(current.time_since_epoch()).count();
    status_ = motor_->action(at);
    next_ += tick_;
    const auto after = Clock::now();
    if (next_ < after) { next_ += tick_ * ((after - next_) / tick_ + 1); }
    return status_;
}
