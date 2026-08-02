#ifndef BASE_CPP_STEPPER_MOTOR_H
#define BASE_CPP_STEPPER_MOTOR_H

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

const float RESULT_EPS_RATIO = 0.01;
const float SPEED_EPS        = 0.01;
const float ACCELERATION_EPS = 0.01;

struct stepper_options_t {
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

enum interval_algorithm_t {
    CONSTANT_ACCELERATION,
    LINEAR_INTERVAL_ACCELERATION
};

class StepperSeries {

public:

    StepperSeries(
        uint64_t pulseCount,  float firstSpeedDegPerSec,  float lastSpeedDegPerSec,  bool directionForward, const stepper_options_t& stepperOpts,
        interval_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION
    );

    [[nodiscard]] float intervalSec(uint64_t pulseIndex, const stepper_options_t& stepperOpts) const;
    [[nodiscard]] float finalSpeed(const stepper_options_t& stepperOpts) const;
    [[nodiscard]] float totalRotationDeg(const stepper_options_t& stepperOpts) const;
    [[nodiscard]] float totalSec(const stepper_options_t& stepperOpts) const;

    void log(const stepper_options_t& stepperOpts, const char* verboseLabel) const {
        printf("\n%s: pulseCount           : %5lu\n",  verboseLabel, pulseCount_);
        printf("%s: initialSpeedDegPerSec  : %9.3f\n", verboseLabel, initialSpeedDegPerSec_);
        printf("%s: totalRotationDeg       : %9.3f\n", verboseLabel, totalRotationDeg(stepperOpts));
        printf("%s: totalSec               : %9.3f\n", verboseLabel, totalSec(stepperOpts));
        printf("%s: finalSpeed             : %9.3f\n", verboseLabel, finalSpeed(stepperOpts));
        printf("%s: directionForward       : %5d\n",   verboseLabel, directionForward_);
        printf("%s: intervalAlgorithm      : %5d\n",   verboseLabel, intervalAlgorithm_);
        printf("%s: accelerationDegPerSec2 : %9.3f\n", verboseLabel, accelerationDegPerSec2_);
    }


    void limitWithDeg(float targetDeg, const stepper_options_t& stepperOpts);

    // removed "private" to simplify tests
    // private:

    uint64_t pulseCount_          = 0;
    float initialSpeedDegPerSec_  = 0.0;    // signed deg/s
    float accelerationDegPerSec2_ = 0.0;    // signed deg/s^2
    float initialIntervalSec_     = 0.0;    // fallback only
    float intervalChangePerPulse_ = 0.0;    // fallback only
    bool  directionForward_       = true;
    interval_algorithm_t intervalAlgorithm_ = CONSTANT_ACCELERATION;
};

struct stepper_sequence_t {
    std::vector<StepperSeries> seriesSequence;
    std::string error;
};

bool optionsIsOk(const stepper_options_t& stepperOpts, std::string& error);

StepperSeries getFastestSeries(
        float initialSpeedDegPerSec,
        float finalSpeedDegPerSec,
        const stepper_options_t& stepperOpts,
        interval_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION);
bool getAcceleratedSequence(
        std::vector<StepperSeries>& seriesSequence,
        float baseSpeedDegPerSec,
        float targetRotationDeg,
        const stepper_options_t& stepperOpts,
        interval_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION);
stepper_sequence_t calculateSequence(
        float initialSpeedDegPerSec,
        float totalRotationDeg,
        float finalSpeedDegPerSec,
        const stepper_options_t& stepperOpts,
        interval_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION);

// series_evaluation_t evaluateSeries(const PulseSeries& series, float currentSpeed, float degreesPerPulse);
// using pulse_callback_t = std::function<bool(int direction)>;
// bool action(const PulseSeries& series,  const pulse_callback_t& pulseCallback, std::string& error);

#endif // BASE_CPP_STEPPER_MOTOR_H
