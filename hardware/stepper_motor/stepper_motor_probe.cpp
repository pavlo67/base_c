#include "stepper_motor.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

#include "lib/mathlib.h"

const stepper_options_t STEPPER_OPTS {
    .freqMax = 20'000.0F,
    .freqMaxAllowed = 5'000.0F,
    .accelMax = 720.0F,
    .degreesPerPulse = 0.045F
};

const float INITIAL_SPEED     =  0.0F;
const float FINAL_SPEED       = 80.0F;
const bool  DIRECTION_FORWARD = FINAL_SPEED - INITIAL_SPEED > 0;
const float LIMIT1            = -2.25F;
const float LIMIT2            =  2.25F;
const float TARGET_CHANGE_DEG = 91.0F;


void testSeries(const StepperSeries& series, float totalRotationDeg, float finalSpeedDegPerSec, bool directionForward, stepper_options_t stepperOpts, interval_algorithm_t intervalAlgorithm, const std::string& verboseLabel) {
    if (!verboseLabel.empty()) {
        printf("\n%s: pulseCount             : %5lu\n",  verboseLabel.c_str(), series.pulseCount_);
        printf("%s: initialSpeedDegPerSec  : %9.3f\n", verboseLabel.c_str(), series.initialSpeedDegPerSec_);
        printf("%s: totalRotationDeg       : %9.3f\n", verboseLabel.c_str(), series.totalRotationDeg(STEPPER_OPTS));
        printf("%s: finalSpeed             : %9.3f\n", verboseLabel.c_str(), series.finalSpeed(STEPPER_OPTS));
        printf("%s: directionForward       : %5d\n",   verboseLabel.c_str(), series.directionForward_);
        printf("%s: intervalAlgorithm      : %5d\n",   verboseLabel.c_str(), series.intervalAlgorithm_);
        printf("%s: accelerationDegPerSec2 : %9.3f\n", verboseLabel.c_str(), series.accelerationDegPerSec2_);
    }

    ASSERT_EQ(series.directionForward_, directionForward);
    ASSERT_EQ(series.intervalAlgorithm_, intervalAlgorithm);

    if (!isnanf(finalSpeedDegPerSec)) {
        ASSERT_NEAR(series.finalSpeed(stepperOpts), finalSpeedDegPerSec, SPEED_DEG_PER_SEC_EPS);
    }
    if (!isnanf(totalRotationDeg)) {
        ASSERT_NEAR(series.totalRotationDeg(stepperOpts), totalRotationDeg, stepperOpts.degreesPerPulse);
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

    float speed = series.initialSpeedDegPerSec_;
    float accelErrorMax = 0.0F;
    for (std::uint64_t pulseIndex = 0; pulseIndex < series.pulseCount_; ++pulseIndex) {
        const float interval  = series.intervalSec(pulseIndex, STEPPER_OPTS);
        const float nextSpeed = 2.0F * STEPPER_OPTS.degreesPerPulse / interval - speed;
        const float accel = (nextSpeed * nextSpeed - speed * speed) / (2.0F * STEPPER_OPTS.degreesPerPulse);

        // printf("accel: %f\n", accel);

        accelErrorMax = std::max(accelErrorMax,std::abs(accel - series.accelerationDegPerSec2_));
        speed = nextSpeed;
    }

    if (!verboseLabel.empty()) {
        printf("%s: accelErrorMax          : %9.3f\n", verboseLabel.c_str(), accelErrorMax);
    }

    // return accelErrorMax;
    // assert(std::abs(series.totalRotationDeg(stepperOpts) - totalRotationDeg) < stepperOpts.degreesPerPulse);
}

void testResult(float calculatedTotalRotationDeg) {
    ASSERT_NEAR(calculatedTotalRotationDeg, TARGET_CHANGE_DEG, STEPPER_OPTS.degreesPerPulse);
    printf("\nRESULT!!! calculatedTotalRotationDeg: %f, TARGET_CHANGE_DEG: %f, STEPPER_OPTS.degreesPerPulse: %f\n", calculatedTotalRotationDeg, TARGET_CHANGE_DEG, STEPPER_OPTS.degreesPerPulse);
}

int main() {
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
        testSeries(series, NAN, NAN, DIRECTION_FORWARD, STEPPER_OPTS, CONSTANT_ACCELERATION, "calculated" + std::to_string(i++));
    }

    testResult(calculatedTotalRotationDeg);
}
