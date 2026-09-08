#ifndef BASE_CPP_STEPPER_MOTOR_H
#define BASE_CPP_STEPPER_MOTOR_H

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "lib/timelib.h"

const float RESULT_EPS_RATIO = 0.01;
const float SPEED_EPS        = 0.01;
const float ACCELERATION_EPS = 0.01;

struct stepper_motor_options_t {
    float freqMax         = 0;      // pulses/s, motor/driver specification
    float degPulse        = 0;      // deg/pulse for the configured motor mode
    float speedMaxDegSec  = 0;      // deg/s,    video software limit
    float accelMaxDegSec2 = 0;      // deg/s^2,  construction/motor limit

    [[nodiscard]] uint64_t pulsesForDeg(float changeDeg, bool directionForward) const {
        return changeDeg * (directionForward ? 1.F : -1.F) >= 0 ? std::llround(std::abs(changeDeg) / degPulse) : 0;
    }

    [[nodiscard]] float pulseInterval(float speedDegPerSec) const {
        return degPulse / std::abs(speedDegPerSec);
    }

    [[nodiscard]] float speedAfterOnePulse() const {
        return std::sqrt(2.0F * accelMaxDegSec2 * degPulse);
    }

};

enum stepper_motor_algorithm_t {
    CONSTANT_ACCELERATION,
    LINEAR_INTERVAL_ACCELERATION
};

class StepperMotorSeries {

public:

    StepperMotorSeries(
        uint64_t pulsesCount, float firstSpeedDegPerSec,  float lastSpeedDegPerSec,  bool directionForward, const stepper_motor_options_t& stepperOpts,
        stepper_motor_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION
    );

    [[nodiscard]] float intervalSec(moment at, const stepper_motor_options_t& stepperOpts);
    [[nodiscard]] float scheduledIntervalSec(uint64_t pulseIndex, const stepper_motor_options_t& stepperOpts);
    [[nodiscard]] float idealIntervalSec(uint64_t pulseIndex, const stepper_motor_options_t& stepperOpts) const;
    [[nodiscard]] float idealFinalSpeed(const stepper_motor_options_t& stepperOpts) const;
    [[nodiscard]] float expectedRotationDeg(const stepper_motor_options_t& stepperOpts) const;
    [[nodiscard]] float idealTotalSec(const stepper_motor_options_t& stepperOpts) const;
    [[nodiscard]] float totalSec() const;
    void reset();
    [[nodiscard]] float finalSpeed(const stepper_motor_options_t& stepperOpts) const;
    [[nodiscard]] float totalRotationDeg(const stepper_motor_options_t& stepperOpts) const;
    [[nodiscard]] float totalSec(const stepper_motor_options_t& stepperOpts) const;

    void log(const stepper_motor_options_t& stepperOpts, const char* verboseLabel) const {
        printf("%s: pulsesCount            : %5lu\n",  verboseLabel, pulsesCount_);
        printf("%s: expectedPulsesCount    : %5lu\n", verboseLabel, expectedPulsesCount_);
        printf("%s: maxSpeedDegSec         : %9.3f\n", verboseLabel, maxSpeedDegSec_);
        printf("%s: maxAccelerationDegSec2 : %9.3f\n", verboseLabel, maxAccelerationDegSec2_);
        printf("%s: initialSpeedDegPerSec  : %9.3f\n", verboseLabel, initialSpeedDegPerSec_);
        printf("%s: totalRotationDeg       : %9.3f\n", verboseLabel, totalRotationDeg(stepperOpts));
        printf("%s: totalSec               : %9.3f\n", verboseLabel, totalSec(stepperOpts));
        printf("%s: finalSpeed             : %9.3f\n", verboseLabel, finalSpeed(stepperOpts));
        printf("%s: directionForward       : %5d\n",   verboseLabel, directionForward_);
        printf("%s: intervalAlgorithm      : %5d\n",   verboseLabel, intervalAlgorithm_);
        printf("%s: accelerationDegPerSec2 : %9.3f\n", verboseLabel, accelerationDegPerSec2_);
    }


    void limitWithDeg(float targetDeg, const stepper_motor_options_t& stepperOpts);

    // removed "private" to simplify tests
    // private:

    uint64_t expectedPulsesCount_  = 0;
    uint64_t pulsesCount_          = 0;
    // Finite reserved final interval for accelerated rest-to-rest motion.
    duration initialPulseInterval_ = 0; // Previous live section's last pulse interval.
    duration setupDelay_           = 0; // Live enable/direction guard, included in totalSec().
    duration terminalInterval_     = 0;
    unsigned cruiseFrequency_     = 0; // Preserve the reached integer-Hz PWM command in cruise.
    uint64_t stopAfterPulses_       = 0;
    uint64_t pairedTargetPulses_ = 0; // Acceleration/cruise/braking budget, or remaining cruise/braking budget.
    bool frequencyLimited_        = false;
    bool livePwm_                  = false;
    uint64_t minimumPulsesCount_   = 0; // Complete a remaining angle at the last period.
    moment   firstPulseAt_         = 0;
    moment   lastPulseAt_          = 0;
    moment   startedAt_            = 0;
    moment   observedAt_           = 0;
    moment   nextPulseAt_          = 0;
    moment   recalculateAfter_     = 0;
    duration activeInterval_       = 0;
    duration pendingInterval_      = 0;
    duration lastInterval_         = 0;
    uint64_t intervalIndex_        = 0;
    bool started_                 = false;
    bool finished_                = false;
    bool repeatLastInterval_       = false;
    std::function<void(moment)> onPulse_;
    float maxSpeedDegSec_         = 0;
    float maxAccelerationDegSec2_ = 0;
    float initialSpeedDegPerSec_  = 0.0;    // signed deg/s
    float accelerationDegPerSec2_ = 0.0;    // signed deg/s^2
    float initialIntervalSec_     = 0.0;    // fallback only
    float intervalChangePerPulse_ = 0.0;    // fallback only
    bool  directionForward_       = true;
    stepper_motor_algorithm_t intervalAlgorithm_ = CONSTANT_ACCELERATION;
};

struct StepperMotorSeriesSequence {
    std::vector<StepperMotorSeries> seq;
    std::string error;

    void log(const stepper_motor_options_t& stepperOpts, const char* label, bool verbose = false) const;
};

bool optionsIsOk(const stepper_motor_options_t& stepperOpts, std::string& error);

StepperMotorSeries getFastestSeries(
        float initialSpeedDegPerSec,
        float finalSpeedDegPerSec,
        const stepper_motor_options_t& stepperOpts,
        stepper_motor_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION);

// Evaluate optional cruise followed by braking over the remaining displacement.
// The remaining count excludes any separately reserved terminal pulse.
std::vector<StepperMotorSeries> getCruiseAndBraking(float speedDegPerSec, float finalSpeedDegPerSec,
        uint64_t remainingPulses, duration timer, const stepper_motor_options_t& options,
        stepper_motor_algorithm_t algorithm, moment startedAt = 0, bool livePwm = false);

bool addAcceleratedSeries(
        StepperMotorSeriesSequence& seriesSequence,
        float baseSpeedDegPerSec,
        float targetRotationDeg,
        duration expecterInterval,
        const stepper_motor_options_t& stepperOpts,
        stepper_motor_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION);

StepperMotorSeriesSequence getSeriesSequence(
        float initialSpeedDegPerSec,
        float totalRotationDeg,
        float finalSpeedDegPerSec,
        duration expecterInterval,
        const stepper_motor_options_t& stepperOpts,
        stepper_motor_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION);

// GPIO lifecycle belongs to the caller. Serialize calls for each motor.
class StepperMotorAction {
public:
    static constexpr int RUNNING = 0;
    static constexpr int COMPLETE = 1;
    static constexpr int TIME_LIMIT = -10008;

    StepperMotorAction(const StepperMotorSeriesSequence& sequence, unsigned pinStep,
        unsigned pinDir, unsigned pinEna, const stepper_motor_options_t& options,
        duration pulseHigh, bool hardwarePwm = false);
    ~StepperMotorAction();
    StepperMotorAction(const StepperMotorAction&) = delete;
    StepperMotorAction& operator=(const StepperMotorAction&) = delete;

    // No sleeps: one update at a monotonic timestamp. Negative results are errors.
    int action(moment at);
    // Fixed timer and independent watchdog; does not inspect the series plan.
    int run(duration expecterInterval, duration timeLimit = 60 * SECOND);
    int stop();
    [[nodiscard]] const StepperMotorSeriesSequence& result() const { return sequence_; }

private:
    StepperMotorSeriesSequence sequence_;
    stepper_motor_options_t options_;
    unsigned pinStep_, pinDir_, pinEna_;
    duration pulseHigh_;
    bool hardwarePwm_;
    bool enableConfigured_ = false;
    bool stepConfigured_ = false;
    bool directionConfigured_ = false;
    bool initialized_ = false;
    bool hasMoment_ = false;
    moment lastAt_ = 0;
    duration observedInterval_ = 0; // Largest external tick spacing observed so far.
    moment readyAt_ = 0;
    moment phaseStartedAt_ = 0;
    size_t index_ = 0;
    int status_ = RUNNING;
    unsigned frequency_ = 0;
};

// Complete, serialized move from rest to rest; owns GPIO initialization/termination.
struct StepperMotorRunConfig {
    unsigned pinStep_ = 0;
    unsigned pinDir_ = 0;
    unsigned pinEna_ = 0;
    stepper_motor_options_t options_{};
    duration expecterInterval_ = 0;
    duration pulseHigh_ = 0;
    duration timeLimit_ = 60 * SECOND;
    bool hardwarePwm_ = false;
    bool verbose_ = false;
};

// rotationDeg is signed degrees, independent of application command encoding.
// Logs ideal/clocked/real statistics; real also receives partial results on failure.
int run(const StepperMotorRunConfig& config, float rotationDeg,
        const std::string& label = "motor", StepperMotorSeriesSequence* real = nullptr);

// Direct blocking replacement for move(); real receives actual update statistics.
int executeSequence(const StepperMotorSeriesSequence& sequence, unsigned pinStep, unsigned pinDir, unsigned pinEna,
        duration expecterInterval, const stepper_motor_options_t& stepperOpts, duration pulseHigh,
        bool hardwarePwm = false, duration timeLimit = 60 * SECOND,
        StepperMotorSeriesSequence* real = nullptr);

// Evaluate a fresh copy on a timer grid anchored at zero. A zero timer visits pulse boundaries.
// stopAfterPulses is checked on timer ticks; repeated PWM pulses can cross this threshold.
StepperMotorSeries evaluateSeries(StepperMotorSeries series, duration expecterInterval,
        const stepper_motor_options_t& stepperOpts, moment startedAt = 0,
        uint64_t stopAfterPulses = 0, const std::function<void(moment)>& onPulse = {});

#endif // BASE_CPP_STEPPER_MOTOR_H
