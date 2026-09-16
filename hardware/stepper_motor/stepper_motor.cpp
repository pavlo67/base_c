#include "stepper_motor.h"
#include "smart/stepper_motor_smart.h"
#include "hardware/gpio/gpio.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <thread>

StepperMotor::StepperMotor(const StepperMotorSeriesSequence& sequence, unsigned pinStep,
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

StepperMotor::~StepperMotor() { (void)stop(); }

const std::string ON_STEPPER_STOP = "[StepperMotor.stop()]";

int StepperMotor::stop() {
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

std::mutex& stepperMotorExecutionMutex() {
    static std::mutex executionMutex;
    return executionMutex;
}

constexpr const char* ON_RUN_REAL = "[StepperMotor.runReal()]";

int StepperMotor::runReal(const StepperMotorRunConfig& cfg, const StepperMotorSeriesSequence& plan,
        const std::string& label, StepperMotorSeriesSequence* real) {
    StepperMotorSeriesSequence observed;
    int result;
    {
        StepperMotorSmart motor(plan, cfg.pinStep_, cfg.pinDir_, cfg.pinEna_,
            cfg.options_, cfg.pulseHigh_, cfg.hardwarePwm_);
        const auto origin = std::chrono::steady_clock::now();
        const auto deadline = origin + std::chrono::nanoseconds(cfg.timeLimit_);
        const auto tick = std::chrono::nanoseconds(std::max<duration>(MICROSECOND, cfg.expecterInterval_));
        auto next = origin;
        for (;;) {
            const auto current = std::chrono::steady_clock::now();
            if (current >= deadline) {
                motor.status_ = TIME_LIMIT;
                motor.sequence_.error = std::string(ON_RUN_REAL) + " time limit exceeded";
                printf("%s ERROR: time limit exceeded\n", ON_RUN_REAL);
                result = motor.stop();
                break;
            }
            const moment at = std::chrono::duration_cast<std::chrono::nanoseconds>(current.time_since_epoch()).count();
            const int code = motor.action(at);
            if (code != RUNNING) {
                result = code < 0 ? code : Gpio::SUCCESS;
                break;
            }
            next += tick;
            const auto after = std::chrono::steady_clock::now();
            if (next < after) { next += tick * ((after - next) / tick + 1); }
            std::this_thread::sleep_until(std::min(next, deadline));
        }
        observed = motor.result();
    }
    if (real) { *real = observed; }
    const std::string prefix = "[" + label + "]";
    observed.log(cfg.options_, (prefix + " real").c_str(), cfg.verbose_,
        cfg.expecterInterval_ ? cfg.expecterInterval_ : MICROSECOND);
    if (result < 0) { printf("%s ERROR: %s: motion failed (code %d)\n", ON_RUN_REAL, label.c_str(), result); }
    printf("%s Motion finished; result: %d\n", prefix.c_str(), result);
    fflush(stdout);
    return result;
}

constexpr const char* ON_PROBE_REAL = "[StepperMotor.probeReal()]";

int StepperMotor::probeReal(const StepperMotorRunConfig& cfg, float rotationDeg,
        const std::string& label, StepperMotorSeriesSequence* real) {
    const std::lock_guard<std::mutex> execution(stepperMotorExecutionMutex());
    if (real) { *real = {}; }
    const auto fail = [&](int code, const std::string& message) {
        const std::string error = std::string(ON_PROBE_REAL) + " " + label + ": " + message;
        printf("%s ERROR: %s: %s (code %d)\n", ON_PROBE_REAL, label.c_str(), message.c_str(), code);
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
    const auto plan = getSeriesSequence(0, rotationDeg, 0, cfg.expecterInterval_, cfg.options_);
    if (!plan.error.empty()) { return fail(Gpio::INVALID_ARGUMENT, plan.error); }
    return runReal(cfg, plan, label, real);
}

constexpr const char* ON_PROBE_ALL = "[StepperMotor.probeAll()]";

int StepperMotor::probeAll(const StepperMotorRunConfig& cfg, float rotationDeg,
        const std::string& label, StepperMotorSeriesSequence* real) {
    const std::lock_guard<std::mutex> execution(stepperMotorExecutionMutex());
    if (real) { *real = {}; }
    const auto fail = [&](int code, const std::string& message) {
        const std::string error = std::string(ON_PROBE_ALL) + " " + label + ": " + message;
        printf("%s ERROR: %s: %s (code %d)\n", ON_PROBE_ALL, label.c_str(), message.c_str(), code);
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
    const auto clocked = getSeriesSequence(0, rotationDeg, 0, cfg.expecterInterval_, cfg.options_);
    if (!clocked.error.empty()) { return fail(Gpio::INVALID_ARGUMENT, clocked.error); }
    const auto ideal = getSeriesSequence(0, rotationDeg, 0, 0, cfg.options_);
    if (!ideal.error.empty()) { return fail(Gpio::INVALID_ARGUMENT, ideal.error); }
    ideal.log(cfg.options_, (prefix + " ideal").c_str(), cfg.verbose_);
    clocked.log(cfg.options_, (prefix + " clocked").c_str(), cfg.verbose_, cfg.expecterInterval_);
    fflush(stdout);
    return runReal(cfg, clocked, label, real);
}
