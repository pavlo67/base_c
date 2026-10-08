#include "platform_config.h"

#include <cstdio>
#include <limits>
#include <numbers>
#include "hardware/gpio/gpio.h"
#include "lib/mathlib.h"

const std::string ON_STEPPER_OPTIONS_OK = "[stepperMotorOptionsOk()] ERROR: ";

bool stepperMotorOptionsOk(stepper_motor_options_t& options, std::string& error) {
    error.clear();
    if (!isFinitePositive(options.freqMax)) {
        error = ON_STEPPER_OPTIONS_OK + "freqMax must be finite and greater than zero";
        return false;
    }
    if (!isFinitePositive(options.speedMaxDegSec)) {
        error = ON_STEPPER_OPTIONS_OK + "speedMaxDegSec must be finite and greater than zero";
        return false;
    }
    if (!isFinitePositive(options.degPulse)) {
        error = ON_STEPPER_OPTIONS_OK + "degPulse must be finite and greater than zero";
        return false;
    }
    const auto& m = options.mechanics_;
    if (!isFinitePositive(m.gearRatio_) || !isFinitePositive(m.momentOfInertia_) ||
            !std::isfinite(m.rotorInertia_) || m.rotorInertia_ < 0 ||
            !isFinitePositive(m.transmissionEfficiency_) || m.transmissionEfficiency_ > 1 ||
            !isFinitePositive(m.torqueMaxNm_)) {
        error = ON_STEPPER_OPTIONS_OK + "invalid ratio, load/rotor inertia, efficiency or torque";
        return false;
    }
    const double effectiveInertia = m.rotorInertia_ / m.gearRatio_ +
        m.momentOfInertia_ * m.gearRatio_ / m.transmissionEfficiency_;
    const double acceleration = m.torqueMaxNm_ / effectiveInertia * 180.0 / std::numbers::pi;
    const double degreesPerPulse = options.degPulse * m.gearRatio_;
    if (!isFinitePositive(acceleration) || acceleration > std::numeric_limits<float>::max() ||
            !isFinitePositive(degreesPerPulse) || degreesPerPulse > std::numeric_limits<float>::max()) {
        error = ON_STEPPER_OPTIONS_OK + "derived platform options are out of range";
        return false;
    }
    options.degPulseGeared = static_cast<float>(degreesPerPulse);
    options.accelMaxDegSec2 = static_cast<float>(acceleration);
    if (!isFinitePositive(options.degPulseGeared) || !isFinitePositive(options.accelMaxDegSec2)) {
        error = ON_STEPPER_OPTIONS_OK + "derived platform options are below the supported range";
        return false;
    }
    return true;
}

constexpr const char* ON_LOAD_MOTOR_CONFIG = "[loadPlatformMotorConfig()]";

bool loadPlatformMotorConfig(const Config& cfg, std::array<StepperMotorRunConfig, 2>& motors) {
    if (!cfg.loadedOk()) {
        printf("%s ERROR: configuration is unavailable\n", ON_LOAD_MOTOR_CONFIG);
        return false;
    }
    std::array<bool, Gpio::PIN_COUNT> used{};
    const char* axis = "pan";
    try {
        const auto kindNode = cfg.get("motorClass");
        const std::string kind = kindNode ? kindNode.as<std::string>() : "Smart";
        if (kind != "Smart" && kind != "Dumb") {
            printf("%s ERROR: motorClass must be Smart or Dumb\n", ON_LOAD_MOTOR_CONFIG);
            return false;
        }
        for (size_t i = 0; i < motors.size(); ++i) {
            axis = i == 0 ? "pan" : "tilt";
            const auto node = cfg.get(axis);
            auto& motor = motors[i];
            motor = {};
            motor.kind_ = kind == "Smart" ? StepperMotorKind::smart : StepperMotorKind::dumb;
            motor.pinStep_ = node["pinStep"].as<unsigned>();
            motor.pinDir_ = node["pinDir"].as<unsigned>();
            motor.pinEna_ = node["pinEna"].as<unsigned>();
            motor.options_.freqMax = node["freqMax"].as<float>();
            motor.options_.degPulse = node["degPulse"].as<float>();
            motor.options_.speedMaxDegSec = node["speedMaxDegSec"].as<float>();
            auto& mechanics = motor.options_.mechanics_;
            mechanics.gearRatio_ = node["gearRatio"].as<double>();
            mechanics.momentOfInertia_ = node["momentOfInertia"].as<double>();
            mechanics.rotorInertia_ = node["rotorInertia"].as<double>();
            mechanics.transmissionEfficiency_ = node["transmissionEfficiency"].as<double>();
            mechanics.torqueMaxNm_ = node["torqueMaxNm"].as<double>();
            std::string error;
            if (!stepperMotorOptionsOk(motor.options_, error)) {
                printf("%s ERROR: %s: %s\n", ON_LOAD_MOTOR_CONFIG, axis, error.c_str());
                return false;
            }
            motor.hardwarePwm_ = node["hardwarePwm"].as<bool>();
            const auto interval = node["expecterIntervalUs"].as<uint64_t>();
            const auto pulse = node["pulseHighUs"].as<uint64_t>();
            const auto limit = node["timeLimitMs"].as<uint64_t>();
            constexpr auto maximum = std::numeric_limits<int64_t>::max() / 2;
            if (interval > maximum / MICROSECOND || pulse == 0 || pulse > SECOND / (2 * MICROSECOND) ||
                    limit == 0 || limit > maximum / MILLISECOND) {
                printf("%s ERROR: %s: invalid timing parameters\n", ON_LOAD_MOTOR_CONFIG, axis);
                return false;
            }
            motor.expecterInterval_ = interval * MICROSECOND;
            motor.pulseHigh_ = pulse * MICROSECOND;
            motor.timeLimit_ = limit * MILLISECOND;
            if (motor.kind_ == StepperMotorKind::dumb) {
                motor.dumbSpeedDegSec_ = node["dumbSpeedDegSec"].as<float>();
                const auto pauseMs = node["dumbPauseMs"].as<uint64_t>();
                motor.dumbRemainingPulsesTolerance_ = node["dumbRemainingPulsesTolerance"].as<uint64_t>();
                if (!isFinitePositive(motor.dumbSpeedDegSec_) || pauseMs >= limit) {
                    printf("%s ERROR: %s: invalid Dumb speed or pause\n", ON_LOAD_MOTOR_CONFIG, axis);
                    return false;
                }
                motor.dumbPause_ = pauseMs * MILLISECOND;
            }
            for (const unsigned pin : {motor.pinStep_, motor.pinDir_, motor.pinEna_}) {
                if (pin >= Gpio::PIN_COUNT || used[pin]) {
                    printf("%s ERROR: %s: BCM pins must be in 0..27 and distinct across both motors\n", ON_LOAD_MOTOR_CONFIG, axis);
                    return false;
                }
                used[pin] = true;
            }
            if (motor.hardwarePwm_ && Gpio::instance().hardwarePwmChannel(motor.pinStep_) < 0) {
                printf("%s ERROR: %s: STEP does not support hardware PWM\n", ON_LOAD_MOTOR_CONFIG, axis);
                return false;
            }
        }
    } catch (const YAML::Exception& error) {
        printf("%s ERROR: %s: %s\n", ON_LOAD_MOTOR_CONFIG, axis, error.what());
        return false;
    }
    return true;
}
