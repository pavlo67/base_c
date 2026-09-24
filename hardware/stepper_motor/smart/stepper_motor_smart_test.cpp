#include "../stepper_motor.h"
#include "stepper_motor_smart.h"
#include "hardware/stepper_motor/stepper_motor_test_config.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "lib/mathlib.h"

const stepper_motor_options_t STEPPER_OPTS {
    .freqMax         = 8000,
    .degPulse        = 0.225F,
    .speedMaxDegSec  = 180.0F,
    .accelMaxDegSec2 = 720.0F
};

TEST(stepper_motor_timing, brakingControlUsesModeledTimerAndRemainingPulses) {
    printf("[MODEL] Check exact braking budget, timer sensitivity and both directions\n");
    for (const float direction : {1.0F, -1.0F}) {
        for (const duration timer : {2 * MILLISECOND, 9 * MILLISECOND}) {
            for (const float finalSpeed : {0.0F, 20.0F}) {
                auto reference = getFastestSeries(direction * 90, direction * finalSpeed, STEPPER_OPTS);
                reference.livePwm_ = true;
                reference = evaluateSeries(reference, timer, STEPPER_OPTS);
                const auto model = StepperMotorSmart::getBrakingModel(direction * 90, direction * finalSpeed, timer, STEPPER_OPTS);
                ASSERT_FALSE(model.brakingModel_.empty());
                ASSERT_EQ(model.expectedPulsesCount_, reference.pulsesCount_);
                ASSERT_EQ(model.idealFinalSpeed(STEPPER_OPTS), direction * finalSpeed);
                ASSERT_TRUE(StepperMotorSmart::canBrake(direction * 90, direction * finalSpeed, timer, reference.pulsesCount_, STEPPER_OPTS));
                ASSERT_FALSE(StepperMotorSmart::canBrake(direction * 90, direction * finalSpeed, timer, reference.pulsesCount_ - 1, STEPPER_OPTS));
                const auto replay = evaluateSeries(model, timer, STEPPER_OPTS);
                ASSERT_EQ(replay.pulsesCount_, reference.pulsesCount_);
                ASSERT_EQ(replay.lastInterval_, reference.lastInterval_);
                ASSERT_NEAR(replay.totalSec(), reference.totalSec(), 1e-6);
            }
        }
    }
    ASSERT_GT(StepperMotorSmart::getBrakingModel(90, 0, 9 * MILLISECOND, STEPPER_OPTS).expectedPulsesCount_,
        StepperMotorSmart::getBrakingModel(90, 0, 2 * MILLISECOND, STEPPER_OPTS).expectedPulsesCount_);
    ASSERT_FALSE(StepperMotorSmart::canBrake(90, 0, 0, 1000, STEPPER_OPTS));
    ASSERT_FALSE(StepperMotorSmart::canBrake(90, -20, MILLISECOND, 1000, STEPPER_OPTS));
}

#if defined(SYSTEM_IS_DESKTOP) && SYSTEM_IS_DESKTOP
class StepperMotorTest : public ::testing::Test {
protected:
    const unsigned pinStep_ = testPanHardware.pinStep_;
    const unsigned pinDir_ = testPanHardware.pinDir_;
    const unsigned pinEna_ = testPanHardware.pinEna_;
    std::array<unsigned, 3> otherPins_{};

    void SetUp() override {
        size_t count = 0;
        for (unsigned pin = 0; pin < Gpio::PIN_COUNT && count < otherPins_.size(); ++pin) {
            if (pin == pinStep_ || pin == pinDir_ || pin == pinEna_) { continue; }
            otherPins_[count++] = pin;
        }
        ASSERT_EQ(count, otherPins_.size());
        ASSERT_EQ(Gpio::instance().initialize(), Gpio::SUCCESS);
    }
    void TearDown() override { ASSERT_EQ(Gpio::instance().terminate(), Gpio::SUCCESS); }
};

TEST_F(StepperMotorTest, separateMotorPinsAndCleanup) {
    auto& gpio = Gpio::instance();
    const stepper_motor_options_t options{10000, 0.1F, 100, 1000};
    for (const unsigned pin : otherPins_) {
        ASSERT_EQ(gpio.setMode(pin, GpioMode::output), Gpio::SUCCESS);
        ASSERT_EQ(gpio.write(pin, 1), Gpio::SUCCESS);
    }
    StepperMotorRunConfig config{.pinStep_ = pinStep_, .pinDir_ = pinDir_, .pinEna_ = pinEna_,
        .options_ = options, .pulseHigh_ = MICROSECOND};
    printf("[motor-test] Run first motor\n");
    ASSERT_EQ(StepperMotorSmart(config).probe(0.1F, false), Gpio::SUCCESS);
    ASSERT_EQ(gpio.read(pinStep_), 0);
    ASSERT_EQ(gpio.read(pinDir_), 0);
    ASSERT_EQ(gpio.read(pinEna_), 1);
    for (const unsigned pin : otherPins_) { ASSERT_EQ(gpio.read(pin), 1); }
    printf("[motor-test] Run second motor\n");
    config.pinStep_ = otherPins_[0]; config.pinDir_ = otherPins_[1]; config.pinEna_ = otherPins_[2];
    ASSERT_EQ(StepperMotorSmart(config).probe(0.1F, false), Gpio::SUCCESS);
    ASSERT_EQ(gpio.read(otherPins_[0]), 0);
    ASSERT_EQ(gpio.read(otherPins_[1]), 0);
    ASSERT_EQ(gpio.read(otherPins_[2]), 1);
    ASSERT_EQ(gpio.read(pinEna_), 1);
}

TEST_F(StepperMotorTest, rejectsInvalidPinsAndReportsUninitializedBackend) {
    StepperMotorSeriesSequence sequence;
    sequence.seq.push_back(evaluateSeries(StepperMotorSeries(1, 10, 10, true, STEPPER_OPTS), 0, STEPPER_OPTS));
    printf("[motor-test] Run duplicatePins\n");
    StepperMotorSmart duplicatePins(sequence, pinStep_, pinStep_, pinEna_, STEPPER_OPTS, MICROSECOND);
    ASSERT_EQ(duplicatePins.action(0), Gpio::INVALID_ARGUMENT);
    printf("[motor-test] Run invalidPin\n");
    StepperMotorSmart invalidPin(sequence, Gpio::PIN_COUNT, pinDir_, pinEna_, STEPPER_OPTS, MICROSECOND);
    ASSERT_EQ(invalidPin.action(0), Gpio::INVALID_ARGUMENT);
    ASSERT_EQ(Gpio::instance().terminate(), Gpio::SUCCESS);
    printf("[motor-test] Run uninitialized\n");
    StepperMotorSmart uninitialized(sequence, pinStep_, pinDir_, pinEna_, STEPPER_OPTS, MICROSECOND);
    ASSERT_EQ(uninitialized.action(0), Gpio::NOT_INITIALIZED);
}

TEST_F(StepperMotorTest, externalTicksDrivePwmAndCompletionWithoutReplay) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(3, 10, 10, true, options);
    StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, options, MICROSECOND);
    const moment start = 20 * SECOND;
    ASSERT_EQ(motor.action(start - MILLISECOND), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(start), StepperMotor::RUNNING);
    PwmSettings pwm;
    ASSERT_EQ(Gpio::instance().getPwmSettings(pinStep_, pwm), Gpio::SUCCESS);
    ASSERT_TRUE(pwm.enabled);
    ASSERT_EQ(pwm.frequency, 10);
    ASSERT_EQ(pwm.duty * 2, pwm.range);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 0);
    ASSERT_EQ(motor.action(start + 100 * MILLISECOND), StepperMotor::RUNNING);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 1);
    ASSERT_EQ(motor.action(start + 100 * MILLISECOND), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(start), StepperMotor::RUNNING);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 1);
    ASSERT_EQ(motor.action(start + 200 * MILLISECOND), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(start + 300 * MILLISECOND), StepperMotor::COMPLETE);
    ASSERT_EQ(motor.action(start + SECOND), StepperMotor::COMPLETE);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 3);
    ASSERT_NEAR(motor.result().seq[0].totalSec(), 0.301, 1e-6);
    ASSERT_EQ(Gpio::instance().getPwmSettings(pinStep_, pwm), Gpio::SUCCESS);
    ASSERT_FALSE(pwm.enabled);
    ASSERT_EQ(Gpio::instance().read(pinEna_), 1);
}

TEST_F(StepperMotorTest, delayedUpdatesCountElapsedPwmPeriods) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(2, -10, -10, false, options);
    StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, options, MICROSECOND);
    ASSERT_EQ(motor.action(0), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(MILLISECOND), StepperMotor::RUNNING);
    ASSERT_EQ(Gpio::instance().read(pinDir_), 0);
    ASSERT_EQ(motor.action(551 * MILLISECOND), StepperMotor::RUNNING);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 5);
    ASSERT_EQ(motor.action(651 * MILLISECOND), StepperMotor::COMPLETE);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 6);
    ASSERT_NEAR(motor.result().seq[0].finalSpeed(options), -10, 1e-6);
}

TEST_F(StepperMotorTest, hardwarePwmModeAndDestructorCleanup) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(20, 10, 10, true, options);
    {
        StepperMotorSmart motor(plan, 18, 2, 3, options, MICROSECOND, true);
        ASSERT_EQ(motor.action(0), StepperMotor::RUNNING);
        ASSERT_EQ(motor.action(MILLISECOND), StepperMotor::RUNNING);
        ASSERT_EQ(Gpio::instance().read(18), Gpio::WRONG_MODE);
        PwmSettings pwm;
        ASSERT_EQ(Gpio::instance().getPwmSettings(18, pwm), Gpio::SUCCESS);
        ASSERT_TRUE(pwm.enabled);
    }
    PwmSettings pwm;
    ASSERT_EQ(Gpio::instance().getPwmSettings(18, pwm), Gpio::SUCCESS);
    ASSERT_FALSE(pwm.enabled);
    ASSERT_EQ(Gpio::instance().read(3), 1);
    StepperMotorSmart invalid(plan, 17, 2, 3, options, MICROSECOND, true);
    ASSERT_EQ(invalid.action(0), Gpio::NOT_SUPPORTED);
}

TEST_F(StepperMotorTest, watchdogStopsPwmBeforeNextLongTick) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorRunConfig config{.pinStep_ = pinStep_, .pinDir_ = pinDir_, .pinEna_ = pinEna_,
        .options_ = options, .expecterInterval_ = 100 * MICROSECOND,
        .pulseHigh_ = MICROSECOND, .timeLimit_ = 2 * MILLISECOND};
    ASSERT_EQ(StepperMotorSmart(config).probe(100, false), StepperMotor::TIME_LIMIT);
    PwmSettings pwm;
    ASSERT_EQ(Gpio::instance().getPwmSettings(pinStep_, pwm), Gpio::SUCCESS);
    ASSERT_FALSE(pwm.enabled);
    ASSERT_EQ(Gpio::instance().read(pinEna_), 1);
    config.expecterInterval_ = SECOND;
    config.timeLimit_ = MILLISECOND;
    ASSERT_EQ(StepperMotorSmart(config).probe(100, false), StepperMotor::TIME_LIMIT);
}

TEST_F(StepperMotorTest, invalidPeriodAndPulseWidthAreErrorsWithCleanup) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(1, 0.5F, 0.5F, true, options);
    StepperMotorSmart slow(plan, pinStep_, pinDir_, pinEna_, options, MICROSECOND);
    ASSERT_EQ(slow.action(0), StepperMotor::RUNNING);
    ASSERT_EQ(slow.action(MILLISECOND), Gpio::INVALID_ARGUMENT);
    ASSERT_EQ(Gpio::instance().read(pinEna_), 1);
    plan.seq.clear();
    plan.seq.emplace_back(1, 100, 100, true, options);
    StepperMotorSmart wide(plan, pinStep_, pinDir_, pinEna_, options, 10 * MILLISECOND);
    ASSERT_EQ(wide.action(0), StepperMotor::RUNNING);
    ASSERT_EQ(wide.action(20 * MILLISECOND), Gpio::INVALID_ARGUMENT);
    ASSERT_EQ(Gpio::instance().read(pinEna_), 1);
}

TEST_F(StepperMotorTest, runReturnsRuntimeStatisticsAndQuantizedFrequency) {
    const stepper_motor_options_t options{10000, 0.1F, 100, 1000};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(2, 10.35F, 10.35F, true, options);
    printf("[motor-test] Run and inspect runtime statistics\n");
    StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, options, MICROSECOND);
    StepperMotor& base = motor;
    int status = StepperMotor::RUNNING;
    for (moment at = 0; at < SECOND && status == StepperMotor::RUNNING; at += MILLISECOND) {
        status = base.action(at);
    }
    ASSERT_EQ(status, StepperMotor::COMPLETE);
    const auto& real = motor.result();
    ASSERT_EQ(real.seq.size(), 1);
    ASSERT_TRUE(real.seq[0].finished_);
    ASSERT_GE(real.seq[0].pulsesCount_, 2);
    ASSERT_GT(real.seq[0].startedAt_, 0);
    ASSERT_GT(real.seq[0].totalSec(), 0);
    ASSERT_NEAR(real.seq[0].finalSpeed(options), 10.3, 1e-4);
    ASSERT_EQ(plan.seq[0].pulsesCount_, 0);
}

TEST_F(StepperMotorTest, emptySequenceDoesNotTouchPins) {
    auto& gpio = Gpio::instance();
    ASSERT_EQ(gpio.setMode(pinStep_, GpioMode::output), Gpio::SUCCESS);
    ASSERT_EQ(gpio.write(pinStep_, 1), Gpio::SUCCESS);
    StepperMotorSmart empty({}, pinStep_, pinDir_, pinEna_, STEPPER_OPTS, MICROSECOND);
    ASSERT_EQ(empty.action(0), StepperMotor::COMPLETE);
    ASSERT_EQ(gpio.read(pinStep_), 1);
}
TEST_F(StepperMotorTest, liveSequenceUsesRuntimeBrakingAndFiniteTerminalSpeed) {
    printf("[LIVE] Check predictive braking and finite terminal speed\n");
    const auto plan = getSeriesSequence(0, 90, 0, 5 * MILLISECOND, STEPPER_OPTS);
    ASSERT_TRUE(plan.error.empty());
    StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, STEPPER_OPTS, MICROSECOND);
    int status = StepperMotor::RUNNING;
    for (moment at = 0; at < 5 * SECOND && status == StepperMotor::RUNNING; at += 5 * MILLISECOND) {
        status = motor.action(at);
    }
    ASSERT_EQ(status, StepperMotor::COMPLETE);
    const auto& real = motor.result();
    ASSERT_EQ(real.seq.size(), 3);
    ASSERT_LT(real.seq[1].accelerationDegPerSec2_, 0);
    ASSERT_FALSE(real.seq[1].brakingModel_.empty());
    ASSERT_EQ(real.seq[1].modelInterval_, 5 * MILLISECOND);
    ASSERT_EQ(real.seq[1].minimumPulsesCount_, 0U);
    ASSERT_NEAR(real.seq[1].initialSpeedDegPerSec_, real.seq[0].finalSpeed(STEPPER_OPTS), 1e-5);
    ASSERT_NEAR(real.seq.back().finalSpeed(STEPPER_OPTS),
        STEPPER_OPTS.degPulse * std::floor(1.0 / (2 * std::sqrt(STEPPER_OPTS.degPulse / STEPPER_OPTS.accelMaxDegSec2))), 1e-5);
    for (const auto& series : real.seq) {
        ASSERT_TRUE(series.finished_);
        ASSERT_LE(series.maxSpeedDegSec_, STEPPER_OPTS.speedMaxDegSec + SPEED_EPS);
    }
    ASSERT_LE(real.seq.front().maxAccelerationDegSec2_, STEPPER_OPTS.accelMaxDegSec2 * 1.01F);
}
TEST_F(StepperMotorTest, probeWithEstimatesPreservesSharedGpioAndRunsSignedRequestedAngles) {
    printf("[RUN] Execute small signed angles through the shared helper\n");
    const StepperMotorRunConfig cfg {
        .pinStep_ = pinStep_, .pinDir_ = pinDir_, .pinEna_ = pinEna_,
        .options_ = STEPPER_OPTS, .expecterInterval_ = 5 * MILLISECOND,
        .pulseHigh_ = 15 * MICROSECOND, .timeLimit_ = 2 * SECOND
    };
    ASSERT_EQ(Gpio::instance().setMode(otherPins_[0], GpioMode::output), Gpio::SUCCESS);
    ASSERT_EQ(Gpio::instance().write(otherPins_[0], 1), Gpio::SUCCESS);
    for (const float angle : {0.5F, -0.5F}) {
        StepperMotorSeriesSequence real;
        testing::internal::CaptureStdout();
        const int result = StepperMotorSmart(cfg).probe(angle, true, "test", &real);
        const auto output = testing::internal::GetCapturedStdout();
        ASSERT_EQ(result, Gpio::SUCCESS) << output;
        ASSERT_TRUE(real.error.empty());
        ASSERT_FALSE(real.seq.empty());
        uint64_t pulses = 0;
        for (const auto& series : real.seq) {
            ASSERT_EQ(series.directionForward_, angle > 0);
            ASSERT_TRUE(series.finished_);
            pulses += series.pulsesCount_;
        }
        ASSERT_GE(pulses, 2U);
        ASSERT_LT(pulses, 10U);
        ASSERT_NE(output.find("[test] ideal TOTAL:"), std::string::npos);
        ASSERT_NE(output.find("[test] clocked TOTAL:"), std::string::npos);
        ASSERT_NE(output.find("[test] real TOTAL:"), std::string::npos);
        ASSERT_EQ(Gpio::instance().read(cfg.pinEna_), 1);
        ASSERT_EQ(Gpio::instance().read(otherPins_[0]), 1);
    }
}

TEST_F(StepperMotorTest, probeWithEstimatesValidatesBeforePlanningAndCleansUpTimeout) {
    printf("[RUN] Check zero, invalid options and watchdog cleanup\n");
    StepperMotorRunConfig cfg {
        .pinStep_ = pinStep_, .pinDir_ = pinDir_, .pinEna_ = pinEna_,
        .options_ = STEPPER_OPTS, .expecterInterval_ = 5 * MILLISECOND,
        .pulseHigh_ = 15 * MICROSECOND, .timeLimit_ = MILLISECOND
    };
    ASSERT_EQ(Gpio::instance().setMode(cfg.pinEna_, GpioMode::output), Gpio::SUCCESS);
    ASSERT_EQ(Gpio::instance().write(cfg.pinEna_, 1), Gpio::SUCCESS);
    StepperMotorSeriesSequence real;
    ASSERT_EQ(StepperMotorSmart(cfg).probe(0, true, "zero", &real), Gpio::SUCCESS);
    ASSERT_TRUE(real.seq.empty());
    ASSERT_EQ(Gpio::instance().read(cfg.pinEna_), 1);
    cfg.options_.degPulse = 0;
    ASSERT_EQ(StepperMotorSmart(cfg).probe(0.5F, true, "invalid", &real), Gpio::INVALID_ARGUMENT);
    ASSERT_FALSE(real.error.empty());
    ASSERT_EQ(Gpio::instance().read(cfg.pinEna_), 1);
    cfg.options_ = STEPPER_OPTS;
    ASSERT_EQ(StepperMotorSmart(cfg).probe(0.5F, true, "timeout", &real), StepperMotor::TIME_LIMIT);
    ASSERT_FALSE(real.error.empty());
    ASSERT_FALSE(real.seq.empty());
    ASSERT_EQ(Gpio::instance().read(cfg.pinEna_), 1);
}

TEST_F(StepperMotorTest, probeWithoutEstimatesReportsOnlyTheExecutedMove) {
    printf("[RUN] Execute a real move without ideal or clocked reports\n");
    const StepperMotorRunConfig cfg {
        .pinStep_ = pinStep_, .pinDir_ = pinDir_, .pinEna_ = pinEna_,
        .options_ = STEPPER_OPTS, .expecterInterval_ = 5 * MILLISECOND,
        .pulseHigh_ = 15 * MICROSECOND, .timeLimit_ = 2 * SECOND
    };
    StepperMotorSeriesSequence real;
    StepperMotorSmart smart(cfg);
    StepperMotor& motor = smart;
    testing::internal::CaptureStdout();
    const int result = motor.probe(0.5F, false, "real-only", &real);
    const auto output = testing::internal::GetCapturedStdout();
    ASSERT_EQ(result, Gpio::SUCCESS) << output;
    ASSERT_FALSE(real.seq.empty());
    ASSERT_NE(output.find("[real-only] real TOTAL:"), std::string::npos);
    ASSERT_EQ(output.find("[real-only] ideal"), std::string::npos);
    ASSERT_EQ(output.find("[real-only] clocked"), std::string::npos);
}

TEST_F(StepperMotorTest, brakingRetainsTargetAfterSimulatedHalfwayOvershoot) {
    printf("[STEP] Preserve all 147 pulses for signed 33-degree moves\n");
    for (const float angle : {33.0F, -33.0F}) {
        const auto plan = getSeriesSequence(0, angle, 0, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(plan.error.empty());
        ASSERT_EQ(plan.seq.front().stopAfterPulses_, 73U);
        ASSERT_EQ(plan.seq.front().pulsesCount_, 74U);
        StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, STEPPER_OPTS, 15 * MICROSECOND);
        int status = StepperMotor::RUNNING;
        for (moment at = 0; at < 5 * SECOND && status == StepperMotor::RUNNING; at += 5 * MILLISECOND) {
            status = motor.action(at);
        }
        ASSERT_EQ(status, StepperMotor::COMPLETE);
        const auto& real = motor.result();
        ASSERT_GT(real.seq.front().pulsesCount_, plan.seq.front().stopAfterPulses_);
        ASSERT_EQ(real.seq.size(), 3U);
        const auto& braking = real.seq[1];
        ASSERT_FALSE(braking.brakingModel_.empty());
        ASSERT_EQ(braking.expectedPulsesCount_ + real.seq.front().pulsesCount_, 146U);
        ASSERT_EQ(braking.minimumPulsesCount_, 0U);
        auto oldBraking = getFastestSeries(real.seq[0].finalSpeed(STEPPER_OPTS), 0, STEPPER_OPTS);
        oldBraking.livePwm_ = true;
        oldBraking.minimumPulsesCount_ = braking.expectedPulsesCount_;
        oldBraking = evaluateSeries(oldBraking, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_LT(braking.totalSec(), oldBraking.totalSec());
        printf("[LIVE] %.0f deg: acceleration=%lu pulses, braking=%.3f s, old braking=%.3f s\n",
            angle, real.seq[0].pulsesCount_, braking.totalSec(), oldBraking.totalSec());
        uint64_t total = 0;
        for (const auto& series : real.seq) {
            total += series.pulsesCount_;
            ASSERT_EQ(series.directionForward_, angle > 0);
        }
        ASSERT_EQ(total, 147U);
    }
}

TEST_F(StepperMotorTest, predictiveBrakingReplacesPlannedCruiseWithUnevenTicks) {
    printf("[LIVE] Predict braking for both directions with uneven external timer ticks\n");
    for (const float angle : {33.0F, -33.0F, 180.0F, -180.0F}) {
        const auto plan = getSeriesSequence(0, angle, 0, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(plan.error.empty());
        StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, STEPPER_OPTS, 15 * MICROSECOND);
        int status = StepperMotor::RUNNING;
        unsigned tick = 0;
        for (moment at = 0; at < 5 * SECOND && status == StepperMotor::RUNNING;
                at += (++tick % 2 ? 4 : 6) * MILLISECOND) {
            status = motor.action(at);
        }
        ASSERT_EQ(status, StepperMotor::COMPLETE);
        const auto& real = motor.result();
        ASSERT_EQ(real.seq.size(), 3U);
        ASSERT_GT(real.seq[0].accelerationDegPerSec2_, 0);
        ASSERT_LT(real.seq[1].accelerationDegPerSec2_, 0);
        ASSERT_FALSE(real.seq[1].brakingModel_.empty());
        ASSERT_LE(real.seq[0].maxAccelerationDegSec2_, STEPPER_OPTS.accelMaxDegSec2 * 1.01F);
        uint64_t pulses = 0;
        for (const auto& section : real.seq) {
            ASSERT_TRUE(section.finished_);
            ASSERT_EQ(section.directionForward_, angle > 0);
            ASSERT_LE(section.maxSpeedDegSec_, STEPPER_OPTS.speedMaxDegSec + SPEED_EPS);
            pulses += section.pulsesCount_;
        }
        const uint64_t target = STEPPER_OPTS.pulsesForDeg(angle, angle > 0);
        ASSERT_GE(pulses, target);
        ASSERT_LE(pulses, target + 5);
    }
}

TEST_F(StepperMotorTest, brakingFreezesMeanSinceAccelerationAndHandlesLaterDelays) {
    printf("[LIVE] Exclude startup, ignore duplicate ticks, freeze mean and delay braking updates\n");
    for (const duration planTimer : {duration(0), 5 * MILLISECOND}) {
        for (const float angle : {33.0F, -33.0F, 720.0F, -720.0F}) {
            const auto plan = getSeriesSequence(0, angle, 0, planTimer, STEPPER_OPTS);
            ASSERT_TRUE(plan.error.empty());
            StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, STEPPER_OPTS, 15 * MICROSECOND);
            ASSERT_EQ(motor.action(0), StepperMotor::RUNNING);
            // A large startup delay must not become the model timer.
            moment at = 100 * MILLISECOND;
            ASSERT_EQ(motor.action(at), StepperMotor::RUNNING);
            const moment started = motor.result().seq.front().startedAt_;
            uint64_t samples = 0;
            while (!motor.result().seq.front().finished_ && at < 10 * SECOND) {
                at += (++samples % 2 ? 1 : 9) * MILLISECOND;
                ASSERT_EQ(motor.action(at), StepperMotor::RUNNING);
                ASSERT_EQ(motor.action(at), StepperMotor::RUNNING);
                ASSERT_EQ(motor.action(at - 1), StepperMotor::RUNNING);
            }
            ASSERT_TRUE(motor.result().seq.front().finished_);
            ASSERT_EQ(motor.result().seq.size(), 3U);
            const duration mean = (at - started) / samples;
            ASSERT_EQ(motor.result().seq[1].modelInterval_, mean);
            ASSERT_LT(mean, 6 * MILLISECOND);
            const auto frozen = motor.result().seq[1].brakingModel_;
            ASSERT_FALSE(frozen.empty());
            int status = StepperMotor::RUNNING;
            unsigned brakingTicks = 0;
            while (status == StepperMotor::RUNNING && at < 20 * SECOND) {
                at += (++brakingTicks % 3 ? 5 : 20) * MILLISECOND;
                status = motor.action(at);
            }
            ASSERT_EQ(status, StepperMotor::COMPLETE);
            const auto& real = motor.result();
            ASSERT_EQ(real.seq[1].modelInterval_, mean);
            ASSERT_EQ(real.seq[1].brakingModel_.size(), frozen.size());
            for (size_t i = 0; i < frozen.size(); ++i) {
                ASSERT_EQ(real.seq[1].brakingModel_[i].pulses_, frozen[i].pulses_);
                ASSERT_EQ(real.seq[1].brakingModel_[i].interval_, frozen[i].interval_);
            }
            uint64_t pulses = 0;
            for (const auto& section : real.seq) {
                ASSERT_TRUE(section.finished_);
                ASSERT_LE(section.maxSpeedDegSec_, STEPPER_OPTS.speedMaxDegSec + SPEED_EPS);
                pulses += section.pulsesCount_;
            }
            ASSERT_LE(real.seq[0].maxAccelerationDegSec2_, STEPPER_OPTS.accelMaxDegSec2 * 1.01F);
            ASSERT_GE(pulses, STEPPER_OPTS.pulsesForDeg(angle, angle > 0));
            ASSERT_LE(pulses, STEPPER_OPTS.pulsesForDeg(angle, angle > 0) + 5);
            ASSERT_NEAR(real.seq.back().finalSpeed(STEPPER_OPTS), (angle > 0 ? 1.F : -1.F) * STEPPER_OPTS.degPulse *
                std::floor(1.0 / (2 * std::sqrt(STEPPER_OPTS.degPulse / STEPPER_OPTS.accelMaxDegSec2))), SPEED_EPS);
        }
    }
}

TEST_F(StepperMotorTest, predictiveBrakingPreservesNonzeroFinalSpeed) {
    printf("[LIVE] Finish both directions at a finite nonzero base speed\n");
    for (const float direction : {1.0F, -1.0F}) {
        const auto plan = getSeriesSequence(direction * 20, direction * 180, direction * 20,
            5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(plan.error.empty());
        StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, STEPPER_OPTS, 15 * MICROSECOND);
        int status = StepperMotor::RUNNING;
        for (moment at = 0; at < 5 * SECOND && status == StepperMotor::RUNNING; at += 5 * MILLISECOND) {
            status = motor.action(at);
        }
        ASSERT_EQ(status, StepperMotor::COMPLETE);
        const auto& real = motor.result();
        ASSERT_EQ(real.seq.size(), 2U);
        ASSERT_FALSE(real.seq.back().brakingModel_.empty());
        ASSERT_NEAR(real.seq.back().idealFinalSpeed(STEPPER_OPTS), direction * 20, SPEED_EPS);
        // finalSpeed is the last pulse's average, not the kinematic endpoint.
        const float lastPulseMaximum = (20 + std::sqrt(400 + 2 * STEPPER_OPTS.accelMaxDegSec2 *
            STEPPER_OPTS.degPulse)) / 2;
        ASSERT_GE(std::abs(real.seq.back().finalSpeed(STEPPER_OPTS)), 20 - STEPPER_OPTS.degPulse);
        ASSERT_LE(std::abs(real.seq.back().finalSpeed(STEPPER_OPTS)), lastPulseMaximum + SPEED_EPS);
        ASSERT_GE(real.seq[0].pulsesCount_ + real.seq[1].pulsesCount_, 800U);
    }
}

TEST_F(StepperMotorTest, exhaustedBrakingBudgetSkipsTailAfterLargeDelay) {
    printf("[LIVE] Retain overshoot observations and skip an exhausted braking budget\n");
    const auto plan = getSeriesSequence(0, 33, 0, 5 * MILLISECOND, STEPPER_OPTS);
    ASSERT_TRUE(plan.error.empty());
    StepperMotorSmart motor(plan, pinStep_, pinDir_, pinEna_, STEPPER_OPTS, 15 * MICROSECOND);
    ASSERT_EQ(motor.action(0), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(5 * MILLISECOND), StepperMotor::RUNNING);
    ASSERT_EQ(motor.action(5 * SECOND), StepperMotor::RUNNING);
    ASSERT_EQ(motor.result().seq.size(), 2U);
    ASSERT_GT(motor.result().seq.front().pulsesCount_, 146U);
    ASSERT_EQ(motor.action(5 * SECOND + 100 * MILLISECOND), StepperMotor::COMPLETE);
    ASSERT_TRUE(motor.result().error.empty());
    // The 100-ms observation gap spans two natural terminal PWM periods.
    ASSERT_EQ(motor.result().seq.back().pulsesCount_, 2U);
}
#endif
