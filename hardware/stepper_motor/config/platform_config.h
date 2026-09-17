#ifndef BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H
#define BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H

#include <array>
#include "hardware/stepper_motor/stepper_motor_series.h"
#include "lib/config/config.h"

struct PlatformMechanics {
    double gearRatio_ = 1;
    double momentOfInertia_ = 0;
    double rotorInertia_ = 0;
    double transmissionEfficiency_ = 1;
    double torqueMaxNm_ = 0;
};

enum class StepperMotorKind { smart, dumb };

struct DumbMotorConfig {
    float speedDegPerSec_ = 0;
    duration pauseAfterSeries_ = 0;
    uint64_t remainingPulsesTolerance_ = 0;
};

struct StepperMotorProbeConfig {
    std::array<StepperMotorRunConfig, 2> motors_{};
    StepperMotorKind kind_ = StepperMotorKind::smart;
    std::array<DumbMotorConfig, 2> dumb_{};
};

// Converts motor degrees/pulse and torque to platform options, atomically.
// speedMaxDegSec is already in platform units. Acceleration input is ignored.
bool platformMotorOptions(const PlatformMechanics& mechanics, stepper_motor_options_t& options);
// Loads both axes atomically, with platform units in the resulting run configurations.
bool loadPlatformMotorConfig(const Config& config, std::array<StepperMotorRunConfig, 2>& motors);
// Reads the probe's motor class and class-specific fields after the shared axis configuration.
bool loadStepperMotorProbeConfig(const Config& config, StepperMotorProbeConfig& probe);

#endif // BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H
