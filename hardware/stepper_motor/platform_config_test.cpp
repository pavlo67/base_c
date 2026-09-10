#include "platform_config.h"
#include "lib/number_parse.h"

#include <gtest/gtest.h>
#include <limits>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <numbers>

TEST(PlatformMotor, MechanicsAndPulseDisplacement) {
    printf("[PLT] Convert pan and tilt mechanics to platform units\n");
    for (const double ratio : {20.0 / 36, 18.0 / 36}) {
        stepper_motor_options_t options{8000, 0.225F, 180, 0};
        const PlatformMechanics mechanics{ratio, 0.075, 0.00002, 0.95, 0.5};
        ASSERT_TRUE(platformMotorOptions(mechanics, options));
        ASSERT_NEAR(options.degPulse, 0.225 * ratio, 1e-8);
        ASSERT_FLOAT_EQ(options.speedMaxDegSec, 180);
        ASSERT_FLOAT_EQ(options.freqMax, 8000);
        const double accelerationRad = options.accelMaxDegSec2 * std::numbers::pi / 180;
        const double rotorTorque = mechanics.rotorInertia_ * accelerationRad / ratio;
        const double loadTorque = mechanics.momentOfInertia_ * accelerationRad * ratio / mechanics.transmissionEfficiency_;
        ASSERT_NEAR(rotorTorque + loadTorque, 0.5, 1e-7);
        for (const float angle : {90.F, -90.F}) {
            const auto sequence = getSeriesSequence(0, angle, 0, 0, options);
            ASSERT_TRUE(sequence.error.empty()) << sequence.error;
            ASSERT_EQ(options.pulsesForDeg(angle, angle > 0), std::llround(90 / (0.225 * ratio)));
        }
    }
}

TEST(PlatformMotor, RejectInvalidMechanicsAtomically) {
    printf("[PLT] Reject invalid and unrepresentable mechanics\n");
    for (int field = 0; field < 5; ++field) {
        for (const double value : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            PlatformMechanics mechanics{0.5, 0.075, 0.00002, 0.95, 0.5};
            double* fields[] = {&mechanics.gearRatio_, &mechanics.momentOfInertia_, &mechanics.rotorInertia_,
                &mechanics.transmissionEfficiency_, &mechanics.torqueMaxNm_};
            *fields[field] = value;
            stepper_motor_options_t options{8000, 0.225F, 180, 720};
            ASSERT_FALSE(platformMotorOptions(mechanics, options));
            ASSERT_FLOAT_EQ(options.degPulse, 0.225F);
            ASSERT_FLOAT_EQ(options.accelMaxDegSec2, 720);
        }
    }
    stepper_motor_options_t options{8000, 0.225F, 180, 720};
    ASSERT_FALSE(platformMotorOptions({0.5, 0.075, 0, 1.01, 0.5}, options));
    ASSERT_FALSE(platformMotorOptions({0, 0.075, 0, 1, 0.5}, options));
    ASSERT_FALSE(platformMotorOptions({0.5, 0, 0, 1, 0.5}, options));
    ASSERT_TRUE(platformMotorOptions({0.5, 0.075, 0, 1, 0.5}, options));
}

TEST(PlatformMotor, SignedCliNumbers) {
    printf("[PLT] Parse the complete CLI list before movement\n");
    float value = 0;
    ASSERT_TRUE(parseFiniteFloat("+90", value));
    ASSERT_FLOAT_EQ(value, 90);
    ASSERT_TRUE(parseFiniteFloat("-4.5e1", value));
    ASSERT_FLOAT_EQ(value, -45);
    for (const auto* invalid : {"", "+", "++1", "+-1", "90junk", "nan", "inf", "1e100", " 90"}) {
        ASSERT_FALSE(parseFiniteFloat(invalid, value)) << invalid;
    }
}

TEST(PlatformMotor, YamlConversionAndAtomicReload) {
    printf("[PLT] Load physical disk configurations and reject an invalid tilt reload\n");
    const auto path = std::filesystem::temp_directory_path() /
        ("platform_config_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".yaml");
    struct Cleanup {
        std::filesystem::path path_;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path_, error); }
    } cleanup{path};
    YAML::Node document;
    for (const auto* axis : {"pan", "tilt"}) {
        const bool pan = std::string(axis) == "pan";
        auto node = document[axis];
        node["pinStep"] = pan ? 17 : 18;
        node["pinDir"] = pan ? 24 : 23;
        node["pinEna"] = pan ? 27 : 22;
        node["freqMax"] = 8000;
        node["degPulse"] = 0.225;
        node["speedMaxDegSec"] = 180;
        node["gearRatio"] = pan ? 20.0 / 36 : 0.5;
        node["momentOfInertia"] = pan ? 0.00421875 : 0.000703125;
        node["rotorInertia"] = pan ? 0.00002 : 0.0000054;
        node["transmissionEfficiency"] = 0.95;
        node["torqueMaxNm"] = 0.5;
        node["hardwarePwm"] = false;
        node["expecterIntervalUs"] = 5000;
        node["pulseHighUs"] = 15;
        node["timeLimitMs"] = 60000;
    }
    { std::ofstream file(path); file << document; ASSERT_TRUE(file.good()); }
    std::array<StepperMotorRunConfig, 2> motors{};
    ASSERT_TRUE(loadPlatformMotorConfig(Config(path.string()), motors));
    ASSERT_NEAR(motors[0].options_.degPulse, 0.125, 1e-8);
    ASSERT_NEAR(motors[1].options_.degPulse, 0.1125, 1e-8);
    ASSERT_NEAR(motors[0].options_.accelMaxDegSec2, 11444.940082303707, 0.1);
    const auto original = motors;
    document["pan"]["speedMaxDegSec"] = 25;
    document["tilt"]["torqueMaxNm"] = -0.5;
    { std::ofstream file(path); file << document; ASSERT_TRUE(file.good()); }
    ASSERT_FALSE(loadPlatformMotorConfig(Config(path.string()), motors));
    for (size_t i = 0; i < motors.size(); ++i) {
        ASSERT_FLOAT_EQ(motors[i].options_.speedMaxDegSec, original[i].options_.speedMaxDegSec);
        ASSERT_FLOAT_EQ(motors[i].options_.accelMaxDegSec2, original[i].options_.accelMaxDegSec2);
    }
}
