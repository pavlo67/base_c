#ifndef BASE_CPP_STEPPER_MOTOR_DUMB_H
#define BASE_CPP_STEPPER_MOTOR_DUMB_H

#include "../stepper_motor.h"

// Fixed-frequency PWM move. The requested speed is an upper bound; the timer and
// tolerated remaining pulse count may require a lower integer-Hz command.
class StepperMotorDumb final : public StepperMotor {
public:
    StepperMotorDumb(const StepperMotorRunConfig& config, float speedDegPerSec,
        duration pauseAfterSeries, uint64_t remainingPulsesTolerance);
    int action(moment at) override;

private:
    StepperMotorSeriesSequence planSequence(float angle, duration expecterInterval) const override;
    void prepareSequence() override;

    float speedDegPerSec_;
    duration pauseAfterSeries_;
    uint64_t remainingPulsesTolerance_;
    bool waitingAfterSeries_ = false;
    moment pauseStartedAt_ = 0;
};

#endif // BASE_CPP_STEPPER_MOTOR_DUMB_H
