#ifndef BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H
#define BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H

#include <array>
#include "hardware/stepper_motor/stepper_motor_series.h"
#include "lib/config/config.h"

// Validates source options and derives geared displacement and acceleration in place.
// Call before using the options, and again after changing their source values.
bool stepperMotorOptionsOk(stepper_motor_options_t& options, std::string& error);
// Loads both axes in place, including motor class and class-specific settings.
// On failure, the configurations may be partially updated.
bool loadPlatformMotorConfig(const Config& config, std::array<StepperMotorRunConfig, 2>& motors);

#endif // BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H
