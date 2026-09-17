#include "stepper_motor_dumb.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

StepperMotorDumb::StepperMotorDumb(const StepperMotorRunConfig& config, float speedDegPerSec,
        duration pauseAfterSeries, uint64_t remainingPulsesTolerance)
    : StepperMotor(config), speedDegPerSec_(speedDegPerSec), pauseAfterSeries_(pauseAfterSeries),
      remainingPulsesTolerance_(remainingPulsesTolerance) {}

constexpr const char* ON_DUMB_PLAN_SEQUENCE = "[StepperMotorDumb.planSequence()]";

StepperMotorSeriesSequence StepperMotorDumb::planSequence(float angle, duration expecterInterval) const {
    StepperMotorSeriesSequence result;
    const auto fail = [&](const char* detail) {
        result.error = std::string(ON_DUMB_PLAN_SEQUENCE) + " " + detail;
        return result;
    };
    if (!std::isfinite(angle) || !std::isfinite(speedDegPerSec_) || speedDegPerSec_ <= 0 ||
            !std::isfinite(config_.options_.degPulse) || config_.options_.degPulse <= 0 ||
            config_.pulseHigh_ == 0 || pauseAfterSeries_ >= config_.timeLimit_) {
        return fail("invalid angle, speed, pulse width or pause");
    }
    const double rawPulses = std::abs(static_cast<double>(angle)) / config_.options_.degPulse;
    if (rawPulses >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
        return fail("requested pulse count is out of range");
    }
    const uint64_t targetPulses = std::llround(rawPulses);
    if (targetPulses == 0) { return result; }
    const uint64_t tolerance = std::min(remainingPulsesTolerance_, targetPulses - 1);
    const duration tick = std::max<duration>(MICROSECOND, expecterInterval);
    // Crossing the completion threshold may add at most tolerance + 1 pulses
    // before the next expected timer observation.
    const double timerLimit = static_cast<double>(tolerance + 1) * SECOND / tick;
    const double maxFrequency = std::min({10000.0, static_cast<double>(config_.options_.freqMax),
        static_cast<double>(config_.options_.speedMaxDegSec) / config_.options_.degPulse,
        static_cast<double>(speedDegPerSec_) / config_.options_.degPulse,
        static_cast<double>(SECOND) / (2.0 * config_.pulseHigh_), timerLimit});
    if (!std::isfinite(maxFrequency) || maxFrequency < 1) {
        return fail("no valid integer PWM frequency satisfies the limits and timer tolerance");
    }
    unsigned frequency = static_cast<unsigned>(std::floor(maxFrequency));
    while (frequency > 0) {
        const duration period = std::llround(static_cast<double>(SECOND) / frequency);
        if (static_cast<long double>(period) * (tolerance + 1) >= tick) { break; }
        --frequency;
    }
    if (frequency == 0) { return fail("no PWM period fits the timer tolerance"); }
    const float speed = static_cast<float>(frequency * config_.options_.degPulse) * (angle >= 0 ? 1.0F : -1.0F);
    StepperMotorSeries series(targetPulses, speed, speed, angle >= 0, config_.options_,
        LINEAR_INTERVAL_ACCELERATION);
    series.livePwm_ = true;
    series.cruiseFrequency_ = frequency;
    result.seq.push_back(evaluateSeries(series, tick, config_.options_, 0, targetPulses - tolerance));
    result.seq.back().maxAccelerationDegSec2_ = 0;
    return result;
}

void StepperMotorDumb::prepareSequence() {
    waitingAfterSeries_ = false;
    pauseStartedAt_ = 0;
    for (auto& series : sequence_.seq) {
        series.reset();
        series.livePwm_ = true;
    }
}

constexpr const char* ON_DUMB_ACTION = "[StepperMotorDumb.action()]";

int StepperMotorDumb::action(moment at) {
    if (status_ != RUNNING) { return status_; }
    if (!initialized_) { return initialize(at); }
    if (at <= lastAt_) { return RUNNING; }
    lastAt_ = at;
    if (waitingAfterSeries_) {
        if (at - pauseStartedAt_ < pauseAfterSeries_) { return RUNNING; }
        (void)stop();
        return status_;
    }
    if (at < readyAt_) { return RUNNING; }
    const auto fail = [&](int code) {
        status_ = code;
        sequence_.error = std::string(ON_DUMB_ACTION) + " execution failed: " + std::to_string(code);
        printf("%s ERROR: GPIO operation or motor configuration failed: %d\n", ON_DUMB_ACTION, code);
        (void)stop();
        return status_;
    };
    auto& series = sequence_.seq.front();
    if (!series.started_) { series.setupDelay_ = at - phaseStartedAt_; }
    const float interval = series.intervalSec(at, config_.options_);
    series.maxAccelerationDegSec2_ = 0;
    const uint64_t tolerance = std::min(remainingPulsesTolerance_, series.expectedPulsesCount_ - 1);
    if (series.pulsesCount_ >= series.expectedPulsesCount_ - tolerance) {
        series.finished_ = true;
        const int code = Gpio::instance().setEnabled(config_.pinStep_, false);
        if (code < 0) { return fail(code); }
        frequency_ = 0;
        waitingAfterSeries_ = true;
        pauseStartedAt_ = at;
        if (pauseAfterSeries_ == 0) { (void)stop(); }
        return status_;
    }
    if (!(interval > 0)) { return fail(Gpio::INVALID_ARGUMENT); }
    if (frequency_ == 0) {
        auto& gpio = Gpio::instance();
        int code = gpio.setFrequency(config_.pinStep_, series.cruiseFrequency_);
        if (code >= 0) { code = gpio.setDuty(config_.pinStep_, 20000); }
        if (code >= 0) { code = gpio.setEnabled(config_.pinStep_, true); }
        if (code < 0) { return fail(code); }
        frequency_ = series.cruiseFrequency_;
    }
    return RUNNING;
}
