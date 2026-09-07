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
    for (auto& series : sequence_.seq) {
        series.reset();
        series.livePwm_ = true;
        series.repeatLastInterval_ = series.stopAfterPulses_ != 0;
    }
}

StepperMotorAction::~StepperMotorAction() { (void)stop(); }

const std::string ON_STEPPER_STOP = "on StepperMotorAction.stop(): ";

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
        printf("ERROR: %sGPIO cleanup failed: %d\n", ON_STEPPER_STOP.c_str(), result);
        if (status_ >= 0) { status_ = result; }
    }
    if (status_ == RUNNING) { status_ = COMPLETE; }
    return status_ < 0 ? status_ : Gpio::SUCCESS;
}

const std::string ON_STEPPER_ACTION = "on StepperMotorAction.action(): ";

int StepperMotorAction::action(moment at) {
    if (status_ != RUNNING) { return status_; }
    const auto fail = [&](int code) {
        status_ = code;
        sequence_.error = "execution failed: " + std::to_string(code);
        printf("ERROR: %sGPIO operation or motor configuration failed: %d\n", ON_STEPPER_ACTION.c_str(), code);
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
        if (!series.started_) { series.setupDelay_ = at - phaseStartedAt_; }
        const float interval = series.intervalSec(at, options_);
        const bool threshold = series.stopAfterPulses_ && series.pulsesCount_ >= series.stopAfterPulses_;
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
            auto& next = sequence_.seq[index_];
            if (series.stopAfterPulses_ && next.accelerationDegPerSec2_ < 0) {
                // Scheduler delay may change the speed reached at the halfway tick.
                const float targetSpeed = next.terminalInterval_ ? 0 : next.idealFinalSpeed(options_);
                const duration terminal = next.terminalInterval_;
                const uint64_t target = series.stopAfterPulses_ + next.minimumPulsesCount_;
                const uint64_t remaining = target > series.pulsesCount_ ? target - series.pulsesCount_ : 0;
                next = getFastestSeries(series.finalSpeed(options_), targetSpeed, options_, next.intervalAlgorithm_);
                next.minimumPulsesCount_ = remaining;
                next.terminalInterval_ = terminal;
                next.livePwm_ = true;
            }
            next.initialPulseInterval_ = series.lastInterval_;
            phaseStartedAt_ = at;
            code = gpio.write(pinDir_, next.directionForward_ ? 1 : 0);
            if (code < 0) { return fail(code); }
            if (series.directionForward_ != next.directionForward_) {
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

const std::string ON_STEPPER_RUN = "on StepperMotorAction.run(): ";

int StepperMotorAction::run(duration expecterInterval, duration timeLimit) {
    const duration maximum = static_cast<duration>(std::numeric_limits<int64_t>::max() / 2);
    if (timeLimit == 0 || timeLimit > maximum || expecterInterval > maximum) {
        status_ = Gpio::INVALID_ARGUMENT;
        sequence_.error = ON_STEPPER_RUN + "invalid timer or time limit";
        printf("ERROR: %sinvalid timer or time limit\n", ON_STEPPER_RUN.c_str());
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
            sequence_.error = ON_STEPPER_RUN + "time limit exceeded";
            printf("ERROR: %stime limit exceeded\n", ON_STEPPER_RUN.c_str());
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

int run(const StepperMotorSeriesSequence& sequence, unsigned pinStep, unsigned pinDir, unsigned pinEna,
        duration expecterInterval, const stepper_motor_options_t& stepperOpts, duration pulseHigh,
        bool hardwarePwm, duration timeLimit, StepperMotorSeriesSequence* real) {
    StepperMotorAction motor(sequence, pinStep, pinDir, pinEna, stepperOpts, pulseHigh, hardwarePwm);
    const int code = motor.run(expecterInterval, timeLimit);
    if (real) { *real = motor.result(); }
    return code;
}
