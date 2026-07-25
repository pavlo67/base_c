#ifndef BASE_CPP_STEPPER_MOTOR_H
#define BASE_CPP_STEPPER_MOTOR_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct stepper_options_t {
    double freqMax = 0.0;              // pulses/s, motor/driver specification
    double freqAllowed = 0.0;          // pulses/s, software limit, <= freqMax
    double accelMax = 0.0;             // deg/s^2
    double currentSpeed = 0.0;         // deg/s; sign is direction
    double targetPositionDeg = 0.0;    // deg relative to current position
    double targetSpeed = 0.0;          // deg/s; sign is direction
    double degreesPerPulse = 0.0;      // deg/pulse for the configured microstep mode
};

struct pulse_series_t {
    std::uint64_t pulseCount = 0;
    double initialIntervalSec = 0.0;
    double intervalChangeSec = 0.0;
    int direction = 0;                 // -1 or +1
    bool stopAfterSeries = false;       // final speed is zero after the last pulse
};

struct stepper_action_t {
    std::vector<pulse_series_t> series;
    std::string error;
};

struct series_evaluation_t {
    double durationSec = 0.0;
    double displacementDeg = 0.0;
    double finalSpeed = 0.0;
    std::string error;
};

using pulse_callback_t = std::function<bool(int direction)>;

stepper_action_t CalculateAction(const stepper_options_t& options);

series_evaluation_t evaluateSeries(
        const pulse_series_t& series,
        double currentSpeed,
        double degreesPerPulse);

bool action(
        const pulse_series_t& series,
        const pulse_callback_t& pulseCallback,
        std::string& error);

#endif // BASE_CPP_STEPPER_MOTOR_H
