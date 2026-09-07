#include "stepper_motor.h"
#include "hardware/hardware.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "lib/mathlib.h"

const stepper_motor_options_t STEPPER_OPTS {
    .freqMax         = FREQ_MAX_DEFAULT,
    .degPulse        = DEG_PULSE_DEFAULT,
    .speedMaxDegSec  = SPEED_MAX_DEG_SEC,
    .accelMaxDegSec2 = ACCEL_MAX_DEG_SEC2
};

const float INITIAL_SPEED     =  0.0F;
const float FINAL_SPEED       = 80.0F;
const bool  DIRECTION_FORWARD = FINAL_SPEED - INITIAL_SPEED > 0;
const float LIMIT1            = -2.25F;
const float LIMIT2            =  2.25F;
const float TARGET_CHANGE_DEG = 91.0F;


void testSeries(const StepperMotorSeries& series, float totalRotationDegExpected, float finalSpeedDegPerSecExpected, bool directionForwardExpected, stepper_motor_options_t stepperOpts, stepper_motor_algorithm_t intervalAlgorithm, const std::string& verboseLabel) {
    if (!verboseLabel.empty()) {
        series.log(stepperOpts, verboseLabel.c_str());
    }

    ASSERT_EQ(series.directionForward_, directionForwardExpected);
    ASSERT_EQ(series.intervalAlgorithm_, intervalAlgorithm);

    if (!isnanf(finalSpeedDegPerSecExpected)) {
        ASSERT_NEAR(series.idealFinalSpeed(stepperOpts), finalSpeedDegPerSecExpected, std::max(SPEED_EPS, std::abs(finalSpeedDegPerSecExpected) * RESULT_EPS_RATIO));
    }
    if (!isnanf(totalRotationDegExpected)) {
        ASSERT_NEAR(series.expectedRotationDeg(stepperOpts), totalRotationDegExpected, std::max(stepperOpts.degPulse, totalRotationDegExpected * RESULT_EPS_RATIO));
    }

    if (series.expectedPulsesCount_ > 1) {
        if (series.idealFinalSpeed(stepperOpts) > series.initialSpeedDegPerSec_) {
            ASSERT_LT(series.idealIntervalSec(series.expectedPulsesCount_ - 1, STEPPER_OPTS), series.idealIntervalSec(0, STEPPER_OPTS));
        } else if (series.idealFinalSpeed(stepperOpts) < series.initialSpeedDegPerSec_) {
            ASSERT_GT(series.idealIntervalSec(series.expectedPulsesCount_ - 1, STEPPER_OPTS), series.idealIntervalSec(0, STEPPER_OPTS));
        } else {
            ASSERT_NEAR(series.idealIntervalSec(series.expectedPulsesCount_ - 1, STEPPER_OPTS), series.idealIntervalSec(0, STEPPER_OPTS), EPS);
        }
    }

    float speed         = series.initialSpeedDegPerSec_;
    float accelErrorMax = 0;
    float intervalPrev  = series.initialSpeedDegPerSec_ < EPS ? 0 :  STEPPER_OPTS.degPulse / series.initialSpeedDegPerSec_;

    StepperMotorSeries executing = series;
    executing.reset();
    moment at = 0;
    for (uint64_t pulseIndex = 0; pulseIndex < series.expectedPulsesCount_; ++pulseIndex) {
        const float intervalSec = executing.intervalSec(at, STEPPER_OPTS);
        at = executing.nextPulseAt_;
        const float nextSpeed   = 2.0F * STEPPER_OPTS.degPulse / intervalSec - speed;
        const float accel       = std::abs(intervalSec - intervalPrev) <= EPS ? 0
                                : (nextSpeed * nextSpeed - speed * speed) / (2.0F * STEPPER_OPTS.degPulse);

        // printf("intervalSec: %f, speed: %f, nextSpeed: %f, accel: %f, accelerationDegPerSec2_: %f, accelErrorMax: %f\n", interval, speed, nextSpeed, accel, series.accelerationDegPerSec2_, accelErrorMax);

        accelErrorMax = std::max(accelErrorMax,std::abs(accel - series.accelerationDegPerSec2_));
        speed         = nextSpeed;
        intervalPrev  = intervalSec;
    }
    ASSERT_LE(accelErrorMax, std::max(ACCELERATION_EPS, std::abs(series.accelerationDegPerSec2_) * RESULT_EPS_RATIO));

    if (!verboseLabel.empty()) {
        printf("%s: accelErrorMax          : %9.3f\n", verboseLabel.c_str(), accelErrorMax);
    }

    // return accelErrorMax;
    // assert(std::abs(series.expectedRotationDeg(stepperOpts) - totalRotationDeg) < stepperOpts.degreesPerPulse);
}

void testResult(float calculatedTotalRotationDeg) {
    ASSERT_NEAR(calculatedTotalRotationDeg, TARGET_CHANGE_DEG, std::max(STEPPER_OPTS.degPulse, TARGET_CHANGE_DEG * RESULT_EPS_RATIO));
    printf("\nRESULT!!! calculatedTotalRotationDeg: %f, TARGET_CHANGE_DEG: %f, STEPPER_OPTS.degreesPerPulse: %f\n\n", calculatedTotalRotationDeg, TARGET_CHANGE_DEG, STEPPER_OPTS.degPulse);
}

TEST(stepper_motor_test, stepper_motor_test) {
    const StepperMotorSeries seriesExact = getFastestSeries(INITIAL_SPEED, FINAL_SPEED, STEPPER_OPTS,CONSTANT_ACCELERATION);
    testSeries(seriesExact, NAN, FINAL_SPEED, DIRECTION_FORWARD, STEPPER_OPTS, CONSTANT_ACCELERATION, "exact");

    // const PulseSeries fallback = getFastestSeries(INITIAL_SPEED, FINAL_SPEED, STEPPER_OPTS, LINEAR_INTERVAL_ACCELERATION);
    // const float fallbackAccelErrorMax = testSeries(fallback, FINAL_SPEED, DIRECTION_FORWARD, STEPPER_OPTS, LINEAR_INTERVAL_ACCELERATION);
    //
    // printf("fallbackAccelErrorMax: %f\n", fallbackAccelErrorMax);

    // assert(fallback.pulseCount_ == exact.pulseCount_);
    // assert(fallbackAccelErrorMax >= exactAccelErrorMax);

    StepperMotorSeries seriesLimit1 = seriesExact;
    seriesLimit1.limitWithDeg(LIMIT1, STEPPER_OPTS);
    testSeries(seriesLimit1, 0, NAN, DIRECTION_FORWARD, STEPPER_OPTS, seriesExact.intervalAlgorithm_, "limit1");

    StepperMotorSeries seriesLimit2 = seriesExact;
    seriesLimit2.limitWithDeg(LIMIT2, STEPPER_OPTS);
    testSeries(seriesLimit2, LIMIT2, NAN, DIRECTION_FORWARD, STEPPER_OPTS, seriesExact.intervalAlgorithm_, "limit2");

    const StepperMotorSeriesSequence sequenceCalculated = getSeriesSequence(0.0F, TARGET_CHANGE_DEG,0.0F, 0, STEPPER_OPTS);
    ASSERT_TRUE(sequenceCalculated.error.empty());
    ASSERT_FALSE(sequenceCalculated.seq.empty());

    float calculatedTotalRotationDeg = 0.0F;
    int i = 0;
    for (const StepperMotorSeries& series : sequenceCalculated.seq) {
        calculatedTotalRotationDeg += series.expectedRotationDeg(STEPPER_OPTS);
        testSeries(series, NAN, NAN, TARGET_CHANGE_DEG >= 0, STEPPER_OPTS, CONSTANT_ACCELERATION, "calculated" + std::to_string(i++));
    }

    testResult(calculatedTotalRotationDeg);
}

TEST(stepper_motor_timing, pwmUpdateWaitsForCurrentBoundary) {
    const stepper_motor_options_t opts{1000, 1, 100, 100};
    StepperMotorSeries series(10, 10, 20, true, opts);
    const moment start = 123 * SECOND;
    ASSERT_GT(series.intervalSec(start, opts), 0);
    const duration first = series.activeInterval_;
    ASSERT_GT(series.intervalSec(start + first / 2, opts), 0);
    ASSERT_EQ(series.pulsesCount_, 0);
    ASSERT_EQ(series.intervalIndex_, 0);
    const moment update = start + first + first / 2;
    ASSERT_GT(series.intervalSec(update, opts), 0);
    ASSERT_EQ(series.pulsesCount_, 1);
    ASSERT_EQ(series.firstPulseAt_, start + first);
    ASSERT_EQ(series.nextPulseAt_, start + 2 * first);
    ASSERT_EQ(series.activeInterval_, first);
    ASSERT_GT(series.pendingInterval_, 0);
    const duration pending = series.pendingInterval_;
    ASSERT_GT(series.intervalSec(start + 2 * first, opts), 0);
    ASSERT_EQ(series.pulsesCount_, 2);
    ASSERT_EQ(series.lastInterval_, first);
    ASSERT_EQ(series.activeInterval_, pending);
    ASSERT_EQ(series.nextPulseAt_, start + 2 * first + pending);
    ASSERT_EQ(series.intervalIndex_, 1);
    ASSERT_NEAR(series.totalSec(), 2.0 * first / SECOND, 1e-7);
    ASSERT_NEAR(series.finalSpeed(opts), static_cast<double>(SECOND) / first, 1e-5);
}

TEST(stepper_motor_timing, lateTimerCountsEveryRepeatedPulse) {
    const stepper_motor_options_t opts{1000, 1, 100, 100};
    StepperMotorSeries series(4, 10, 10, true, opts);
    ASSERT_GT(series.intervalSec(0, opts), 0);
    const duration period = series.activeInterval_;
    ASSERT_GT(series.intervalSec(5 * period + period / 2, opts), 0);
    ASSERT_EQ(series.pulsesCount_, 5);
    ASSERT_EQ(series.intervalIndex_, 1);
    ASSERT_EQ(series.firstPulseAt_, period);
    ASSERT_EQ(series.lastPulseAt_, 5 * period);
    ASSERT_EQ(series.expectedPulsesCount_, 4);
    const uint64_t count = series.pulsesCount_;
    ASSERT_GT(series.intervalSec(period, opts), 0);
    ASSERT_EQ(series.pulsesCount_, count);
}

TEST(stepper_motor_timing, zeroTimerPreservesIdealIntervals) {
    for (const bool forward : {true, false}) {
        const float direction = forward ? 1 : -1;
        const StepperMotorSeries plan(100, direction * 10, direction * 20, forward, STEPPER_OPTS);
        const auto actual = evaluateSeries(plan, 0, STEPPER_OPTS);
        ASSERT_EQ(actual.pulsesCount_, plan.expectedPulsesCount_);
        ASSERT_NEAR(actual.totalSec(), plan.idealTotalSec(STEPPER_OPTS), 1e-6);
        ASSERT_NEAR(actual.finalSpeed(STEPPER_OPTS), direction * STEPPER_OPTS.degPulse /
            plan.idealIntervalSec(99, STEPPER_OPTS), 1e-3);
        ASSERT_EQ(actual.observedAt_, actual.lastPulseAt_);
        ASSERT_TRUE(actual.finished_);
    }
}

TEST(stepper_motor_timing, clockedAccelerationSwitchesAtHalfActualAngle) {
    for (const float angle : {90.0F, -180.0F}) {
        const auto sequence = getSeriesSequence(0, angle, 0, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(sequence.error.empty()) << sequence.error;
        ASSERT_EQ(sequence.seq.size(), 2);
        const auto& acceleration = sequence.seq[0];
        const auto& deceleration = sequence.seq[1];
        const uint64_t halfPulses = (STEPPER_OPTS.pulsesForDeg(angle, angle > 0) + 1) / 2;
        ASSERT_GE(acceleration.pulsesCount_, halfPulses);
        auto before = evaluateSeries(acceleration, 5 * MILLISECOND, STEPPER_OPTS, 0, halfPulses);
        ASSERT_EQ(before.observedAt_, acceleration.observedAt_);
        ASSERT_EQ(deceleration.startedAt_, acceleration.observedAt_);
        ASSERT_NEAR(deceleration.initialSpeedDegPerSec_, acceleration.finalSpeed(STEPPER_OPTS), 1e-5);
        ASSERT_NEAR(deceleration.idealFinalSpeed(STEPPER_OPTS), 0, SPEED_EPS);
        ASSERT_GT(deceleration.pulsesCount_, 0);
        StepperMotorSeries previous = acceleration;
        previous.reset();
        previous.repeatLastInterval_ = true;
        ASSERT_GT(previous.intervalSec(0, STEPPER_OPTS), 0);
        for (moment at = 5 * MILLISECOND; at < acceleration.observedAt_; at += 5 * MILLISECOND) {
            ASSERT_GT(previous.intervalSec(at, STEPPER_OPTS), 0);
        }
        ASSERT_LT(previous.pulsesCount_, halfPulses);
    }
}

TEST(stepper_motor_timing, emptySeriesAndDuplicateMoments) {
    StepperMotorSeries empty(0, 0, 0, true, STEPPER_OPTS);
    ASSERT_EQ(empty.intervalSec(0, STEPPER_OPTS), 0);
    ASSERT_EQ(empty.totalSec(), 0);
    ASSERT_EQ(empty.finalSpeed(STEPPER_OPTS), 0);
    StepperMotorSeries series(2, 10, 20, true, STEPPER_OPTS);
    const float first = series.intervalSec(0, STEPPER_OPTS);
    ASSERT_EQ(series.intervalSec(0, STEPPER_OPTS), first);
    ASSERT_EQ(series.intervalIndex_, 0);
}

TEST(stepper_motor_timing, replayMatchesEvaluatedSequence) {
    for (const duration timer : {duration(0), 5 * MILLISECOND, 10 * MILLISECOND}) {
        const auto sequence = getSeriesSequence(0, 90, 0, timer, STEPPER_OPTS);
        ASSERT_TRUE(sequence.error.empty());
        for (const auto& series : sequence.seq) {
            std::vector<moment> pulses;
            const auto replay = evaluateSeries(series, timer, STEPPER_OPTS, series.startedAt_,
                series.pulsesCount_, [&](moment at) { pulses.push_back(at); });
            ASSERT_EQ(pulses.size(), series.pulsesCount_);
            ASSERT_EQ(replay.observedAt_, series.observedAt_);
            ASSERT_EQ(replay.lastPulseAt_, series.lastPulseAt_);
            ASSERT_EQ(replay.firstPulseAt_, series.firstPulseAt_);
            ASSERT_EQ(replay.lastInterval_, series.lastInterval_);
            ASSERT_TRUE(std::is_sorted(pulses.begin(), pulses.end()));
        }
    }
}

TEST(stepper_motor_timing, delayedConstantSpeedSeriesKeepsPwmRunning) {
    const stepper_motor_options_t opts{1000, 1, 100, 100};
    const StepperMotorSeries plan(4, 10, 10, true, opts);
    const auto ideal = evaluateSeries(plan, 0, opts);
    const auto clocked = evaluateSeries(plan, SECOND, opts);
    ASSERT_GT(clocked.pulsesCount_, ideal.pulsesCount_);
    ASSERT_GT(clocked.totalSec(), ideal.totalSec());
    ASSERT_NEAR(clocked.finalSpeed(opts), 10, 1e-5);
    ASSERT_NEAR(clocked.maxAccelerationDegSec2_, 0, 1e-5);
}

TEST(stepper_motor_timing, completesNinetyDegreesWithTwoHundredPulsesPerPhase) {
    for (const float direction : {1.0F, -1.0F}) {
        const auto sequence = getSeriesSequence(0, direction * 90, 0, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(sequence.error.empty());
        ASSERT_EQ(sequence.seq.size(), 2);
        ASSERT_EQ(sequence.seq[0].pulsesCount_, 200);
        ASSERT_EQ(sequence.seq[1].pulsesCount_, 200);
        ASSERT_NEAR(sequence.seq[0].totalRotationDeg(STEPPER_OPTS) +
            sequence.seq[1].totalRotationDeg(STEPPER_OPTS), direction * 90, 1e-5);
        const auto& deceleration = sequence.seq[1];
        ASSERT_TRUE(deceleration.finished_);
        ASSERT_EQ(deceleration.minimumPulsesCount_, 200);
        ASSERT_EQ(deceleration.lastInterval_, deceleration.activeInterval_);
    }
}

TEST(stepper_motor_timing, roundsHalfPulseUpInBothDirections) {
    const stepper_motor_options_t opts{1000, 1, 100, 100};
    for (const float direction : {1.0F, -1.0F}) {
        for (const duration timer : {duration(0), 5 * MILLISECOND}) {
            for (const float angle : {0.49F, 0.5F, 0.51F}) {
                const auto sequence = getSeriesSequence(0, direction * angle, 0, timer, opts);
                ASSERT_TRUE(sequence.error.empty());
                uint64_t count = 0;
                for (const auto& series : sequence.seq) { count += series.pulsesCount_; }
                ASSERT_EQ(count, angle < 0.5F ? 0 : 1);
            }
        }
    }
}

TEST(stepper_motor_timing, completesRemainingRoundedAngleAndReplaysCorrection) {
    for (const float direction : {1.0F, -1.0F}) {
        for (const float fraction : {0.49F, 0.5F, 0.51F}) {
            const float angle = direction * ((400.0F + fraction) * STEPPER_OPTS.degPulse);
            StepperMotorSeriesSequence sequence;
            ASSERT_TRUE(addAcceleratedSeries(sequence, 0, angle, 5 * MILLISECOND, STEPPER_OPTS));
            uint64_t count = 0;
            for (const auto& series : sequence.seq) {
                count += series.pulsesCount_;
                uint64_t emitted = 0;
                const auto replay = evaluateSeries(series, 5 * MILLISECOND, STEPPER_OPTS,
                    series.startedAt_, series.pulsesCount_, [&](moment) { ++emitted; });
                ASSERT_EQ(emitted, series.pulsesCount_);
                ASSERT_EQ(replay.lastPulseAt_, series.lastPulseAt_);
                ASSERT_EQ(replay.observedAt_, series.observedAt_);
            }
            const uint64_t roundedTarget = fraction < 0.5F ? 400 : 401;
            ASSERT_GE(count, roundedTarget);
            ASSERT_EQ(sequence.seq.back().minimumPulsesCount_, roundedTarget - sequence.seq.front().pulsesCount_);
        }
    }
}

#if defined(SYSTEM_IS_DESKTOP) && SYSTEM_IS_DESKTOP
class StepperMotorActionTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(Gpio::instance().initialize(), Gpio::SUCCESS); }
    void TearDown() override { ASSERT_EQ(Gpio::instance().terminate(), Gpio::SUCCESS); }
};

TEST_F(StepperMotorActionTest, separateMotorPinsAndCleanup) {
    auto& gpio = Gpio::instance();
    const stepper_motor_options_t options{10000, 0.1F, 100, 1000};
    StepperMotorSeriesSequence sequence;
    sequence.seq.push_back(evaluateSeries(StepperMotorSeries(1, 100, 100, true, options), 0, options));
    for (const unsigned pin : {4U, 5U, 6U}) {
        ASSERT_EQ(gpio.setMode(pin, GpioMode::output), Gpio::SUCCESS);
        ASSERT_EQ(gpio.write(pin, 1), Gpio::SUCCESS);
    }
    ASSERT_EQ(move(sequence, 1, 2, 3, 0, options, MICROSECOND), Gpio::SUCCESS);
    ASSERT_EQ(gpio.read(1), 0);
    ASSERT_EQ(gpio.read(2), 0);
    ASSERT_EQ(gpio.read(3), 1);
    for (const unsigned pin : {4U, 5U, 6U}) { ASSERT_EQ(gpio.read(pin), 1); }
    ASSERT_EQ(move(sequence, 4, 5, 6, 0, options, MICROSECOND), Gpio::SUCCESS);
    ASSERT_EQ(gpio.read(4), 0);
    ASSERT_EQ(gpio.read(5), 0);
    ASSERT_EQ(gpio.read(6), 1);
    ASSERT_EQ(gpio.read(3), 1);
}

TEST_F(StepperMotorActionTest, rejectsInvalidPinsAndReportsUninitializedBackend) {
    StepperMotorSeriesSequence sequence;
    sequence.seq.push_back(evaluateSeries(StepperMotorSeries(1, 10, 10, true, STEPPER_OPTS), 0, STEPPER_OPTS));
    ASSERT_EQ(move(sequence, 1, 1, 3, 0, STEPPER_OPTS, MICROSECOND), Gpio::INVALID_ARGUMENT);
    ASSERT_EQ(move(sequence, Gpio::PIN_COUNT, 2, 3, 0, STEPPER_OPTS, MICROSECOND), Gpio::INVALID_ARGUMENT);
    ASSERT_EQ(Gpio::instance().terminate(), Gpio::SUCCESS);
    ASSERT_EQ(move(sequence, 1, 2, 3, 0, STEPPER_OPTS, MICROSECOND), Gpio::NOT_INITIALIZED);
}
#endif
