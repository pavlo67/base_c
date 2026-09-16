#ifndef BASE_CPP_STEPPER_MOTOR_H
#define BASE_CPP_STEPPER_MOTOR_H

#include <mutex>

#include "smart/helpers.h"

// GPIO lifecycle belongs to the caller. Serialize calls for each motor.
std::mutex& stepperMotorExecutionMutex();

class StepperMotor {
public:
    static constexpr int RUNNING = 0;
    static constexpr int COMPLETE = 1;
    static constexpr int TIME_LIMIT = -10008;

    StepperMotor(const StepperMotorSeriesSequence& sequence, unsigned pinStep,
        unsigned pinDir, unsigned pinEna, const stepper_motor_options_t& options,
        duration pulseHigh, bool hardwarePwm = false);
    virtual ~StepperMotor();
    StepperMotor(const StepperMotor&) = delete;
    StepperMotor& operator=(const StepperMotor&) = delete;

    // No sleeps: one update at a monotonic timestamp. Negative results are errors.
    virtual int action(moment at) = 0;
    // rotationDeg is signed degrees. Optional estimates log ideal and clocked plans.
    // The real output receives runtime/partial results even on failure.
    static int probe(const StepperMotorRunConfig& config, float rotationDeg, bool withEstimates,
            const std::string& label = "motor", StepperMotorSeriesSequence* real = nullptr);
    int stop();
    [[nodiscard]] const StepperMotorSeriesSequence& result() const { return sequence_; }

protected:
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
    uint64_t accelerationTicks_ = 0;
    moment readyAt_ = 0;
    moment phaseStartedAt_ = 0;
    size_t index_ = 0;
    int status_ = RUNNING;
    unsigned frequency_ = 0;

private:
    static int runReal(const StepperMotorRunConfig& config, const StepperMotorSeriesSequence& plan,
            const std::string& label, StepperMotorSeriesSequence* real);
};

#endif // BASE_CPP_STEPPER_MOTOR_H
