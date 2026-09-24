#include "stepper_motor.h"
#include "hardware/stepper_motor/stepper_motor_test_config.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cassert>
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

const float INITIAL_SPEED     =  0.0F;
const float FINAL_SPEED       = 80.0F;
const bool  DIRECTION_FORWARD = FINAL_SPEED - INITIAL_SPEED > 0;
const float LIMIT1            = -2.25F;
const float LIMIT2            =  2.25F;
const float TARGET_CHANGE_DEG = 91.0F;


constexpr const char* HARDWARE_CONFIG_PATH = HARDWARE_DEFAULT_CONFIG_PATH; // Or a path to machina.yaml.
StepperMotorRunConfig testPanHardware;

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    printf("[MOT] Load pan hardware from %s\n", HARDWARE_CONFIG_PATH);
    std::array<StepperMotorRunConfig, 2> motors;
    if (!loadPlatformMotorConfig(Config(HARDWARE_CONFIG_PATH), motors)) { return 1; }
    testPanHardware = motors[0];
    return RUN_ALL_TESTS();
}


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

    if (series.terminalInterval_) {
        const auto terminal = evaluateSeries(series, 0, stepperOpts);
        ASSERT_EQ(terminal.pulsesCount_, 1);
        ASSERT_GE(terminal.lastInterval_, series.terminalInterval_);
        ASSERT_LE(terminal.maxAccelerationDegSec2_, stepperOpts.accelMaxDegSec2);
        return;
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
    printf("[SIM] Check halfway acceleration and optional cruise before braking\n");
    for (const float angle : {90.0F, -180.0F}) {
        const auto sequence = getSeriesSequence(0, angle, 0, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(sequence.error.empty()) << sequence.error;
        ASSERT_GE(sequence.seq.size(), 3);
        const auto& acceleration = sequence.seq[0];
        const auto& deceleration = sequence.seq[sequence.seq.size() - 2];
        const uint64_t halfPulses = (STEPPER_OPTS.pulsesForDeg(angle, angle > 0) + 1) / 2;
        ASSERT_GE(acceleration.pulsesCount_, halfPulses);
        auto before = evaluateSeries(acceleration, 5 * MILLISECOND, STEPPER_OPTS, 0, halfPulses);
        ASSERT_EQ(before.observedAt_, acceleration.observedAt_);
        ASSERT_EQ(sequence.seq[1].startedAt_, acceleration.observedAt_);
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

TEST(stepper_motor_timing, cruiseUsesTimerBrakingBudgetAndPreservesIdealTriangle) {
    printf("[SIM] Compare cruise against slow completion of the same remaining angle\n");
    for (const float direction : {1.0F, -1.0F}) {
        const auto ideal = getSeriesSequence(0, direction * 33, 0, 0, STEPPER_OPTS);
        ASSERT_EQ(ideal.seq.size(), 3U);
        ASSERT_GT(ideal.seq[0].accelerationDegPerSec2_, 0);
        ASSERT_LT(ideal.seq[1].accelerationDegPerSec2_, 0);
        const auto clocked = getSeriesSequence(0, direction * 180, 0, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_EQ(clocked.seq.size(), 4U);
        const auto& acceleration = clocked.seq[0];
        const auto& cruise = clocked.seq[1];
        const auto& braking = clocked.seq[2];
        ASSERT_EQ(cruise.accelerationDegPerSec2_, 0);
        ASSERT_GT(cruise.pulsesCount_, 0U);
        ASSERT_NEAR(cruise.finalSpeed(STEPPER_OPTS), acceleration.finalSpeed(STEPPER_OPTS), SPEED_EPS);
        ASSERT_NEAR(braking.initialSpeedDegPerSec_, cruise.finalSpeed(STEPPER_OPTS), SPEED_EPS);
        const uint64_t remaining = 799 - acceleration.pulsesCount_;
        auto oldBraking = getFastestSeries(acceleration.finalSpeed(STEPPER_OPTS), 0, STEPPER_OPTS);
        oldBraking.minimumPulsesCount_ = remaining;
        oldBraking = evaluateSeries(oldBraking, 5 * MILLISECOND, STEPPER_OPTS, acceleration.observedAt_);
        ASSERT_EQ(cruise.pulsesCount_ + braking.pulsesCount_, remaining);
        ASSERT_LT(cruise.totalSec() + braking.totalSec(), oldBraking.totalSec());
        const auto noCruise = getCruiseAndBraking(direction * 110, 0, 1,
            5 * MILLISECOND, STEPPER_OPTS, CONSTANT_ACCELERATION);
        ASSERT_EQ(noCruise.size(), 1U);
        ASSERT_LT(noCruise.front().accelerationDegPerSec2_, 0);
    }
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
        ASSERT_EQ(sequence.seq.size(), 3);
        ASSERT_EQ(sequence.seq[0].pulsesCount_, 200);
        ASSERT_EQ(sequence.seq[1].pulsesCount_ + sequence.seq[2].pulsesCount_, 200);
        ASSERT_NEAR(sequence.seq[0].totalRotationDeg(STEPPER_OPTS) +
            sequence.seq[1].totalRotationDeg(STEPPER_OPTS) + sequence.seq[2].totalRotationDeg(STEPPER_OPTS), direction * 90, 1e-5);
        const auto& deceleration = sequence.seq[1];
        ASSERT_TRUE(deceleration.finished_);
        ASSERT_EQ(deceleration.minimumPulsesCount_, 199);
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
            ASSERT_EQ(sequence.seq[1].minimumPulsesCount_, roundedTarget - sequence.seq.front().pulsesCount_ - 1);
            ASSERT_EQ(sequence.seq.back().pulsesCount_, 1);
        }
    }
}

TEST(stepper_motor_timing, terminalIntervalUsesKinematicsWithoutAddingDisplacement) {
    printf("[TERM] Check natural terminal timing in both directions\n");
    for (const float direction : {1.0F, -1.0F}) {
        for (const duration timer : {duration(0), 5 * MILLISECOND}) {
            const auto sequence = getSeriesSequence(0, direction * 90, 0, timer, STEPPER_OPTS);
            ASSERT_TRUE(sequence.error.empty());
            ASSERT_GE(sequence.seq.size(), 3);
            const auto& terminal = sequence.seq.back();
            const double naturalPeriod = 2 * std::sqrt(STEPPER_OPTS.degPulse / STEPPER_OPTS.accelMaxDegSec2);
            ASSERT_EQ(terminal.pulsesCount_, 1);
            ASSERT_NEAR(terminal.finalSpeed(STEPPER_OPTS), direction * STEPPER_OPTS.degPulse / naturalPeriod, 1e-5);
            ASSERT_GE(terminal.totalSec() + 1e-7, naturalPeriod);
            ASSERT_LE(terminal.totalSec(), naturalPeriod + static_cast<double>(timer) / SECOND + 1e-7);
            uint64_t pulses = 0;
            for (const auto& series : sequence.seq) {
                pulses += series.pulsesCount_;
                ASSERT_LE(series.maxAccelerationDegSec2_, STEPPER_OPTS.accelMaxDegSec2 * 1.01F);
            }
            ASSERT_EQ(pulses, 400);
        }
    }
}

TEST(stepper_motor_timing, slowSinglePulseIsNotShortenedAndNonzeroBaseIsPreserved) {
    const stepper_motor_options_t slow{100, 1, 1, 1};
    const auto single = getSeriesSequence(0, 1, 0, 0, slow);
    ASSERT_TRUE(single.error.empty());
    ASSERT_EQ(single.seq.size(), 1);
    ASSERT_NEAR(single.seq[0].totalSec(), 2, 1e-6);
    ASSERT_NEAR(single.seq[0].finalSpeed(slow), 0.5, 1e-6);
    const auto moving = getSeriesSequence(10, 90, 10, 0, STEPPER_OPTS);
    ASSERT_TRUE(moving.error.empty());
    for (const auto& series : moving.seq) { ASSERT_EQ(series.terminalInterval_, 0); }
}

TEST(stepper_motor_timing, quietLoggingGroupsBrakingAndKeepsAllTotals) {
    const auto sequence = getSeriesSequence(0, 90, 0, 0, STEPPER_OPTS);
    ::testing::internal::CaptureStdout();
    sequence.log(STEPPER_OPTS, "ideal", false);
    const std::string quiet = ::testing::internal::GetCapturedStdout();
    ASSERT_EQ(quiet.find("expectedPulsesCount    :"), std::string::npos);
    ASSERT_NE(quiet.find("ideal: Acceleration block total:"), std::string::npos);
    ASSERT_NE(quiet.find("ideal: Cruise block total:"), std::string::npos);
    const size_t braking = quiet.find("ideal: Deceleration block total:");
    ASSERT_NE(braking, std::string::npos);
    ASSERT_EQ(quiet.find("ideal: Deceleration block total:", braking + 1), std::string::npos);
    ASSERT_NE(quiet.substr(braking).find("pulses=101,"), std::string::npos);
    ASSERT_NE(quiet.find("ideal TOTAL:"), std::string::npos);
    ASSERT_EQ(quiet.find("expected="), std::string::npos);
    ::testing::internal::CaptureStdout();
    sequence.log(STEPPER_OPTS, "ideal", true);
    const std::string verbose = ::testing::internal::GetCapturedStdout();
    ASSERT_NE(verbose.find("expectedPulsesCount    :"), std::string::npos);
    ASSERT_EQ(verbose.find("expected="), std::string::npos);
    ASSERT_NE(verbose.find("ideal: Deceleration block total:"), std::string::npos);
}

TEST(stepper_motor_timing, frozenBrakingSkipsCommandsByElapsedPulses) {
    printf("[MODEL] Delay updates and catch up by pulses, allowing stronger braking\n");
    const stepper_motor_options_t options{10000, 0.1F, 100, 1000};
    StepperMotorSeries braking(7, 100, 0, true, options);
    braking.livePwm_ = true;
    braking.brakingModel_ = {{0, MILLISECOND}, {3, 2 * MILLISECOND}, {5, 4 * MILLISECOND}};
    ASSERT_NEAR(braking.intervalSec(0, options), 0.001, 1e-9);
    ASSERT_NEAR(braking.intervalSec(4 * MILLISECOND, options), 0.002, 1e-9);
    ASSERT_EQ(braking.pulsesCount_, 4U);
    ASSERT_NEAR(braking.intervalSec(8 * MILLISECOND, options), 0.004, 1e-9);
    ASSERT_EQ(braking.pulsesCount_, 6U);
    ASSERT_GT(braking.maxAccelerationDegSec2_, options.accelMaxDegSec2);
    ASSERT_NEAR(braking.intervalSec(8 * MILLISECOND, options), 0.004, 1e-9);
    ASSERT_NEAR(braking.intervalSec(7 * MILLISECOND, options), 0.004, 1e-9);
    ASSERT_EQ(braking.pulsesCount_, 6U);
    ASSERT_EQ(braking.intervalSec(12 * MILLISECOND, options), 0);
    ASSERT_EQ(braking.pulsesCount_, 7U);
    braking.reset();
    ASSERT_EQ(braking.brakingModel_.size(), 3U);
    ASSERT_NEAR(braking.intervalSec(0, options), 0.001, 1e-9);
}


TEST(stepper_motor_timing, shortPlatformMovesHaveNoFixedTerminalDelay) {
    printf("[TERM] Check one/two pulses and quarter/one/two-degree moves\n");
    const stepper_motor_options_t options{8000, 0.125F, 180, 11444.94F};
    for (const float angle : {0.125F, 0.25F, 1.F, 2.F, -0.125F, -0.25F, -1.F, -2.F}) {
        const auto sequence = getSeriesSequence(0, angle, 0, 0, options);
        ASSERT_TRUE(sequence.error.empty());
        double elapsed = 0;
        double rotation = 0;
        for (const auto& section : sequence.seq) {
            elapsed += section.totalSec();
            rotation += section.totalRotationDeg(options);
        }
        ASSERT_NEAR(rotation, angle, 1e-6);
        ASSERT_GT(elapsed, 0);
        ASSERT_LT(elapsed, 0.05);
        ASSERT_NEAR(sequence.seq.back().totalSec(), 2 * std::sqrt(options.degPulse / options.accelMaxDegSec2), 1e-7);
    }
}
