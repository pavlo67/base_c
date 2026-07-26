#ifndef BASE_CPP_STEPPER_MOTOR_H
#define BASE_CPP_STEPPER_MOTOR_H

#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

const float SPEED_DEG_PER_SEC_EPS = 0.1;

struct stepper_options_t {
    float freqMax = 0.0;              // pulses/s, motor/driver specification
    float freqMaxAllowed = 0.0;       // pulses/s, software limit, <= freqMax
    float accelMax = 0.0;             // deg/s^2, construction/motor limit
    float degreesPerPulse = 0.0;      // deg/pulse for the configured motor mode


    [[nodiscard]] std::uint64_t pulsesForDeg(float changeDeg, bool directionForward) const {
        return changeDeg * (directionForward ? 1.F : -1.F) >= 0 ? std::llround(std::abs(changeDeg) / degreesPerPulse) : 0;
    }

    [[nodiscard]] float pulseInterval(float speedDegPerSec) const {
        return degreesPerPulse / std::abs(speedDegPerSec);
    }

    [[nodiscard]] float speedAfterOnePulse() const {
        return std::sqrt(2.0F * accelMax * degreesPerPulse);
    }

};

enum interval_algorithm_t {
    CONSTANT_ACCELERATION,
    LINEAR_INTERVAL_ACCELERATION
};

class StepperSeries {

public:

    StepperSeries(
        std::uint64_t pulseCount,  float firstSpeedDegPerSec,  float lastSpeedDegPerSec,  bool directionForward, const stepper_options_t& stepperOpts,
        interval_algorithm_t intervalAlgorithm = CONSTANT_ACCELERATION
    );

    [[nodiscard]] float intervalSec(std::uint64_t pulseIndex, const stepper_options_t& stepperOpts) const;
    [[nodiscard]] float finalSpeed(const stepper_options_t& stepperOpts) const;
    [[nodiscard]] float totalRotationDeg(const stepper_options_t& stepperOpts) const;

    void limitWithDeg(float targetDeg, const stepper_options_t& stepperOpts);

    // removed "private" to simplify tests
    // private:

    std::uint64_t pulseCount_               = 0;
    interval_algorithm_t intervalAlgorithm_ = CONSTANT_ACCELERATION;
    float initialSpeedDegPerSec_  = 0.0;    // signed deg/s
    float accelerationDegPerSec2_ = 0.0;    // signed deg/s^2
    float initialIntervalSec_     = 0.0;    // fallback only
    float intervalChangePerPulse_ = 0.0;    // fallback only
    bool  directionForward_       = true;

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
        float totalRotationDeg,
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
