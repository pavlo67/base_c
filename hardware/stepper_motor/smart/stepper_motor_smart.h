#ifndef BASE_CPP_STEPPER_MOTOR_SMART_H
#define BASE_CPP_STEPPER_MOTOR_SMART_H

#include "../stepper_motor.h"

class StepperMotorSmart final : public StepperMotor {
public:
    using StepperMotor::StepperMotor;
    int action(moment at) override;
};

#endif // BASE_CPP_STEPPER_MOTOR_SMART_H
