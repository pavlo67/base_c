#include "platform_config.h"
#include "lib/strlib.h"

#include <gtest/gtest.h>
#include <limits>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <numbers>

TEST(PlatformMotor, MechanicsAndPulseDisplacement) {
    printf("[PLT] Convert pan and tilt mechanics to platform units\n");
    std::string error;
    for (const double ratio : {20.0 / 36, 18.0 / 36}) {
        stepper_motor_options_t options{8000, 0.225F, 180, 0};
        options.mechanics_ = {ratio, 0.075, 0.00002, 0.95, 0.5};
        const auto& mechanics = options.mechanics_;
        ASSERT_TRUE(stepperMotorOptionsOk(options, error));
        ASSERT_NEAR(options.degPulseGeared, 0.225 * ratio, 1e-8);
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

TEST(PlatformMotor, RejectInvalidMechanics) {
    printf("[PLT] Reject invalid and unrepresentable mechanics\n");
    std::string error;
    for (int field = 0; field < 5; ++field) {
        for (const double value : {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            stepper_motor_options_t options{8000, 0.225F, 180, 720};
            options.mechanics_ = {0.5, 0.075, 0.00002, 0.95, 0.5};
            auto& mechanics = options.mechanics_;
            double* fields[] = {&mechanics.gearRatio_, &mechanics.momentOfInertia_, &mechanics.rotorInertia_,
                &mechanics.transmissionEfficiency_, &mechanics.torqueMaxNm_};
            *fields[field] = value;
            ASSERT_FALSE(stepperMotorOptionsOk(options, error));
            ASSERT_FLOAT_EQ(options.degPulse, 0.225F);
            ASSERT_FLOAT_EQ(options.accelMaxDegSec2, 720);
        }
    }
    stepper_motor_options_t options{8000, 0.225F, 180, 720};
    options.mechanics_ = {0.5, 0.075, 0, 1.01, 0.5};
    ASSERT_FALSE(stepperMotorOptionsOk(options, error));
    options.mechanics_ = {0, 0.075, 0, 1, 0.5};
    ASSERT_FALSE(stepperMotorOptionsOk(options, error));
    options.mechanics_ = {0.5, 0, 0, 1, 0.5};
    ASSERT_FALSE(stepperMotorOptionsOk(options, error));
    options.mechanics_ = {0.5, 0.075, 0, 1, 0.5};
    ASSERT_TRUE(stepperMotorOptionsOk(options, error));
}

TEST(PlatformMotor, RepeatedValidationRecalculatesFromSourceValues) {
    printf("[PLT] Preserve motor displacement when preparing the same options repeatedly\n");
    stepper_motor_options_t options{8000, 0.225F, 180, 0};
    options.mechanics_ = {0.5, 0.075, 0.00002, 0.95, 0.5};
    std::string error;
    ASSERT_TRUE(stepperMotorOptionsOk(options, error)) << error;
    const float geared = options.degPulseGeared;
    const float acceleration = options.accelMaxDegSec2;
    for (int repeat = 0; repeat < 3; ++repeat) {
        ASSERT_TRUE(stepperMotorOptionsOk(options, error)) << error;
        ASSERT_FLOAT_EQ(options.degPulse, 0.225F);
        ASSERT_FLOAT_EQ(options.degPulseGeared, geared);
        ASSERT_FLOAT_EQ(options.accelMaxDegSec2, acceleration);
    }
    printf("[PLT] Apply externally changed mechanics and motor displacement\n");
    options.mechanics_.gearRatio_ = 0.25;
    options.mechanics_.torqueMaxNm_ = 0.75;
    options.degPulse = 0.45F;
    ASSERT_TRUE(stepperMotorOptionsOk(options, error)) << error;
    ASSERT_FLOAT_EQ(options.degPulse, 0.45F);
    ASSERT_FLOAT_EQ(options.degPulseGeared, 0.1125F);
    ASSERT_NE(options.accelMaxDegSec2, acceleration);
    ASSERT_EQ(options.pulsesForDeg(90, true), 800U);
}

TEST(PlatformMotor, RejectInvalidSourceAndDerivedOptions) {
    printf("[PLT] Reject invalid source options before movement\n");
    std::string error;
    for (int field = 0; field < 3; ++field) {
        for (const float value : {0.F, -1.F, std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN()}) {
            stepper_motor_options_t options{8000, 0.225F, 180, 0};
            options.mechanics_ = {0.5, 0.075, 0.00002, 0.95, 0.5};
            float* fields[] = {&options.freqMax, &options.degPulse, &options.speedMaxDegSec};
            *fields[field] = value;
            ASSERT_FALSE(stepperMotorOptionsOk(options, error));
            ASSERT_EQ(error.find("[stepperMotorOptionsOk()] ERROR: "), 0U);
        }
    }
    printf("[PLT] Reject float overflow and underflow of derived values\n");
    stepper_motor_options_t options{8000, std::numeric_limits<float>::max(), 180, 0};
    options.mechanics_ = {2, 0.075, 0.00002, 0.95, 0.5};
    ASSERT_FALSE(stepperMotorOptionsOk(options, error));
    options.degPulse = std::numeric_limits<float>::denorm_min();
    options.mechanics_.gearRatio_ = 0.25;
    ASSERT_FALSE(stepperMotorOptionsOk(options, error));
    options.degPulse = 0.225F;
    options.mechanics_.torqueMaxNm_ = 1e-100;
    ASSERT_FALSE(stepperMotorOptionsOk(options, error));
    options.mechanics_.torqueMaxNm_ = std::numeric_limits<double>::max();
    ASSERT_FALSE(stepperMotorOptionsOk(options, error));
    options.mechanics_.torqueMaxNm_ = 0.5;
    ASSERT_TRUE(stepperMotorOptionsOk(options, error)) << error;
    ASSERT_TRUE(error.empty());
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

TEST(PlatformMotor, YamlConversionAndInPlaceReload) {
    printf("[PLT] Load physical disk configurations and reject an invalid tilt reload\n");
    const auto directory = std::filesystem::temp_directory_path() /
        ("platform_config_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const std::filesystem::path path = "machina.yaml";
    ASSERT_TRUE(std::filesystem::create_directory(directory));
    struct Cleanup {
        std::filesystem::path directory_;
        std::filesystem::path originalDirectory_;
        ~Cleanup() {
            std::error_code error;
            std::filesystem::current_path(originalDirectory_, error);
            std::filesystem::remove_all(directory_, error);
        }
    } cleanup{directory, std::filesystem::current_path()};
    std::filesystem::current_path(directory);
    YAML::Node document;
    for (const auto* axis : {"pan", "tilt"}) {
        const bool pan = std::string(axis) == "pan";
        auto node = document[axis];
        node["pinStep"] = pan ? 17 : 18;
        node["pinDir"] = pan ? 22 : 23;
        node["pinEna"] = pan ? 27 : 26;
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
    ComponentStateMove section;
    ASSERT_TRUE(section.load(Config(path.string())));
    section.getMotors(motors);
    ASSERT_NEAR(motors[0].options_.degPulseGeared, 0.125, 1e-8);
    ASSERT_NEAR(motors[1].options_.degPulseGeared, 0.1125, 1e-8);
    ASSERT_NEAR(motors[0].options_.accelMaxDegSec2, 11444.940082303707, 0.1);
    printf("[PLT] Select Smart or Dumb for the paired probe\n");
    ASSERT_TRUE(section.load(Config(path.string())));
    section.getMotors(motors);
    ASSERT_EQ(motors[0].kind_, StepperMotorKind::smart);
    document["motorClass"] = "Smart";
    { std::ofstream file(path); file << document; ASSERT_TRUE(file.good()); }
    ASSERT_TRUE(section.load(Config(path.string())));
    section.getMotors(motors);
    ASSERT_EQ(motors[0].kind_, StepperMotorKind::smart);
    document["motorClass"] = "Dumb";
    for (const auto* axis : {"pan", "tilt"}) {
        document[axis]["dumbSpeedDegSec"] = 90;
        document[axis]["dumbPauseMs"] = 10;
        document[axis]["dumbRemainingPulsesTolerance"] = 3;
    }
    { std::ofstream file(path); file << document; ASSERT_TRUE(file.good()); }
    ASSERT_TRUE(section.load(Config(path.string())));
    section.getMotors(motors);
    ASSERT_EQ(motors[0].kind_, StepperMotorKind::dumb);
    ASSERT_EQ(motors[1].kind_, StepperMotorKind::dumb);
    ASSERT_FLOAT_EQ(motors[0].dumbSpeedDegSec_, 90);
    ASSERT_EQ(motors[1].dumbPause_, 10 * MILLISECOND);
    ASSERT_EQ(motors[1].dumbRemainingPulsesTolerance_, 3U);
    document["tilt"]["dumbPauseMs"] = 60000;
    { std::ofstream file(path); file << document; ASSERT_TRUE(file.good()); }
    ASSERT_FALSE(section.load(Config(path.string())));
    section.getMotors(motors);
    ASSERT_EQ(motors[1].dumbPause_, 10 * MILLISECOND);
    document["motorClass"] = "Unknown";
    { std::ofstream file(path); file << document; ASSERT_TRUE(file.good()); }
    ASSERT_FALSE(section.load(Config(path.string())));
    section.getMotors(motors);
    ASSERT_EQ(motors[0].kind_, StepperMotorKind::dumb);
    ASSERT_EQ(motors[1].kind_, StepperMotorKind::dumb);
    document["motorClass"] = "Dumb";
    document["tilt"]["dumbPauseMs"] = 10;
    document["pan"]["speedMaxDegSec"] = 25;
    document["tilt"]["torqueMaxNm"] = -0.5;
    { std::ofstream file(path); file << document; ASSERT_TRUE(file.good()); }
    ASSERT_FALSE(section.load(Config(path.string())));
    section.getMotors(motors);
    ASSERT_FLOAT_EQ(motors[0].options_.speedMaxDegSec, 180);
    ASSERT_DOUBLE_EQ(motors[1].options_.mechanics_.torqueMaxNm_, 0.5);

    printf("[PLT] Reload Smart after Dumb and clear class-specific settings\n");
    document["motorClass"] = "Smart";
    document["tilt"]["torqueMaxNm"] = 0.5;
    { std::ofstream file(path); file << document; ASSERT_TRUE(file.good()); }
    ASSERT_TRUE(section.load(Config(path.string())));
    section.getMotors(motors);
    for (const auto& motor : motors) {
        ASSERT_EQ(motor.kind_, StepperMotorKind::smart);
        ASSERT_FLOAT_EQ(motor.dumbSpeedDegSec_, 0);
        ASSERT_EQ(motor.dumbPause_, 0);
        ASSERT_EQ(motor.dumbRemainingPulsesTolerance_, 0U);
    }
    ASSERT_NEAR(motors[0].options_.degPulseGeared, 0.125, 1e-8);
    ASSERT_NEAR(motors[1].options_.degPulseGeared, 0.1125, 1e-8);
}
