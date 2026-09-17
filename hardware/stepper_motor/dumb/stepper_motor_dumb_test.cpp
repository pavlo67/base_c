#include "stepper_motor_dumb.h"

#include <gtest/gtest.h>

namespace {

StepperMotorRunConfig dumbConfig() {
    return StepperMotorRunConfig{
        .pinStep_ = 1, .pinDir_ = 2, .pinEna_ = 3,
        .options_ = {10000, 0.1F, 1000, 1000},
        .expecterInterval_ = 5 * MILLISECOND,
        .pulseHigh_ = 10 * MICROSECOND,
        .timeLimit_ = 2 * SECOND
    };
}

TEST(StepperMotorDumbPlan, TimerToleranceCapsTheConstantFrequency) {
    printf("[DMB] Plan 400 fixed pulses with a three-pulse completion window\n");
    StepperMotorDumb motor(dumbConfig(), 500, 15 * MILLISECOND, 3);
    std::array<bool, Gpio::PIN_COUNT> usedPins{};
    ASSERT_EQ(motor.prepare(40, usedPins), StepperMotor::RUNNING);
    ASSERT_EQ(motor.result().seq.size(), 1U);
    const auto& series = motor.result().seq.front();
    ASSERT_EQ(series.expectedPulsesCount_, 400U);
    ASSERT_EQ(series.cruiseFrequency_, 800U);
    ASSERT_EQ(series.intervalAlgorithm_, LINEAR_INTERVAL_ACCELERATION);
    ASSERT_FLOAT_EQ(series.accelerationDegPerSec2_, 0);
    ASSERT_NEAR(series.idealIntervalSec(0, dumbConfig().options_), 0.00125, 1e-8);
    ASSERT_NEAR(series.idealIntervalSec(399, dumbConfig().options_), 0.00125, 1e-8);
    ASSERT_EQ(series.pulsesCount_, 0U);

    printf("[DMB] Clamp the completion window for a one-pulse move\n");
    StepperMotorDumb onePulse(dumbConfig(), 500, 0, 3);
    usedPins.fill(false);
    ASSERT_EQ(onePulse.prepare(0.1F, usedPins), StepperMotor::RUNNING);
    ASSERT_EQ(onePulse.result().seq.front().expectedPulsesCount_, 1U);
    ASSERT_EQ(onePulse.result().seq.front().cruiseFrequency_, 200U);

    printf("[DMB] Reject a speed below the minimum integer PWM command\n");
    StepperMotorDumb tooSlow(dumbConfig(), 0.05F, 0, 3);
    usedPins.fill(false);
    ASSERT_EQ(tooSlow.prepare(40, usedPins), Gpio::INVALID_ARGUMENT);
}

#if defined(SYSTEM_IS_DESKTOP) && SYSTEM_IS_DESKTOP
class StepperMotorDumbTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(Gpio::instance().initialize(), Gpio::SUCCESS); }
    void TearDown() override { ASSERT_EQ(Gpio::instance().terminate(), Gpio::SUCCESS); }
};

TEST_F(StepperMotorDumbTest, StopsWithinToleranceThenWaitsBeforeDisablingMotor) {
    printf("[DMB] Start fixed PWM, stop near target, then finish pause\n");
    StepperMotorDumb motor(dumbConfig(), 500, 15 * MILLISECOND, 3);
    std::array<bool, Gpio::PIN_COUNT> usedPins{};
    ASSERT_EQ(motor.prepare(-40, usedPins), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(0), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(5 * MILLISECOND), StepperMotor::RUNNING);
    PwmSettings pwm;
    ASSERT_EQ(Gpio::instance().getPwmSettings(1, pwm), Gpio::SUCCESS);
    ASSERT_TRUE(pwm.enabled);
    ASSERT_EQ(pwm.frequency, 800U);
    ASSERT_EQ(Gpio::instance().read(2), 0);
    moment last = 5 * MILLISECOND;
    for (moment at = 10 * MILLISECOND; at <= 505 * MILLISECOND; at += 5 * MILLISECOND) {
        ASSERT_EQ(motor.action(at), StepperMotor::RUNNING);
        last = at;
        ASSERT_EQ(Gpio::instance().getPwmSettings(1, pwm), Gpio::SUCCESS);
        if (!pwm.enabled) { break; }
    }
    ASSERT_FALSE(pwm.enabled);
    ASSERT_GE(motor.result().seq.front().pulsesCount_, 397U);
    ASSERT_LE(motor.result().seq.front().pulsesCount_, 400U);
    ASSERT_EQ(Gpio::instance().read(3), 0);
    ASSERT_EQ(motor.action(last + 10 * MILLISECOND), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(last + 15 * MILLISECOND), StepperMotor::COMPLETE);
    ASSERT_EQ(Gpio::instance().read(3), 1);
}

TEST_F(StepperMotorDumbTest, ProbeUsesFixedSpeedPlanner) {
    printf("[DMB] Execute inherited probe with constant-speed planning\n");
    StepperMotorDumb motor(dumbConfig(), 500, 2 * MILLISECOND, 1);
    StepperMotorSeriesSequence real;
    ASSERT_EQ(motor.probe(0.5F, true, "dumb", &real), Gpio::SUCCESS);
    ASSERT_EQ(real.seq.size(), 1U);
    ASSERT_EQ(real.seq.front().intervalAlgorithm_, LINEAR_INTERVAL_ACCELERATION);
    ASSERT_FLOAT_EQ(real.seq.front().accelerationDegPerSec2_, 0);
    ASSERT_FLOAT_EQ(real.seq.front().maxAccelerationDegSec2_, 0);
    ASSERT_GE(real.seq.front().pulsesCount_, 4U);
    ASSERT_LE(real.seq.front().pulsesCount_, 5U);
}
#endif

} // namespace
