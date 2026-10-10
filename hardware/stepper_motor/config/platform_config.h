#ifndef BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H
#define BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H

#include <array>
#include <mutex>
#include "hardware/stepper_motor/stepper_motor_series.h"
#include "lib/config/config.h"
#include "lib/config/config_section.h"

// Validates source options and derives geared displacement and acceleration in place.
// Call before using the options, and again after changing their source values.
bool stepperMotorOptionsOk(stepper_motor_options_t& options, std::string& error);
class ComponentStateMove final : public ComponentState {
public:
    bool load(const Config& cfg) final;
    config_values_t get() const final;

    void getMotors(std::array<StepperMotorRunConfig, 2>& motors) const;

private:
    mutable std::mutex mutex_;
    std::array<StepperMotorRunConfig, 2> motors_{};
    bool loaded_ = false;
};

#endif // BASE_CPP_STEPPER_MOTOR_CONFIG_PLATFORM_CONFIG_H
