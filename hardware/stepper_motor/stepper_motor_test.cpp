#include "stepper_motor.h"
#include "hardware/hardware.h"

#include "test/test.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "lib/mathlib.h"

const stepper_options_t STEPPER_OPTS {
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


void testSeries(const StepperSeries& series, float totalRotationDegExpected, float finalSpeedDegPerSecExpected, bool directionForwardExpected, stepper_options_t stepperOpts, interval_algorithm_t intervalAlgorithm, const std::string& verboseLabel) {
    if (!verboseLabel.empty()) {
        series.log(stepperOpts, verboseLabel.c_str());
    }

    ASSERT_EQ(series.directionForward_, directionForwardExpected);
    ASSERT_EQ(series.intervalAlgorithm_, intervalAlgorithm);

    if (!isnanf(finalSpeedDegPerSecExpected)) {
        ASSERT_NEAR(series.finalSpeed(stepperOpts), finalSpeedDegPerSecExpected, std::max(SPEED_EPS, std::abs(finalSpeedDegPerSecExpected) * RESULT_EPS_RATIO));
    }
    if (!isnanf(totalRotationDegExpected)) {
        ASSERT_NEAR(series.totalRotationDeg(stepperOpts), totalRotationDegExpected, std::max(stepperOpts.degPulse, totalRotationDegExpected * RESULT_EPS_RATIO));
    }

    if (series.pulseCount_ > 1) {
        if (series.finalSpeed(stepperOpts) > series.initialSpeedDegPerSec_) {
            ASSERT_LT(series.intervalSec(series.pulseCount_ - 1, STEPPER_OPTS), series.intervalSec(0, STEPPER_OPTS));
        } else if (series.finalSpeed(stepperOpts) < series.initialSpeedDegPerSec_) {
            ASSERT_GT(series.intervalSec(series.pulseCount_ - 1, STEPPER_OPTS), series.intervalSec(0, STEPPER_OPTS));
        } else {
            ASSERT_NEAR(series.intervalSec(series.pulseCount_ - 1, STEPPER_OPTS), series.intervalSec(0, STEPPER_OPTS), EPS);
        }
    }

    float speed         = series.initialSpeedDegPerSec_;
    float accelErrorMax = 0;
    float intervalPrev  = series.initialSpeedDegPerSec_ < EPS ? 0 :  STEPPER_OPTS.degPulse / series.initialSpeedDegPerSec_;

    for (uint64_t pulseIndex = 0; pulseIndex < series.pulseCount_; ++pulseIndex) {
        const float intervalSec = series.intervalSec(pulseIndex, STEPPER_OPTS);
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
    // assert(std::abs(series.totalRotationDeg(stepperOpts) - totalRotationDeg) < stepperOpts.degreesPerPulse);
}

void testResult(float calculatedTotalRotationDeg) {
    ASSERT_NEAR(calculatedTotalRotationDeg, TARGET_CHANGE_DEG, std::max(STEPPER_OPTS.degPulse, TARGET_CHANGE_DEG * RESULT_EPS_RATIO));
    printf("\nRESULT!!! calculatedTotalRotationDeg: %f, TARGET_CHANGE_DEG: %f, STEPPER_OPTS.degreesPerPulse: %f\n\n", calculatedTotalRotationDeg, TARGET_CHANGE_DEG, STEPPER_OPTS.degPulse);
}

TEST(stepper_motor_test, stepper_motor_test) {
    const StepperSeries seriesExact = getFastestSeries(INITIAL_SPEED, FINAL_SPEED, STEPPER_OPTS,CONSTANT_ACCELERATION);
    testSeries(seriesExact, NAN, FINAL_SPEED, DIRECTION_FORWARD, STEPPER_OPTS, CONSTANT_ACCELERATION, "exact");

    // const PulseSeries fallback = getFastestSeries(INITIAL_SPEED, FINAL_SPEED, STEPPER_OPTS, LINEAR_INTERVAL_ACCELERATION);
    // const float fallbackAccelErrorMax = testSeries(fallback, FINAL_SPEED, DIRECTION_FORWARD, STEPPER_OPTS, LINEAR_INTERVAL_ACCELERATION);
    //
    // printf("fallbackAccelErrorMax: %f\n", fallbackAccelErrorMax);

    // assert(fallback.pulseCount_ == exact.pulseCount_);
    // assert(fallbackAccelErrorMax >= exactAccelErrorMax);

    StepperSeries seriesLimit1 = seriesExact;
    seriesLimit1.limitWithDeg(LIMIT1, STEPPER_OPTS);
    testSeries(seriesLimit1, 0, NAN, DIRECTION_FORWARD, STEPPER_OPTS, seriesExact.intervalAlgorithm_, "limit1");

    StepperSeries seriesLimit2 = seriesExact;
    seriesLimit2.limitWithDeg(LIMIT2, STEPPER_OPTS);
    testSeries(seriesLimit2, LIMIT2, NAN, DIRECTION_FORWARD, STEPPER_OPTS, seriesExact.intervalAlgorithm_, "limit2");

    const stepper_sequence_t sequenceCalculated = calculateSequence(0.0F, TARGET_CHANGE_DEG,0.0F, STEPPER_OPTS);
    assert(sequenceCalculated.error.empty());
    assert(!sequenceCalculated.seriesSequence.empty());

    float calculatedTotalRotationDeg = 0.0F;
    int i = 0;
    for (const StepperSeries& series : sequenceCalculated.seriesSequence) {
        calculatedTotalRotationDeg += series.totalRotationDeg(STEPPER_OPTS);
        testSeries(series, NAN, NAN, TARGET_CHANGE_DEG >= 0, STEPPER_OPTS, CONSTANT_ACCELERATION, "calculated" + std::to_string(i++));
    }

    testResult(calculatedTotalRotationDeg);
}
