#ifndef BASE_CPP_HARDWARE_STEPPER_MOTOR_PLATFORM_CONFIG_H
#define BASE_CPP_HARDWARE_STEPPER_MOTOR_PLATFORM_CONFIG_H

#include <array>
#include "smart/helpers.h"
#include "lib/config/config.h"

struct PlatformMechanics {
    double gearRatio_ = 1;
    double momentOfInertia_ = 0;
    double rotorInertia_ = 0;
    double transmissionEfficiency_ = 1;
    double torqueMaxNm_ = 0;
};

// Converts motor degrees/pulse and torque to platform options, atomically.
// speedMaxDegSec is already in platform units. Acceleration input is ignored.
bool platformMotorOptions(const PlatformMechanics& mechanics, stepper_motor_options_t& options);
// Loads both axes atomically, with platform units in the resulting run configurations.
bool loadPlatformMotorConfig(const Config& config, std::array<StepperMotorRunConfig, 2>& motors);

#endif // BASE_CPP_HARDWARE_STEPPER_MOTOR_PLATFORM_CONFIG_H
