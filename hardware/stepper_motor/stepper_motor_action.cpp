#include "stepper_motor.h"
#include "hardware/gpio/gpio.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

StepperMotorAction::StepperMotorAction(const StepperMotorSeriesSequence& sequence, unsigned pinStep,
        unsigned pinDir, unsigned pinEna, const stepper_motor_options_t& options,
        duration pulseHigh, bool hardwarePwm)
    : sequence_(sequence), options_(options), pinStep_(pinStep), pinDir_(pinDir), pinEna_(pinEna),
      pulseHigh_(pulseHigh), hardwarePwm_(hardwarePwm) {
    std::string error;
    if (!optionsIsOk(options_, error)) { return; }
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
        const float speedMax = std::min(options_.speedMaxDegSec, options_.freqMax * options_.degPulse);
        acceleration = getFastestSeries(acceleration.initialSpeedDegPerSec_,
            acceleration.directionForward_ ? speedMax : -speedMax, options_, acceleration.intervalAlgorithm_);
        acceleration.pairedTargetPulses_ = target;
        acceleration.stopAfterPulses_ = target;
    }
    for (auto& series : sequence_.seq) {
        series.reset();
        series.livePwm_ = true;
        series.repeatLastInterval_ = series.stopAfterPulses_ != 0;
    }
}

StepperMotorAction::~StepperMotorAction() { (void)stop(); }

const std::string ON_STEPPER_STOP = "[StepperMotorAction.stop()]";

int StepperMotorAction::stop() {
    auto& gpio = Gpio::instance();
    int result = Gpio::SUCCESS;
    if (stepConfigured_) {
        result = gpio.setEnabled(pinStep_, false);
        if (result >= 0) { stepConfigured_ = false; }
    }
    if (enableConfigured_) {
        const int cleanup = gpio.write(pinEna_, 1);
        if (cleanup >= 0) { enableConfigured_ = false; }
        if (result >= 0) { result = cleanup; }
    }
    if (directionConfigured_) {
        const int cleanup = gpio.write(pinDir_, 0);
        if (cleanup >= 0) { directionConfigured_ = false; }
        if (result >= 0) { result = cleanup; }
    }
    if (result < 0) {
        printf("%s ERROR: GPIO cleanup failed: %d\n", ON_STEPPER_STOP.c_str(), result);
        if (status_ >= 0) { status_ = result; }
    }
    if (status_ == RUNNING) { status_ = COMPLETE; }
    return status_ < 0 ? status_ : Gpio::SUCCESS;
}

const std::string ON_STEPPER_ACTION = "[StepperMotorAction.action()]";

int StepperMotorAction::action(moment at) {
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

const std::string ON_STEPPER_RUN = "[StepperMotorAction.run()]";

int StepperMotorAction::run(duration expecterInterval, duration timeLimit) {
    const duration maximum = static_cast<duration>(std::numeric_limits<int64_t>::max() / 2);
    if (timeLimit == 0 || timeLimit > maximum || expecterInterval > maximum) {
        status_ = Gpio::INVALID_ARGUMENT;
        sequence_.error = ON_STEPPER_RUN + " invalid timer or time limit";
        printf("%s ERROR: invalid timer or time limit\n", ON_STEPPER_RUN.c_str());
        return stop();
    }
    const auto origin = std::chrono::steady_clock::now();
    const auto deadline = origin + std::chrono::nanoseconds(timeLimit);
    const auto tick = std::chrono::nanoseconds(std::max<duration>(MICROSECOND, expecterInterval));
    auto next = origin;
    for (;;) {
        const auto current = std::chrono::steady_clock::now();
        if (current >= deadline) {
            status_ = TIME_LIMIT;
            sequence_.error = ON_STEPPER_RUN + " time limit exceeded";
            printf("%s ERROR: time limit exceeded\n", ON_STEPPER_RUN.c_str());
            return stop();
        }
        const moment at = std::chrono::duration_cast<std::chrono::nanoseconds>(current.time_since_epoch()).count();
        const int code = action(at);
        if (code != RUNNING) { return code < 0 ? code : Gpio::SUCCESS; }
        next += tick;
        const auto after = std::chrono::steady_clock::now();
        if (next < after) { next += tick * ((after - next) / tick + 1); }
        std::this_thread::sleep_until(std::min(next, deadline));
    }
}

constexpr const char* ON_RUN_MOTOR = "[stepper_motor.run()]";

int stepperMotorRun(const StepperMotorRunConfig& cfg, float rotationDeg,
        const std::string& label, StepperMotorSeriesSequence* real) {
    if (real) { *real = {}; }
    const auto fail = [&](int code, const std::string& message) {
        const std::string error = std::string(ON_RUN_MOTOR) + " " + label + ": " + message;
        printf("%s ERROR: %s: %s (code %d)\n", ON_RUN_MOTOR, label.c_str(), message.c_str(), code);
        if (real && real->error.empty()) { real->error = error; }
        fflush(stdout);
        return code;
    };
    std::string error;
    if (!optionsIsOk(cfg.options_, error)) { return fail(Gpio::INVALID_ARGUMENT, error); }
    const duration maximum = static_cast<duration>(std::numeric_limits<int64_t>::max() / 2);
    if (!std::isfinite(rotationDeg) || cfg.timeLimit_ == 0 || cfg.timeLimit_ > maximum ||
            cfg.expecterInterval_ > maximum || cfg.pulseHigh_ == 0 || cfg.pulseHigh_ > SECOND / 2 ||
            cfg.pinStep_ >= Gpio::PIN_COUNT || cfg.pinDir_ >= Gpio::PIN_COUNT || cfg.pinEna_ >= Gpio::PIN_COUNT ||
            cfg.pinStep_ == cfg.pinDir_ || cfg.pinStep_ == cfg.pinEna_ || cfg.pinDir_ == cfg.pinEna_) {
        return fail(Gpio::INVALID_ARGUMENT, "invalid angle, timing or pins");
    }
    const std::string prefix = "[" + label + "]";
    printf("\n%s Target: %.3f deg; timer: %.3f ms; real uses runtime PWM estimates\n",
        prefix.c_str(), rotationDeg, static_cast<double>(cfg.expecterInterval_) / MILLISECOND);
    fflush(stdout);
    if (rotationDeg == 0) {
        printf("%s No movement: zero angle\n", prefix.c_str());
        fflush(stdout);
        return Gpio::SUCCESS;
    }
    const auto clocked = getSeriesSequence(0, rotationDeg, 0, cfg.expecterInterval_, cfg.options_);
    if (!clocked.error.empty()) { return fail(Gpio::INVALID_ARGUMENT, clocked.error); }
    const auto ideal = getSeriesSequence(0, rotationDeg, 0, 0, cfg.options_);
    if (!ideal.error.empty()) { return fail(Gpio::INVALID_ARGUMENT, ideal.error); }
    ideal.log(cfg.options_, (prefix + " ideal").c_str(), cfg.verbose_);
    clocked.log(cfg.options_, (prefix + " clocked").c_str(), cfg.verbose_);
    fflush(stdout);

    auto& gpio = Gpio::instance();
    const int initialized = gpio.initialize();
    if (initialized < 0) { return fail(initialized, "GPIO initialization failed"); }
    StepperMotorSeriesSequence observed;
    int result;
    {
        StepperMotorAction motor(clocked, cfg.pinStep_, cfg.pinDir_, cfg.pinEna_,
            cfg.options_, cfg.pulseHigh_, cfg.hardwarePwm_);
        result = motor.run(cfg.expecterInterval_, cfg.timeLimit_);
        observed = motor.result();
    }
    if (real) { *real = observed; }
    observed.log(cfg.options_, (prefix + " real").c_str(), cfg.verbose_);
    if (result < 0) { (void)fail(result, "motion failed"); }
    const int terminated = gpio.terminate();
    if (terminated < 0) {
        (void)fail(terminated, "GPIO termination failed");
        if (result >= 0) { result = terminated; }
    }
    printf("%s Motion finished; result: %d\n", prefix.c_str(), result);
    fflush(stdout);
    return result;
}
