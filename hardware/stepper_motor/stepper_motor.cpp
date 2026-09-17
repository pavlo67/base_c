#include "stepper_motor.h"
#include "hardware/gpio/gpio.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <thread>

StepperMotor::StepperMotor(const StepperMotorRunConfig& config) : config_(config) {}

StepperMotor::~StepperMotor() { (void)stop(); }

const std::string ON_STEPPER_STOP = "[StepperMotor.stop()]";

int StepperMotor::stop() {
    auto& gpio = Gpio::instance();
    int result = Gpio::SUCCESS;
    if (stepConfigured_) {
        result = gpio.setEnabled(config_.pinStep_, false);
        if (result >= 0) { stepConfigured_ = false; }
    }
    if (enableConfigured_) {
        const int cleanup = gpio.write(config_.pinEna_, 1);
        if (cleanup >= 0) { enableConfigured_ = false; }
        if (result >= 0) { result = cleanup; }
    }
    if (directionConfigured_) {
        const int cleanup = gpio.write(config_.pinDir_, 0);
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

std::mutex& stepperMotorExecutionMutex() {
    static std::mutex executionMutex;
    return executionMutex;
}

void StepperMotor::setSequence(const StepperMotorSeriesSequence& sequence) {
    sequence_ = sequence;
    prepareSequence();
    initialized_ = false;
    lastAt_ = 0;
    accelerationTicks_ = 0;
    readyAt_ = 0;
    phaseStartedAt_ = 0;
    index_ = 0;
    status_ = RUNNING;
    frequency_ = 0;
    started_ = false;
}

StepperMotorSeriesSequence StepperMotor::planSequence(float angle, duration expecterInterval) const {
    return getSeriesSequence(0, angle, 0, expecterInterval, config_.options_);
}

constexpr const char* ON_STEPPER_INITIALIZE = "[StepperMotor.initialize()]";

int StepperMotor::initialize(moment at) {
    lastAt_ = at;
    const auto fail = [&](int code) {
        status_ = code;
        sequence_.error = "execution failed: " + std::to_string(code);
        printf("%s ERROR: GPIO operation or motor configuration failed: %d\n", ON_STEPPER_INITIALIZE, code);
        (void)stop();
        return status_;
    };
    auto& gpio = Gpio::instance();
    std::string error;
    if (config_.pinStep_ >= Gpio::PIN_COUNT || config_.pinDir_ >= Gpio::PIN_COUNT || config_.pinEna_ >= Gpio::PIN_COUNT ||
            config_.pinStep_ == config_.pinDir_ || config_.pinStep_ == config_.pinEna_ || config_.pinDir_ == config_.pinEna_ || config_.pulseHigh_ == 0 ||
            config_.pulseHigh_ > SECOND / 2 || !sequence_.error.empty() || !optionsIsOk(config_.options_, error)) {
        return fail(Gpio::INVALID_ARGUMENT);
    }
    if (sequence_.seq.empty()) { status_ = COMPLETE; return COMPLETE; }
    if (config_.hardwarePwm_) {
        const int channel = gpio.hardwarePwmChannel(config_.pinStep_);
        if (channel < 0) { return fail(channel); }
    }
    int code = gpio.setMode(config_.pinEna_, GpioMode::output);
    enableConfigured_ = code >= 0;
    if (code >= 0) { code = gpio.write(config_.pinEna_, 1); }
    if (code >= 0) {
        code = gpio.setMode(config_.pinStep_, config_.hardwarePwm_ ? GpioMode::hardwarePwm : GpioMode::output);
        stepConfigured_ = code >= 0;
    }
    if (code >= 0) { code = gpio.setEnabled(config_.pinStep_, false); }
    if (code >= 0) { code = gpio.setDuty(config_.pinStep_, 0); }
    if (code >= 0) { code = gpio.setRange(config_.pinStep_, 40000); }
    if (code >= 0) {
        code = gpio.setMode(config_.pinDir_, GpioMode::output);
        directionConfigured_ = code >= 0;
    }
    if (code >= 0) { code = gpio.write(config_.pinDir_, sequence_.seq.front().directionForward_ ? 1 : 0); }
    if (code >= 0) { code = gpio.write(config_.pinEna_, 0); }
    if (code < 0) { return fail(code); }
    initialized_ = true;
    phaseStartedAt_ = at;
    readyAt_ = at + 500 * MICROSECOND + config_.pulseHigh_;
    return RUNNING;
}

constexpr const char* ON_STEPPER_PREPARE = "[StepperMotor.prepare()]";

int StepperMotor::prepare(float angle, std::array<bool, Gpio::PIN_COUNT>& usedPins) {
    const duration maximum = std::numeric_limits<int64_t>::max() / 2;
    std::string error;
    if (!optionsIsOk(config_.options_, error) || !std::isfinite(angle) ||
            config_.timeLimit_ == 0 || config_.timeLimit_ > maximum ||
            config_.expecterInterval_ > maximum || config_.pulseHigh_ == 0 || config_.pulseHigh_ > SECOND / 2) {
        printf("%s ERROR: invalid motor options, angle or timing: %s\n", ON_STEPPER_PREPARE, error.c_str());
        return Gpio::INVALID_ARGUMENT;
    }
    for (const auto pin : {config_.pinStep_, config_.pinDir_, config_.pinEna_}) {
        if (pin >= Gpio::PIN_COUNT || usedPins[pin]) {
            printf("%s ERROR: pins must be valid and distinct across both motors\n", ON_STEPPER_PREPARE);
            return Gpio::INVALID_ARGUMENT;
        }
        usedPins[pin] = true;
    }
    if (angle == 0) { status_ = COMPLETE; return COMPLETE; }
    const auto plan = planSequence(angle, config_.expecterInterval_);
    if (!plan.error.empty()) {
        printf("%s ERROR: planning failed: %s\n", ON_STEPPER_PREPARE, plan.error.c_str());
        return Gpio::INVALID_ARGUMENT;
    }
    setSequence(plan);
    tick_ = std::chrono::nanoseconds(std::max<duration>(MICROSECOND, config_.expecterInterval_));
    return RUNNING;
}

constexpr const char* ON_STEPPER_UPDATE = "[StepperMotor.update()]";

int StepperMotor::update() {
    if (status_ != RUNNING) { return status_; }
    const auto current = Clock::now();
    if (!started_) {
        next_ = current;
        deadline_ = current + std::chrono::nanoseconds(config_.timeLimit_);
        started_ = true;
    }
    if (current >= deadline_) {
        const int cleanup = stop();
        status_ = cleanup < 0 ? cleanup : TIME_LIMIT;
        printf("%s ERROR: motion failed (code %d)\n", ON_STEPPER_UPDATE, status_);
        return status_;
    }
    if (current < next_) { return status_; }
    const moment at = std::chrono::duration_cast<std::chrono::nanoseconds>(current.time_since_epoch()).count();
    status_ = action(at);
    next_ += tick_;
    const auto after = Clock::now();
    if (next_ < after) { next_ += tick_ * ((after - next_) / tick_ + 1); }
    return status_;
}

constexpr const char* ON_RUN_REAL = "[StepperMotor.runReal()]";

int StepperMotor::runReal(const std::string& label, StepperMotorSeriesSequence* real) {
    const auto& cfg = config_;
    const auto origin = std::chrono::steady_clock::now();
    const auto deadline = origin + std::chrono::nanoseconds(cfg.timeLimit_);
    const auto tick = std::chrono::nanoseconds(std::max<duration>(MICROSECOND, cfg.expecterInterval_));
    auto next = origin;
    int result;
    for (;;) {
        const auto current = std::chrono::steady_clock::now();
        if (current >= deadline) {
            status_ = TIME_LIMIT;
            sequence_.error = std::string(ON_RUN_REAL) + " time limit exceeded";
            printf("%s ERROR: time limit exceeded\n", ON_RUN_REAL);
            result = stop();
            break;
        }
        const moment at = std::chrono::duration_cast<std::chrono::nanoseconds>(current.time_since_epoch()).count();
        const int code = action(at);
        if (code != RUNNING) {
            result = code < 0 ? code : Gpio::SUCCESS;
            break;
        }
        next += tick;
        const auto after = std::chrono::steady_clock::now();
        if (next < after) { next += tick * ((after - next) / tick + 1); }
        std::this_thread::sleep_until(std::min(next, deadline));
    }
    if (real) { *real = sequence_; }
    const std::string prefix = "[" + label + "]";
    sequence_.log(cfg.options_, (prefix + " real").c_str(), cfg.verbose_,
        cfg.expecterInterval_ ? cfg.expecterInterval_ : MICROSECOND);
    if (result < 0) { printf("%s ERROR: %s: motion failed (code %d)\n", ON_RUN_REAL, label.c_str(), result); }
    printf("%s Motion finished; result: %d\n", prefix.c_str(), result);
    fflush(stdout);
    return result;
}

constexpr const char* ON_PROBE = "[StepperMotor.probe()]";

int StepperMotor::probe(float rotationDeg, bool withEstimates, const std::string& label, StepperMotorSeriesSequence* real) {
    const auto& cfg = config_;
    const std::lock_guard<std::mutex> execution(stepperMotorExecutionMutex());
    if (real) { *real = {}; }
    const auto fail = [&](int code, const std::string& message) {
        const std::string error = std::string(ON_PROBE) + " " + label + ": " + message;
        printf("%s ERROR: %s: %s (code %d)\n", ON_PROBE, label.c_str(), message.c_str(), code);
        if (real && real->error.empty()) { real->error = error; }
        fflush(stdout);
        return code;
    };
    std::string error;
    if (!optionsIsOk(cfg.options_, error)) { return fail(Gpio::INVALID_ARGUMENT, error); }
    const duration maximum = std::numeric_limits<int64_t>::max() / 2;
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
    const auto plan = planSequence(rotationDeg, cfg.expecterInterval_);
    if (!plan.error.empty()) { return fail(Gpio::INVALID_ARGUMENT, plan.error); }
    if (withEstimates) {
        const auto ideal = planSequence(rotationDeg, 0);
        if (!ideal.error.empty()) { return fail(Gpio::INVALID_ARGUMENT, ideal.error); }
        ideal.log(cfg.options_, (prefix + " ideal").c_str(), cfg.verbose_);
        plan.log(cfg.options_, (prefix + " clocked").c_str(), cfg.verbose_, cfg.expecterInterval_);
        fflush(stdout);
    }
    const int cleanup = stop();
    if (cleanup < 0) { return fail(cleanup, "previous motion cleanup failed"); }
    setSequence(plan);
    return runReal(label, real);
}
