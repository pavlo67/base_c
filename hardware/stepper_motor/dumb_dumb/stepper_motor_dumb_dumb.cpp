#include "stepper_motor_dumb_dumb.h"

#include <cmath>
#include <limits>

constexpr const char* ON_DUMB_DUMB_REPLACE_MOVE = "[StepperMotorDumbDumb.replaceMove()]";
constexpr const char* ON_DUMB_DUMB_BACKGROUND = "[StepperMotorDumbDumb.runBackground()]";

StepperMotorDumbDumb::StepperMotorDumbDumb(const StepperMotorRunConfig& config)
    : StepperMotor(config) {
    worker_ = std::jthread([this](std::stop_token stop) { runBackground(stop); });
}

StepperMotorDumbDumb::~StepperMotorDumbDumb() {
    worker_.request_stop();
    wake_.notify_all();
}

int StepperMotorDumbDumb::action(moment at) {
    (void)at;
    return COMPLETE;
}

int StepperMotorDumbDumb::configurePinsLocked() {
    if (pinsConfigured_) { return Gpio::SUCCESS; }
    auto& gpio = Gpio::instance();
    int code = gpio.setMode(config_.pinStep_, GpioMode::output);
    if (code >= 0) { code = gpio.write(config_.pinStep_, 0); }
    if (code >= 0) { code = gpio.setMode(config_.pinDir_, GpioMode::output); }
    if (code >= 0) { code = gpio.write(config_.pinDir_, 0); }
    if (code < 0) {
        printf("%s ERROR: GPIO configuration failed: %d\n", ON_DUMB_DUMB_REPLACE_MOVE, code);
        result_ = code;
        return code;
    }
    pinsConfigured_ = true;
    return Gpio::SUCCESS;
}

int StepperMotorDumbDumb::replaceMove(float angle) {
    std::lock_guard lock(mutex_);
    if (!std::isfinite(angle) || config_.options_.degPulseGeared <= 0) {
        printf("%s ERROR: angle or configured degrees per pulse is invalid\n", ON_DUMB_DUMB_REPLACE_MOVE);
        return Gpio::INVALID_ARGUMENT;
    }
    const double pulses = std::round(std::abs(static_cast<double>(angle)) / config_.options_.degPulseGeared);
    if (pulses > static_cast<double>(std::numeric_limits<int64_t>::max())) {
        printf("%s ERROR: requested pulse count is out of range\n", ON_DUMB_DUMB_REPLACE_MOVE);
        return Gpio::INVALID_ARGUMENT;
    }
    const int configured = configurePinsLocked();
    if (configured < 0) { return configured; }
    const int direction = stepperMotorDirectionLevel(angle >= 0, config_.reversed_) ? 1 : 0;
    const int code = Gpio::instance().write(config_.pinDir_, direction);
    if (code < 0) {
        printf("%s ERROR: direction setup failed: %d\n", ON_DUMB_DUMB_REPLACE_MOVE, code);
        result_ = code;
        return code;
    }
    remainingPulses_ = angle >= 0 ? static_cast<int64_t>(pulses) : -static_cast<int64_t>(pulses);
    result_ = Gpio::SUCCESS;
    wake_.notify_all();
    return Gpio::SUCCESS;
}

int StepperMotorDumbDumb::updateCalibration(const stepper_motor_options_t& options, bool reversed) {
    std::lock_guard lock(mutex_);
    config_.options_ = options;
    config_.reversed_ = reversed;
    return Gpio::SUCCESS;
}

bool StepperMotorDumbDumb::hasPendingBackgroundMove() const {
    std::lock_guard lock(mutex_);
    return remainingPulses_ != 0;
}

int StepperMotorDumbDumb::backgroundMoveResult() const {
    std::lock_guard lock(mutex_);
    return result_;
}

void StepperMotorDumbDumb::runBackground(std::stop_token stop) {
    const auto interval = std::chrono::nanoseconds(std::max<duration>(MICROSECOND, config_.expecterInterval_));
    std::unique_lock lock(mutex_);
    auto next = Clock::now() + interval;
    while (!stop.stop_requested()) {
        wake_.wait_until(lock, next);
        if (stop.stop_requested()) { break; }
        const auto current = Clock::now();
        if (current < next) { continue; }
        next += interval * ((current - next) / interval + 1);
        if (remainingPulses_ == 0 || result_ < 0) { continue; }
        const int codeHigh = Gpio::instance().write(config_.pinStep_, 1);
        if (codeHigh < 0) {
            printf("%s ERROR: step HIGH failed: %d\n", ON_DUMB_DUMB_BACKGROUND, codeHigh);
            result_ = codeHigh;
            continue;
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(config_.pulseHigh_));
        const int codeLow = Gpio::instance().write(config_.pinStep_, 0);
        if (codeLow < 0) {
            printf("%s ERROR: step LOW failed: %d\n", ON_DUMB_DUMB_BACKGROUND, codeLow);
            result_ = codeLow;
            continue;
        }
        onPulse(remainingPulses_ > 0);
        remainingPulses_ += remainingPulses_ > 0 ? -1 : 1;
    }
}
