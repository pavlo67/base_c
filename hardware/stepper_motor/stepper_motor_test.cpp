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

TEST(stepper_motor_timing, brakingControlUsesModeledTimerAndRemainingPulses) {
    printf("[MODEL] Check exact braking budget, timer sensitivity and both directions\n");
    for (const float direction : {1.0F, -1.0F}) {
        for (const duration timer : {2 * MILLISECOND, 9 * MILLISECOND}) {
            for (const float finalSpeed : {0.0F, 20.0F}) {
                auto reference = getFastestSeries(direction * 90, direction * finalSpeed, STEPPER_OPTS);
                reference.livePwm_ = true;
                reference = evaluateSeries(reference, timer, STEPPER_OPTS);
                const auto model = getBrakingModel(direction * 90, direction * finalSpeed, timer, STEPPER_OPTS);
                ASSERT_FALSE(model.brakingModel_.empty());
                ASSERT_EQ(model.expectedPulsesCount_, reference.pulsesCount_);
                ASSERT_EQ(model.idealFinalSpeed(STEPPER_OPTS), direction * finalSpeed);
                ASSERT_TRUE(canBrake(direction * 90, direction * finalSpeed, timer, reference.pulsesCount_, STEPPER_OPTS));
                ASSERT_FALSE(canBrake(direction * 90, direction * finalSpeed, timer, reference.pulsesCount_ - 1, STEPPER_OPTS));
                const auto replay = evaluateSeries(model, timer, STEPPER_OPTS);
                ASSERT_EQ(replay.pulsesCount_, reference.pulsesCount_);
                ASSERT_EQ(replay.lastInterval_, reference.lastInterval_);
                ASSERT_NEAR(replay.totalSec(), reference.totalSec(), 1e-6);
            }
        }
    }
    ASSERT_GT(getBrakingModel(90, 0, 9 * MILLISECOND, STEPPER_OPTS).expectedPulsesCount_,
        getBrakingModel(90, 0, 2 * MILLISECOND, STEPPER_OPTS).expectedPulsesCount_);
    ASSERT_FALSE(canBrake(90, 0, 0, 1000, STEPPER_OPTS));
    ASSERT_FALSE(canBrake(90, -20, MILLISECOND, 1000, STEPPER_OPTS));
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
    printf("[motor-test] Run motor\n");
    StepperMotorAction motor(sequence, 1, 2, 3, options, MICROSECOND);
    ASSERT_EQ(motor.run(0), Gpio::SUCCESS);
    ASSERT_EQ(gpio.read(1), 0);
    ASSERT_EQ(gpio.read(2), 0);
    ASSERT_EQ(gpio.read(3), 1);
    for (const unsigned pin : {4U, 5U, 6U}) { ASSERT_EQ(gpio.read(pin), 1); }
    printf("[motor-test] Run secondMotor\n");
    StepperMotorAction secondMotor(sequence, 4, 5, 6, options, MICROSECOND);
    ASSERT_EQ(secondMotor.run(0), Gpio::SUCCESS);
    ASSERT_EQ(gpio.read(4), 0);
    ASSERT_EQ(gpio.read(5), 0);
    ASSERT_EQ(gpio.read(6), 1);
    ASSERT_EQ(gpio.read(3), 1);
}

TEST_F(StepperMotorActionTest, rejectsInvalidPinsAndReportsUninitializedBackend) {
    StepperMotorSeriesSequence sequence;
    sequence.seq.push_back(evaluateSeries(StepperMotorSeries(1, 10, 10, true, STEPPER_OPTS), 0, STEPPER_OPTS));
    printf("[motor-test] Run duplicatePins\n");
    StepperMotorAction duplicatePins(sequence, 1, 1, 3, STEPPER_OPTS, MICROSECOND);
    ASSERT_EQ(duplicatePins.run(0), Gpio::INVALID_ARGUMENT);
    printf("[motor-test] Run invalidPin\n");
    StepperMotorAction invalidPin(sequence, Gpio::PIN_COUNT, 2, 3, STEPPER_OPTS, MICROSECOND);
    ASSERT_EQ(invalidPin.run(0), Gpio::INVALID_ARGUMENT);
    ASSERT_EQ(Gpio::instance().terminate(), Gpio::SUCCESS);
    printf("[motor-test] Run uninitialized\n");
    StepperMotorAction uninitialized(sequence, 1, 2, 3, STEPPER_OPTS, MICROSECOND);
    ASSERT_EQ(uninitialized.run(0), Gpio::NOT_INITIALIZED);
}
TEST_F(StepperMotorActionTest, externalTicksDrivePwmAndCompletionWithoutReplay) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(3, 10, 10, true, options);
    StepperMotorAction motor(plan, 1, 2, 3, options, MICROSECOND);
    const moment start = 20 * SECOND;
    ASSERT_EQ(motor.action(start - MILLISECOND), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.action(start), StepperMotorAction::RUNNING);
    PwmSettings pwm;
    ASSERT_EQ(Gpio::instance().getPwmSettings(1, pwm), Gpio::SUCCESS);
    ASSERT_TRUE(pwm.enabled);
    ASSERT_EQ(pwm.frequency, 10);
    ASSERT_EQ(pwm.duty * 2, pwm.range);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 0);
    ASSERT_EQ(motor.action(start + 100 * MILLISECOND), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 1);
    ASSERT_EQ(motor.action(start + 100 * MILLISECOND), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.action(start), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 1);
    ASSERT_EQ(motor.action(start + 200 * MILLISECOND), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.action(start + 300 * MILLISECOND), StepperMotorAction::COMPLETE);
    ASSERT_EQ(motor.action(start + SECOND), StepperMotorAction::COMPLETE);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 3);
    ASSERT_NEAR(motor.result().seq[0].totalSec(), 0.301, 1e-6);
    ASSERT_EQ(Gpio::instance().getPwmSettings(1, pwm), Gpio::SUCCESS);
    ASSERT_FALSE(pwm.enabled);
    ASSERT_EQ(Gpio::instance().read(3), 1);
}

TEST_F(StepperMotorActionTest, delayedUpdatesCountElapsedPwmPeriods) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(2, -10, -10, false, options);
    StepperMotorAction motor(plan, 1, 2, 3, options, MICROSECOND);
    ASSERT_EQ(motor.action(0), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.action(MILLISECOND), StepperMotorAction::RUNNING);
    ASSERT_EQ(Gpio::instance().read(2), 0);
    ASSERT_EQ(motor.action(551 * MILLISECOND), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 5);
    ASSERT_EQ(motor.action(651 * MILLISECOND), StepperMotorAction::COMPLETE);
    ASSERT_EQ(motor.result().seq[0].pulsesCount_, 6);
    ASSERT_NEAR(motor.result().seq[0].finalSpeed(options), -10, 1e-6);
}

TEST_F(StepperMotorActionTest, hardwarePwmModeAndDestructorCleanup) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(20, 10, 10, true, options);
    {
        StepperMotorAction motor(plan, 18, 2, 3, options, MICROSECOND, true);
        ASSERT_EQ(motor.action(0), StepperMotorAction::RUNNING);
        ASSERT_EQ(motor.action(MILLISECOND), StepperMotorAction::RUNNING);
        ASSERT_EQ(Gpio::instance().read(18), Gpio::WRONG_MODE);
        PwmSettings pwm;
        ASSERT_EQ(Gpio::instance().getPwmSettings(18, pwm), Gpio::SUCCESS);
        ASSERT_TRUE(pwm.enabled);
    }
    PwmSettings pwm;
    ASSERT_EQ(Gpio::instance().getPwmSettings(18, pwm), Gpio::SUCCESS);
    ASSERT_FALSE(pwm.enabled);
    ASSERT_EQ(Gpio::instance().read(3), 1);
    StepperMotorAction invalid(plan, 17, 2, 3, options, MICROSECOND, true);
    ASSERT_EQ(invalid.action(0), Gpio::NOT_SUPPORTED);
}

TEST_F(StepperMotorActionTest, watchdogStopsPwmBeforeNextLongTick) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(100, 10, 10, true, options);
    StepperMotorAction motor(plan, 1, 2, 3, options, MICROSECOND);
    ASSERT_EQ(motor.run(100 * MICROSECOND, 2 * MILLISECOND), StepperMotorAction::TIME_LIMIT);
    PwmSettings pwm;
    ASSERT_EQ(Gpio::instance().getPwmSettings(1, pwm), Gpio::SUCCESS);
    ASSERT_FALSE(pwm.enabled);
    ASSERT_EQ(Gpio::instance().read(3), 1);
    ASSERT_EQ(motor.action(SECOND), StepperMotorAction::TIME_LIMIT);
    StepperMotorAction longTick(plan, 1, 2, 3, options, MICROSECOND);
    ASSERT_EQ(longTick.run(SECOND, MILLISECOND), StepperMotorAction::TIME_LIMIT);
}

TEST_F(StepperMotorActionTest, invalidPeriodAndPulseWidthAreErrorsWithCleanup) {
    const stepper_motor_options_t options{1000, 1, 100, 100};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(1, 0.5F, 0.5F, true, options);
    StepperMotorAction slow(plan, 1, 2, 3, options, MICROSECOND);
    ASSERT_EQ(slow.action(0), StepperMotorAction::RUNNING);
    ASSERT_EQ(slow.action(MILLISECOND), Gpio::INVALID_ARGUMENT);
    ASSERT_EQ(Gpio::instance().read(3), 1);
    plan.seq.clear();
    plan.seq.emplace_back(1, 100, 100, true, options);
    StepperMotorAction wide(plan, 1, 2, 3, options, 10 * MILLISECOND);
    ASSERT_EQ(wide.action(0), StepperMotorAction::RUNNING);
    ASSERT_EQ(wide.action(20 * MILLISECOND), Gpio::INVALID_ARGUMENT);
    ASSERT_EQ(Gpio::instance().read(3), 1);
}

TEST_F(StepperMotorActionTest, runReturnsRuntimeStatisticsAndQuantizedFrequency) {
    const stepper_motor_options_t options{10000, 0.1F, 100, 1000};
    StepperMotorSeriesSequence plan;
    plan.seq.emplace_back(2, 10.35F, 10.35F, true, options);
    printf("[motor-test] Run and inspect runtime statistics\n");
    StepperMotorAction motor(plan, 1, 2, 3, options, MICROSECOND);
    ASSERT_EQ(motor.run(MILLISECOND, SECOND), Gpio::SUCCESS);
    const auto& real = motor.result();
    ASSERT_EQ(real.seq.size(), 1);
    ASSERT_TRUE(real.seq[0].finished_);
    ASSERT_GE(real.seq[0].pulsesCount_, 2);
    ASSERT_GT(real.seq[0].startedAt_, 0);
    ASSERT_GT(real.seq[0].totalSec(), 0);
    ASSERT_NEAR(real.seq[0].finalSpeed(options), 10.3, 1e-4);
    ASSERT_EQ(plan.seq[0].pulsesCount_, 0);
}

TEST_F(StepperMotorActionTest, emptySequenceDoesNotTouchPins) {
    auto& gpio = Gpio::instance();
    ASSERT_EQ(gpio.setMode(1, GpioMode::output), Gpio::SUCCESS);
    ASSERT_EQ(gpio.write(1, 1), Gpio::SUCCESS);
    StepperMotorAction empty({}, 1, 2, 3, STEPPER_OPTS, MICROSECOND);
    ASSERT_EQ(empty.action(0), StepperMotorAction::COMPLETE);
    ASSERT_EQ(gpio.read(1), 1);
}
TEST_F(StepperMotorActionTest, liveSequenceUsesRuntimeBrakingAndFiniteTerminalSpeed) {
    printf("[LIVE] Check predictive braking and finite terminal speed\n");
    const auto plan = getSeriesSequence(0, 90, 0, 5 * MILLISECOND, STEPPER_OPTS);
    ASSERT_TRUE(plan.error.empty());
    StepperMotorAction motor(plan, 1, 2, 3, STEPPER_OPTS, MICROSECOND);
    int status = StepperMotorAction::RUNNING;
    for (moment at = 0; at < 5 * SECOND && status == StepperMotorAction::RUNNING; at += 5 * MILLISECOND) {
        status = motor.action(at);
    }
    ASSERT_EQ(status, StepperMotorAction::COMPLETE);
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
TEST(StepperMotorRunTest, ownsLifecycleAndRunsSignedRequestedAngles) {
    printf("[RUN] Execute small signed angles through the shared helper\n");
    ASSERT_EQ(Gpio::instance().terminate(), Gpio::SUCCESS);
    const StepperMotorRunConfig cfg {
        .pinStep_ = 1, .pinDir_ = 2, .pinEna_ = 3,
        .options_ = STEPPER_OPTS, .expecterInterval_ = 5 * MILLISECOND,
        .pulseHigh_ = 15 * MICROSECOND, .timeLimit_ = 2 * SECOND
    };
    for (const float angle : {0.5F, -0.5F}) {
        StepperMotorSeriesSequence real;
        testing::internal::CaptureStdout();
        const int result = stepperMotorRun(cfg, angle, "test", &real);
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
        ASSERT_EQ(Gpio::instance().read(cfg.pinEna_), Gpio::NOT_INITIALIZED);
    }
}

TEST(StepperMotorRunTest, validatesBeforePlanningAndCleansUpTimeout) {
    printf("[RUN] Check zero, invalid options and watchdog cleanup\n");
    ASSERT_EQ(Gpio::instance().terminate(), Gpio::SUCCESS);
    StepperMotorRunConfig cfg {
        .pinStep_ = 1, .pinDir_ = 2, .pinEna_ = 3,
        .options_ = STEPPER_OPTS, .expecterInterval_ = 5 * MILLISECOND,
        .pulseHigh_ = 15 * MICROSECOND, .timeLimit_ = MILLISECOND
    };
    StepperMotorSeriesSequence real;
    ASSERT_EQ(stepperMotorRun(cfg, 0, "zero", &real), Gpio::SUCCESS);
    ASSERT_TRUE(real.seq.empty());
    ASSERT_EQ(Gpio::instance().read(cfg.pinEna_), Gpio::NOT_INITIALIZED);
    cfg.options_.degPulse = 0;
    ASSERT_EQ(stepperMotorRun(cfg, 0.5F, "invalid", &real), Gpio::INVALID_ARGUMENT);
    ASSERT_FALSE(real.error.empty());
    ASSERT_EQ(Gpio::instance().read(cfg.pinEna_), Gpio::NOT_INITIALIZED);
    cfg.options_ = STEPPER_OPTS;
    ASSERT_EQ(stepperMotorRun(cfg, 0.5F, "timeout", &real), StepperMotorAction::TIME_LIMIT);
    ASSERT_FALSE(real.error.empty());
    ASSERT_FALSE(real.seq.empty());
    ASSERT_EQ(Gpio::instance().read(cfg.pinEna_), Gpio::NOT_INITIALIZED);
}
TEST_F(StepperMotorActionTest, brakingRetainsTargetAfterSimulatedHalfwayOvershoot) {
    printf("[STEP] Preserve all 147 pulses for signed 33-degree moves\n");
    for (const float angle : {33.0F, -33.0F}) {
        const auto plan = getSeriesSequence(0, angle, 0, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(plan.error.empty());
        ASSERT_EQ(plan.seq.front().stopAfterPulses_, 73U);
        ASSERT_EQ(plan.seq.front().pulsesCount_, 74U);
        StepperMotorAction motor(plan, 1, 2, 3, STEPPER_OPTS, 15 * MICROSECOND);
        int status = StepperMotorAction::RUNNING;
        for (moment at = 0; at < 5 * SECOND && status == StepperMotorAction::RUNNING; at += 5 * MILLISECOND) {
            status = motor.action(at);
        }
        ASSERT_EQ(status, StepperMotorAction::COMPLETE);
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

TEST_F(StepperMotorActionTest, predictiveBrakingReplacesPlannedCruiseWithUnevenTicks) {
    printf("[LIVE] Predict braking for both directions with uneven external timer ticks\n");
    for (const float angle : {33.0F, -33.0F, 180.0F, -180.0F}) {
        const auto plan = getSeriesSequence(0, angle, 0, 5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(plan.error.empty());
        StepperMotorAction motor(plan, 1, 2, 3, STEPPER_OPTS, 15 * MICROSECOND);
        int status = StepperMotorAction::RUNNING;
        unsigned tick = 0;
        for (moment at = 0; at < 5 * SECOND && status == StepperMotorAction::RUNNING;
                at += (++tick % 2 ? 4 : 6) * MILLISECOND) {
            status = motor.action(at);
        }
        ASSERT_EQ(status, StepperMotorAction::COMPLETE);
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

TEST_F(StepperMotorActionTest, brakingFreezesMeanSinceAccelerationAndHandlesLaterDelays) {
    printf("[LIVE] Exclude startup, ignore duplicate ticks, freeze mean and delay braking updates\n");
    for (const duration planTimer : {duration(0), 5 * MILLISECOND}) {
        for (const float angle : {33.0F, -33.0F, 720.0F, -720.0F}) {
            const auto plan = getSeriesSequence(0, angle, 0, planTimer, STEPPER_OPTS);
            ASSERT_TRUE(plan.error.empty());
            StepperMotorAction motor(plan, 1, 2, 3, STEPPER_OPTS, 15 * MICROSECOND);
            ASSERT_EQ(motor.action(0), StepperMotorAction::RUNNING);
            // A large startup delay must not become the model timer.
            moment at = 100 * MILLISECOND;
            ASSERT_EQ(motor.action(at), StepperMotorAction::RUNNING);
            const moment started = motor.result().seq.front().startedAt_;
            uint64_t samples = 0;
            while (!motor.result().seq.front().finished_ && at < 10 * SECOND) {
                at += (++samples % 2 ? 1 : 9) * MILLISECOND;
                ASSERT_EQ(motor.action(at), StepperMotorAction::RUNNING);
                ASSERT_EQ(motor.action(at), StepperMotorAction::RUNNING);
                ASSERT_EQ(motor.action(at - 1), StepperMotorAction::RUNNING);
            }
            ASSERT_TRUE(motor.result().seq.front().finished_);
            ASSERT_EQ(motor.result().seq.size(), 3U);
            const duration mean = (at - started) / samples;
            ASSERT_EQ(motor.result().seq[1].modelInterval_, mean);
            ASSERT_LT(mean, 6 * MILLISECOND);
            const auto frozen = motor.result().seq[1].brakingModel_;
            ASSERT_FALSE(frozen.empty());
            int status = StepperMotorAction::RUNNING;
            unsigned brakingTicks = 0;
            while (status == StepperMotorAction::RUNNING && at < 20 * SECOND) {
                at += (++brakingTicks % 3 ? 5 : 20) * MILLISECOND;
                status = motor.action(at);
            }
            ASSERT_EQ(status, StepperMotorAction::COMPLETE);
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

TEST_F(StepperMotorActionTest, predictiveBrakingPreservesNonzeroFinalSpeed) {
    printf("[LIVE] Finish both directions at a finite nonzero base speed\n");
    for (const float direction : {1.0F, -1.0F}) {
        const auto plan = getSeriesSequence(direction * 20, direction * 180, direction * 20,
            5 * MILLISECOND, STEPPER_OPTS);
        ASSERT_TRUE(plan.error.empty());
        StepperMotorAction motor(plan, 1, 2, 3, STEPPER_OPTS, 15 * MICROSECOND);
        int status = StepperMotorAction::RUNNING;
        for (moment at = 0; at < 5 * SECOND && status == StepperMotorAction::RUNNING; at += 5 * MILLISECOND) {
            status = motor.action(at);
        }
        ASSERT_EQ(status, StepperMotorAction::COMPLETE);
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

TEST_F(StepperMotorActionTest, exhaustedBrakingBudgetSkipsTailAfterLargeDelay) {
    printf("[LIVE] Retain overshoot observations and skip an exhausted braking budget\n");
    const auto plan = getSeriesSequence(0, 33, 0, 5 * MILLISECOND, STEPPER_OPTS);
    ASSERT_TRUE(plan.error.empty());
    StepperMotorAction motor(plan, 1, 2, 3, STEPPER_OPTS, 15 * MICROSECOND);
    ASSERT_EQ(motor.action(0), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.action(5 * MILLISECOND), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.action(5 * SECOND), StepperMotorAction::RUNNING);
    ASSERT_EQ(motor.result().seq.size(), 2U);
    ASSERT_GT(motor.result().seq.front().pulsesCount_, 146U);
    ASSERT_EQ(motor.action(5 * SECOND + 100 * MILLISECOND), StepperMotorAction::COMPLETE);
    ASSERT_TRUE(motor.result().error.empty());
    // The 100-ms observation gap spans two natural terminal PWM periods.
    ASSERT_EQ(motor.result().seq.back().pulsesCount_, 2U);
}
#endif

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
