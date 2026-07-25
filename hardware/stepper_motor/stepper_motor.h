#ifndef BASE_CPP_STEPPER_MOTOR_H
#define BASE_CPP_STEPPER_MOTOR_H

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

const float DELTA_SPEED_EPS = 0.1;

struct stepper_options_t {
    float freqMax = 0.0;              // pulses/s, motor/driver specification
    float freqMaxAllowed = 0.0;       // pulses/s, software limit, <= freqMax
    float accelMax = 0.0;             // deg/s^2, construction/motor limit
    float degreesPerPulse = 0.0;      // deg/pulse for the configured motor mode
};

struct pulse_series_t {
    std::uint64_t pulseCount = 0;
    float initialIntervalSec = 0.0;
    float intervalChangePerPulse = 0.0;
    bool directionForward = true;

    float changeDeg(const stepper_options_t& stepperOpts)  {
        // TODO!!!
        return 0;
    }

    float targetSpeed(const stepper_options_t& stepperOpts)  {
        // TODO!!!
        return 0;
    }
    void limitWithDeg(float targetDeg, const stepper_options_t& stepperOpts)  {
        // TODO!!!
    }

};

struct stepper_action_t {
    std::vector<pulse_series_t> seriesSequence;
    std::string error;
};

struct series_evaluation_t {
    float durationSec = 0.0;
    float displacementDeg = 0.0;
    float finalSpeed = 0.0;
    std::string error;
};

bool optionsIsOk(const stepper_options_t& stepperOpts, std::string& error);

pulse_series_t   getFastestSeries(float currentSpeed, float changeSpeed, const stepper_options_t& stepperOpts);
bool             getAcceleratedSequence(std::vector<pulse_series_t>& seriesSequence, float currentSpeed, float targetChangeDeg, const stepper_options_t& stepperOpts);
stepper_action_t calculateAction(float currentSpeed, float targetChangeDeg, float targetSpeed, const stepper_options_t& stepperOpts);


series_evaluation_t evaluateSeries(const pulse_series_t& series, float currentSpeed, float degreesPerPulse);


using pulse_callback_t = std::function<bool(int direction)>;
bool action(const pulse_series_t& series,  const pulse_callback_t& pulseCallback, std::string& error);

#endif // BASE_CPP_STEPPER_MOTOR_H
