#include "stepper_motor_smart.h"
#include "hardware/gpio/gpio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

StepperMotorSmart::StepperMotorSmart(const StepperMotorRunConfig& config)
    : StepperMotor(config) {}

StepperMotorSmart::StepperMotorSmart(const StepperMotorRunConfig& config,
        const StepperMotorSeriesSequence& sequence)
    : StepperMotor(config) {
    setSequence(sequence);
}

StepperMotorSmart::StepperMotorSmart(const StepperMotorSeriesSequence& sequence, unsigned pinStep,
        unsigned pinDir, unsigned pinEna, const stepper_motor_options_t& options,
        duration pulseHigh, bool hardwarePwm)
    : StepperMotorSmart(StepperMotorRunConfig{.pinStep_ = pinStep, .pinDir_ = pinDir,
          .pinEna_ = pinEna, .options_ = options, .pulseHigh_ = pulseHigh,
          .hardwarePwm_ = hardwarePwm}, sequence) {}

void StepperMotorSmart::prepareSequence() {
    std::string error;
    if (!optionsIsOk(config_.options_, error)) { return; }
    for (size_t i = 0; i < sequence_.seq.size(); ++i) {
        auto& acceleration = sequence_.seq[i];
        if (acceleration.accelerationDegPerSec2_ <= 0) { continue; }
        size_t braking = i + 1;
        if (braking < sequence_.seq.size() && sequence_.seq[braking].accelerationDegPerSec2_ == 0 &&
                !sequence_.seq[braking].terminalInterval_) { ++braking; }
        if (braking >= sequence_.seq.size() || sequence_.seq[braking].accelerationDegPerSec2_ >= 0 ||
                sequence_.seq[braking].directionForward_ != acceleration.directionForward_) { continue; }
        uint64_t target = acceleration.pairedTargetPulses_;
        if (!target) {
            for (size_t j = i; j <= braking; ++j) { target += sequence_.seq[j].expectedPulsesCount_; }
        }
        const float speedMax = std::min(config_.options_.speedMaxDegSec,
            config_.options_.freqMax * config_.options_.degPulse);
        acceleration = getFastestSeries(acceleration.initialSpeedDegPerSec_,
            acceleration.directionForward_ ? speedMax : -speedMax,
            config_.options_, acceleration.intervalAlgorithm_);
        acceleration.pairedTargetPulses_ = target;
        acceleration.stopAfterPulses_ = target;
    }
    for (auto& series : sequence_.seq) {
        series.reset();
        series.livePwm_ = true;
        series.repeatLastInterval_ = series.stopAfterPulses_ != 0;
    }
}

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
    if (!initialized_) { return initialize(at); }
    if (at <= lastAt_) { return RUNNING; }
    lastAt_ = at;
    auto& gpio = Gpio::instance();
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
        const auto before = predictive ? series : StepperMotorSeries(0, 0, 0, true, config_.options_);
        const float interval = series.intervalSec(at, config_.options_);
        bool threshold = series.stopAfterPulses_ && series.pulsesCount_ >= series.stopAfterPulses_;
        if (predictive && modelInterval && interval > 0) {
            auto nextTick = series;
            const float nextInterval = nextTick.intervalSec(at + modelInterval, config_.options_);
            const uint64_t remaining = series.pairedTargetPulses_ > nextTick.pulsesCount_
                ? series.pairedTargetPulses_ - nextTick.pulsesCount_ : 0;
            const float nextSpeed = nextInterval > 0 ? (series.directionForward_ ? 1.0F : -1.0F) *
                config_.options_.degPulse / nextInterval : 0;
            if (nextInterval <= 0 || !canBrake(nextSpeed, sequence_.seq[brakingIndex].idealFinalSpeed(config_.options_),
                    modelInterval, remaining, config_.options_, series.intervalAlgorithm_)) {
                // Count this observation at the actual old PWM command; the proposed
                // acceleration command has not yet been sent to GPIO.
                series = before;
                series.recalculateAfter_ = at + 1;
                (void)series.intervalSec(at, config_.options_);
                threshold = true;
            }
        }
        if (interval == 0 || threshold) {
            if (interval == 0 && series.intervalIndex_ < series.expectedPulsesCount_) {
                return fail(Gpio::INVALID_ARGUMENT); // Unrepresentable period, not normal completion.
            }
            series.finished_ = true;
            int code = gpio.setEnabled(config_.pinStep_, false);
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
                const float targetSpeed = braking.idealFinalSpeed(config_.options_);
                const uint64_t target = completed.pairedTargetPulses_;
                const uint64_t remaining = target > completed.pulsesCount_ ? target - completed.pulsesCount_ : 0;
                auto model = getBrakingModel((completed.directionForward_ ? 1.0F : -1.0F) *
                    config_.options_.degPulse * SECOND / completed.activeInterval_, targetSpeed,
                    modelInterval, config_.options_, braking.intervalAlgorithm_);
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
            code = gpio.write(config_.pinDir_, next.directionForward_ ? 1 : 0);
            if (code < 0) { return fail(code); }
            if (completed.directionForward_ != next.directionForward_) {
                readyAt_ = at + config_.pulseHigh_;
                return RUNNING;
            }
            continue;
        }
        const unsigned frequency = static_cast<unsigned>(std::llround(1.0 / interval));
        // 50% duty keeps both HIGH and LOW at least pulseHigh long.
        if (frequency == 0 || frequency > 10000 ||
                static_cast<double>(frequency) * config_.pulseHigh_ > static_cast<double>(SECOND) / 2) {
            return fail(Gpio::INVALID_ARGUMENT);
        }
        if (frequency != frequency_) {
            int code = gpio.setFrequency(config_.pinStep_, frequency);
            if (code >= 0 && frequency_ == 0) { code = gpio.setDuty(config_.pinStep_, 20000); }
            if (code >= 0 && frequency_ == 0) { code = gpio.setEnabled(config_.pinStep_, true); }
            if (code < 0) { return fail(code); }
            frequency_ = frequency;
        }
        return RUNNING;
    }
    (void)stop();
    return status_;
}


bool StepperMotorSmart::canBrake(float speedDegPerSec, float finalSpeedDegPerSec, duration modelInterval,
        uint64_t remainingPulses, const stepper_motor_options_t& options, stepper_motor_algorithm_t algorithm) {
    const auto model = getBrakingModel(speedDegPerSec, finalSpeedDegPerSec, modelInterval, options, algorithm);
    return !model.brakingModel_.empty() && model.expectedPulsesCount_ <= remainingPulses;
}

StepperMotorSeries StepperMotorSmart::getBrakingModel(float speedDegPerSec, float finalSpeedDegPerSec,
        duration modelInterval, const stepper_motor_options_t& options, stepper_motor_algorithm_t algorithm) {
    StepperMotorSeries empty(0, 0, 0, speedDegPerSec >= 0, options, algorithm);
    std::string error;
    if (!optionsIsOk(options, error) || !std::isfinite(speedDegPerSec) ||
            !std::isfinite(finalSpeedDegPerSec) || modelInterval == 0 ||
            std::abs(speedDegPerSec) < std::abs(finalSpeedDegPerSec) ||
            speedDegPerSec * finalSpeedDegPerSec < 0) { return empty; }
    auto modeled = getFastestSeries(speedDegPerSec, finalSpeedDegPerSec, options, algorithm);
    modeled.livePwm_ = true;
    auto result = modeled;
    float interval = modeled.intervalSec(0, options);
    if (!(interval > 0)) { return empty; }
    result.brakingModel_.push_back({0, modeled.activeInterval_});
    moment at = 0;
    while (interval > 0) {
        // Skip timer calls which cannot change the model's command.
        const duration wait = modeled.recalculateAfter_ > at ? modeled.recalculateAfter_ - at : 1;
        const uint64_t ticks = (wait - 1) / modelInterval + 1;
        if (ticks > (std::numeric_limits<moment>::max() - at) / modelInterval) { return empty; }
        at += ticks * modelInterval;
        interval = modeled.intervalSec(at, options);
        if (interval > 0 && modeled.activeInterval_ != result.brakingModel_.back().interval_) {
            result.brakingModel_.push_back({modeled.pulsesCount_, modeled.activeInterval_});
        }
    }
    if (modeled.intervalIndex_ < modeled.expectedPulsesCount_ || modeled.pulsesCount_ == 0) { return empty; }
    result.expectedPulsesCount_ = modeled.pulsesCount_;
    result.modelInterval_ = modelInterval;
    result.brakingFinalSpeed_ = finalSpeedDegPerSec;
    return result;
}
