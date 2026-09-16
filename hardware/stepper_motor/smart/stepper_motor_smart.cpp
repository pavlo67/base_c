#include "stepper_motor_smart.h"
#include "hardware/gpio/gpio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

const std::string ON_STEPPER_ACTION = "[StepperMotorSmart.action()]";

int StepperMotorSmart::action(moment at) {
    if (status_ != RUNNING) { return status_; }
    const auto fail = [&](int code) {
        status_ = code;
        sequence_.error = "execution failed: " + std::to_string(code);
        printf("%s ERROR: GPIO operation or motor configuration failed: %d\n", ON_STEPPER_ACTION.c_str(), code);
        (void)stop();
        return status_;
    };
    if (hasMoment_ && at <= lastAt_) { return RUNNING; }
    hasMoment_ = true;
    lastAt_ = at;
    auto& gpio = Gpio::instance();
    if (!initialized_) {
        std::string error;
        if (pinStep_ >= Gpio::PIN_COUNT || pinDir_ >= Gpio::PIN_COUNT || pinEna_ >= Gpio::PIN_COUNT ||
                pinStep_ == pinDir_ || pinStep_ == pinEna_ || pinDir_ == pinEna_ || pulseHigh_ == 0 ||
                pulseHigh_ > SECOND / 2 || !sequence_.error.empty() || !optionsIsOk(options_, error)) {
            return fail(Gpio::INVALID_ARGUMENT);
        }
        if (sequence_.seq.empty()) { status_ = COMPLETE; return COMPLETE; }
        if (hardwarePwm_) {
            const int channel = gpio.hardwarePwmChannel(pinStep_);
            if (channel < 0) { return fail(channel); }
        }
        int code = gpio.setMode(pinEna_, GpioMode::output);
        enableConfigured_ = code >= 0;
        if (code >= 0) { code = gpio.write(pinEna_, 1); }
        if (code >= 0) {
            code = gpio.setMode(pinStep_, hardwarePwm_ ? GpioMode::hardwarePwm : GpioMode::output);
            stepConfigured_ = code >= 0;
        }
        if (code >= 0) { code = gpio.setEnabled(pinStep_, false); }
        if (code >= 0) { code = gpio.setDuty(pinStep_, 0); }
        if (code >= 0) { code = gpio.setRange(pinStep_, 40000); }
        if (code >= 0) {
            code = gpio.setMode(pinDir_, GpioMode::output);
            directionConfigured_ = code >= 0;
        }
        if (code >= 0) { code = gpio.write(pinDir_, sequence_.seq.front().directionForward_ ? 1 : 0); }
        if (code >= 0) { code = gpio.write(pinEna_, 0); }
        if (code < 0) { return fail(code); }
        initialized_ = true;
        phaseStartedAt_ = at;
        readyAt_ = at + 500 * MICROSECOND + pulseHigh_;
        return RUNNING;
    }
    if (at < readyAt_) { return RUNNING; }

    // State transitions and series parameters belong here, never in run().
    while (index_ < sequence_.seq.size()) {
        auto& series = sequence_.seq[index_];
        size_t brakingIndex = index_ + 1;
        if (brakingIndex < sequence_.seq.size() && sequence_.seq[brakingIndex].accelerationDegPerSec2_ == 0 &&
                !sequence_.seq[brakingIndex].terminalInterval_) { ++brakingIndex; }
        const bool predictive = series.accelerationDegPerSec2_ > 0 && series.pairedTargetPulses_ &&
            brakingIndex < sequence_.seq.size() && sequence_.seq[brakingIndex].accelerationDegPerSec2_ < 0;
        duration modelInterval = 0;
        if (!series.started_) {
            series.setupDelay_ = at - phaseStartedAt_;
            accelerationTicks_ = 0;
        } else if (predictive) {
            ++accelerationTicks_;
            modelInterval = std::max<duration>(1, (at - series.startedAt_) / accelerationTicks_);
        }
        const auto before = predictive ? series : StepperMotorSeries(0, 0, 0, true, options_);
        const float interval = series.intervalSec(at, options_);
        bool threshold = series.stopAfterPulses_ && series.pulsesCount_ >= series.stopAfterPulses_;
        if (predictive && modelInterval && interval > 0) {
            auto nextTick = series;
            const float nextInterval = nextTick.intervalSec(at + modelInterval, options_);
            const uint64_t remaining = series.pairedTargetPulses_ > nextTick.pulsesCount_
                ? series.pairedTargetPulses_ - nextTick.pulsesCount_ : 0;
            const float nextSpeed = nextInterval > 0 ? (series.directionForward_ ? 1.0F : -1.0F) *
                options_.degPulse / nextInterval : 0;
            if (nextInterval <= 0 || !canBrake(nextSpeed, sequence_.seq[brakingIndex].idealFinalSpeed(options_),
                    modelInterval, remaining, options_, series.intervalAlgorithm_)) {
                // Count this observation at the actual old PWM command; the proposed
                // acceleration command has not yet been sent to GPIO.
                series = before;
                series.recalculateAfter_ = at + 1;
                (void)series.intervalSec(at, options_);
                threshold = true;
            }
        }
        if (interval == 0 || threshold) {
            if (interval == 0 && series.intervalIndex_ < series.expectedPulsesCount_) {
                return fail(Gpio::INVALID_ARGUMENT); // Unrepresentable period, not normal completion.
            }
            series.finished_ = true;
            int code = gpio.setEnabled(pinStep_, false);
            if (code < 0) { return fail(code); }
            frequency_ = 0;
            ++index_;
            if (index_ == sequence_.seq.size()) {
                (void)stop();
                return status_;
            }
            // Replacing the planned tail may reallocate the sequence.
            const auto completed = series;
            if (predictive) {
                const auto braking = sequence_.seq[brakingIndex];
                const float targetSpeed = braking.idealFinalSpeed(options_);
                const uint64_t target = completed.pairedTargetPulses_;
                const uint64_t remaining = target > completed.pulsesCount_ ? target - completed.pulsesCount_ : 0;
                auto model = getBrakingModel((completed.directionForward_ ? 1.0F : -1.0F) *
                    options_.degPulse * SECOND / completed.activeInterval_, targetSpeed,
                    modelInterval, options_, braking.intervalAlgorithm_);
                if (remaining && model.brakingModel_.empty()) { return fail(Gpio::INVALID_ARGUMENT); }
                if (remaining) {
                    // Fit the frozen pulse profile to the remaining integer distance.
                    // No slow final-period completion tail is needed.
                    for (auto& point : model.brakingModel_) {
                        point.pulses_ = static_cast<uint64_t>(static_cast<long double>(point.pulses_) *
                            remaining / model.expectedPulsesCount_);
                    }
                    model.expectedPulsesCount_ = remaining;
                }
                sequence_.seq.erase(sequence_.seq.begin() + index_, sequence_.seq.begin() + brakingIndex + 1);
                if (remaining) { sequence_.seq.insert(sequence_.seq.begin() + index_, model); }
                if (index_ == sequence_.seq.size()) { (void)stop(); return status_; }
            }
            auto& next = sequence_.seq[index_];
            next.initialPulseInterval_ = completed.lastInterval_;
            phaseStartedAt_ = at;
            code = gpio.write(pinDir_, next.directionForward_ ? 1 : 0);
            if (code < 0) { return fail(code); }
            if (completed.directionForward_ != next.directionForward_) {
                readyAt_ = at + pulseHigh_;
                return RUNNING;
            }
            continue;
        }
        const unsigned frequency = static_cast<unsigned>(std::llround(1.0 / interval));
        // 50% duty keeps both HIGH and LOW at least pulseHigh long.
        if (frequency == 0 || frequency > 10000 ||
                static_cast<double>(frequency) * pulseHigh_ > static_cast<double>(SECOND) / 2) {
            return fail(Gpio::INVALID_ARGUMENT);
        }
        if (frequency != frequency_) {
            int code = gpio.setFrequency(pinStep_, frequency);
            if (code >= 0 && frequency_ == 0) { code = gpio.setDuty(pinStep_, 20000); }
            if (code >= 0 && frequency_ == 0) { code = gpio.setEnabled(pinStep_, true); }
            if (code < 0) { return fail(code); }
            frequency_ = frequency;
        }
        return RUNNING;
    }
    (void)stop();
    return status_;
}

