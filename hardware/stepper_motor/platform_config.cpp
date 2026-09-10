#include "platform_config.h"

#include <cstdio>
#include <limits>
#include <numbers>
#include "hardware/gpio/gpio.h"

constexpr const char* ON_PLATFORM_OPTIONS = "[platformMotorOptions()]";
bool platformMotorOptions(const PlatformMechanics& m, stepper_motor_options_t& options) {
    if (!std::isfinite(m.gearRatio_) || m.gearRatio_ <= 0 ||
            !std::isfinite(m.momentOfInertia_) || m.momentOfInertia_ <= 0 ||
            !std::isfinite(m.rotorInertia_) || m.rotorInertia_ < 0 ||
            !std::isfinite(m.transmissionEfficiency_) || m.transmissionEfficiency_ <= 0 ||
            m.transmissionEfficiency_ > 1 || !std::isfinite(m.torqueMaxNm_) || m.torqueMaxNm_ <= 0) {
        printf("%s ERROR: invalid ratio, load/rotor inertia, efficiency or torque\n", ON_PLATFORM_OPTIONS);
        return false;
    }
    const double effectiveInertia = m.rotorInertia_ / m.gearRatio_ +
        m.momentOfInertia_ * m.gearRatio_ / m.transmissionEfficiency_;
    const double acceleration = m.torqueMaxNm_ / effectiveInertia * 180.0 / std::numbers::pi;
    const double degreesPerPulse = options.degPulse * m.gearRatio_;
    if (!std::isfinite(acceleration) || acceleration <= 0 || acceleration > std::numeric_limits<float>::max() ||
            !std::isfinite(degreesPerPulse) || degreesPerPulse <= 0 || degreesPerPulse > std::numeric_limits<float>::max()) {
        printf("%s ERROR: derived platform options are out of range\n", ON_PLATFORM_OPTIONS);
        return false;
    }
    auto candidate = options;
    candidate.degPulse = static_cast<float>(degreesPerPulse);
    candidate.accelMaxDegSec2 = static_cast<float>(acceleration);
    std::string error;
    if (!optionsIsOk(candidate, error)) {
        printf("%s ERROR: %s\n", ON_PLATFORM_OPTIONS, error.c_str());
        return false;
    }
    options = candidate;
    return true;
}

constexpr const char* ON_LOAD_MOTOR_CONFIG = "[loadPlatformMotorConfig()]";

bool loadPlatformMotorConfig(const Config& cfg, std::array<StepperMotorRunConfig, 2>& motors) {
    if (!cfg.loadedOk()) {
        printf("%s ERROR: configuration is unavailable\n", ON_LOAD_MOTOR_CONFIG);
        return false;
    }
    std::array<StepperMotorRunConfig, 2> candidate{};
    std::array<bool, Gpio::PIN_COUNT> used{};
    const char* axis = "pan";
    try {
        for (size_t i = 0; i < candidate.size(); ++i) {
            axis = i == 0 ? "pan" : "tilt";
            const auto node = cfg.get(axis);
            auto& motor = candidate[i];
            motor.pinStep_ = node["pinStep"].as<unsigned>();
            motor.pinDir_ = node["pinDir"].as<unsigned>();
            motor.pinEna_ = node["pinEna"].as<unsigned>();
            motor.options_.freqMax = node["freqMax"].as<float>();
            motor.options_.degPulse = node["degPulse"].as<float>();
            motor.options_.speedMaxDegSec = node["speedMaxDegSec"].as<float>();
            const PlatformMechanics mechanics {
                node["gearRatio"].as<double>(),
                node["momentOfInertia"].as<double>(),
                node["rotorInertia"].as<double>(),
                node["transmissionEfficiency"].as<double>(),
                node["torqueMaxNm"].as<double>()
            };
            if (!platformMotorOptions(mechanics, motor.options_)) { return false; }
            motor.hardwarePwm_ = node["hardwarePwm"].as<bool>();
            const auto interval = node["expecterIntervalUs"].as<uint64_t>();
            const auto pulse = node["pulseHighUs"].as<uint64_t>();
            const auto limit = node["timeLimitMs"].as<uint64_t>();
            constexpr auto maximum = std::numeric_limits<int64_t>::max() / 2;
            std::string error;
            if (!optionsIsOk(motor.options_, error)) {
                printf("%s ERROR: %s: %s\n", ON_LOAD_MOTOR_CONFIG, axis, error.c_str());
                return false;
            }
            if (interval > maximum / MICROSECOND || pulse == 0 || pulse > SECOND / (2 * MICROSECOND) ||
                    limit == 0 || limit > maximum / MILLISECOND) {
                printf("%s ERROR: %s: invalid timing parameters\n", ON_LOAD_MOTOR_CONFIG, axis);
                return false;
            }
            motor.expecterInterval_ = interval * MICROSECOND;
            motor.pulseHigh_ = pulse * MICROSECOND;
            motor.timeLimit_ = limit * MILLISECOND;
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
    motors = candidate;
    return true;
}

