#ifndef BASE_CPP_STEPPER_MOTOR_SMART_H
#define BASE_CPP_STEPPER_MOTOR_SMART_H

#include "../stepper_motor.h"

class StepperMotorSmart final : public StepperMotor {
public:
    explicit StepperMotorSmart(const StepperMotorRunConfig& config);
    StepperMotorSmart(const StepperMotorRunConfig& config, const StepperMotorSeriesSequence& sequence);
    StepperMotorSmart(const StepperMotorSeriesSequence& sequence, unsigned pinStep,
        unsigned pinDir, unsigned pinEna, const stepper_motor_options_t& options,
        duration pulseHigh, bool hardwarePwm = false);
    int action(moment at) override;
    // Model live braking on a fixed timer; an empty model means it is unrepresentable.
    static StepperMotorSeries getBrakingModel(float speedDegPerSec, float finalSpeedDegPerSec,
            duration modelInterval, const stepper_motor_options_t& options,
            stepper_motor_algorithm_t algorithm = CONSTANT_ACCELERATION);
    static bool canBrake(float speedDegPerSec, float finalSpeedDegPerSec, duration modelInterval,
            uint64_t remainingPulses, const stepper_motor_options_t& options,
            stepper_motor_algorithm_t algorithm = CONSTANT_ACCELERATION);

private:
    void prepareSequence() override;
};



#endif // BASE_CPP_STEPPER_MOTOR_SMART_H
