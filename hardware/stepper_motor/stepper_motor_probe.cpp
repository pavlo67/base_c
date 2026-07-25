#include "stepper_motor.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

const stepper_options_t STEPPER_OPTS {
    .freqMax = 20'000.0F,
    .freqMaxAllowed = 5'000.0F,
    .accelMax = 720.0F,
    .degreesPerPulse = 0.045F
};

const float INITIAL_SPEED = 0.0F;
const float FINAL_SPEED = 90.0F;

float testSeries(const PulseSeries& series, float finalSpeedDegPerSec, bool directionForward, stepper_options_t stepperOpts,  interval_algorithm_t intervalAlgorithm) {
    assert(series.pulseCount_ > 0);
    assert(series.directionForward_ == directionForward);
    assert(series.intervalAlgorithm_ == intervalAlgorithm);
    assert(std::abs(series.finalSpeed(stepperOpts) - finalSpeedDegPerSec) < SPEED_DEG_PER_SEC_EPS);

    if (series.pulseCount_ > 1)
    if (finalSpeedDegPerSec > series.initialSpeedDegPerSec_) {
        assert(series.intervalSec(0, STEPPER_OPTS) > series.intervalSec(series.pulseCount_ - 1, STEPPER_OPTS));
    } else {
        assert(series.intervalSec(0, STEPPER_OPTS) < series.intervalSec(series.pulseCount_ - 1, STEPPER_OPTS));
    }

    float accelErrorMax = 0.0F;
    float speed = series.initialSpeedDegPerSec_;

    for (std::uint64_t pulseIndex = 0; pulseIndex < series.pulseCount_; ++pulseIndex) {
        const float interval  = series.intervalSec(pulseIndex, STEPPER_OPTS);
        const float nextSpeed = 2.0F * STEPPER_OPTS.degreesPerPulse / interval - speed;
        const float accel = (nextSpeed * nextSpeed - speed * speed) / (2.0F * STEPPER_OPTS.degreesPerPulse);

        // printf("accelError: %f\n", accel - series.accelerationDegPerSec2_);

        accelErrorMax = std::max(accelErrorMax,std::abs(accel - series.accelerationDegPerSec2_));
        speed = nextSpeed;
    }

    return accelErrorMax;
    // assert(std::abs(series.totalRotationDeg(stepperOpts) - totalRotationDeg) < stepperOpts.degreesPerPulse);
}

int main() {
    const PulseSeries exact = getFastestSeries(INITIAL_SPEED, FINAL_SPEED, STEPPER_OPTS,CONSTANT_ACCELERATION);
    const float exactAccelErrorMax = testSeries(exact, FINAL_SPEED, FINAL_SPEED - INITIAL_SPEED > 0, STEPPER_OPTS, CONSTANT_ACCELERATION);

    printf("exactAccelErrorMax: %f\n", exactAccelErrorMax);

    const PulseSeries fallback = getFastestSeries(INITIAL_SPEED, FINAL_SPEED, STEPPER_OPTS, LINEAR_INTERVAL_ACCELERATION);
    const float fallbackAccelErrorMax = testSeries(fallback, FINAL_SPEED, FINAL_SPEED - INITIAL_SPEED > 0, STEPPER_OPTS, LINEAR_INTERVAL_ACCELERATION);

    printf("fallbackAccelErrorMax: %f\n", fallbackAccelErrorMax);

    assert(fallback.pulseCount_ == exact.pulseCount_);
    assert(fallbackAccelErrorMax >= exactAccelErrorMax);

    PulseSeries limited = exact;
    limited.limitWithDeg(-2.25F, STEPPER_OPTS);
    assert(limited.pulseCount_ == 50);
    assert(!limited.directionForward_);
    assert(std::abs(limited.totalRotationDeg(STEPPER_OPTS) + 2.25F) < 1e-5F);

    // const float targetChangeDeg = 90.0F;
    // const stepper_action_t calculated = calculateAction(
    //         0.0F,
    //         targetChangeDeg,
    //         0.0F,
    //         STEPPER_OPTS);
    // assert(calculated.error.empty());
    // assert(!calculated.seriesSequence.empty());
    //
    // float calculatedChangeDeg = 0.0F;
    // for (const PulseSeries& item : calculated.seriesSequence) {
    //     assert(item.intervalAlgorithm_ == CONSTANT_ACCELERATION);
    //     calculatedChangeDeg += item.totalRotationDeg(STEPPER_OPTS);
    // }
    //
    // printf("calculatedChangeDeg: %f, targetChangeDeg: %f, delta: %f, STEPPER_OPTS.degreesPerPulse: %f\n", calculatedChangeDeg, targetChangeDeg, calculatedChangeDeg - targetChangeDeg, STEPPER_OPTS.degreesPerPulse);
    //
    // // assert(std::abs(calculatedChangeDeg - targetChangeDeg) <= STEPPER_OPTS.degreesPerPulse / 2.0F + 1e-5F);
    // //
    // // const stepper_action_t fallbackCalculated = calculateAction(
    // //         0.0F,
    // //         targetChangeDeg,
    // //         0.0F,
    // //         STEPPER_OPTS,
    // //         LINEAR_INTERVAL_ACCELERATION);
    // // assert(fallbackCalculated.error.empty());
    // // assert(!fallbackCalculated.seriesSequence.empty());
    // // for (const PulseSeries& item : fallbackCalculated.seriesSequence) {
    // //     assert(item.intervalAlgorithm_ == LINEAR_INTERVAL_ACCELERATION);
    // // }
    // //
    // // std::cout << "exact max acceleration error: " << exactMaxAccelError << '\n';
    // // std::cout << "fallback max acceleration error: " << fallbackMaxAccelError << '\n';
    // // std::cout << "stepper_motor_probe: OK\n";
    return EXIT_SUCCESS;
}
