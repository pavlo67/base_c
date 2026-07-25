#include "stepper_motor.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

int main() {
    const stepper_options_t stepperOpts {
            .freqMax = 20'000.0F,
            .freqMaxAllowed = 5'000.0F,
            .accelMax = 720.0F,
            .degreesPerPulse = 0.045F
    };

    const pulse_series_t exact = getFastestSeries(
            0.0F,
            90.0F,
            stepperOpts,
            pulse_interval_algorithm_t::constantAcceleration);
    const pulse_series_t fallback = getFastestSeries(
            0.0F,
            90.0F,
            stepperOpts,
            pulse_interval_algorithm_t::linearIntervalFallback);

    assert(exact.pulseCount > 0);
    assert(exact.directionForward);
    assert(exact.intervalAlgorithm == pulse_interval_algorithm_t::constantAcceleration);
    assert(std::abs(exact.targetSpeed(stepperOpts) - 90.0F) < 1e-3F);
    assert(exact.intervalSec(0, stepperOpts) > exact.intervalSec(exact.pulseCount - 1, stepperOpts));

    assert(fallback.pulseCount == exact.pulseCount);
    assert(fallback.intervalAlgorithm == pulse_interval_algorithm_t::linearIntervalFallback);
    assert(fallback.intervalSec(0, stepperOpts) > fallback.intervalSec(fallback.pulseCount - 1, stepperOpts));

    float exactMaxAccelError = 0.0F;
    float fallbackMaxAccelError = 0.0F;
    float exactBoundarySpeed = exact.initialSpeed;
    float fallbackBoundarySpeed = fallback.initialSpeed;
    for (std::uint64_t pulseIndex = 0; pulseIndex < exact.pulseCount; ++pulseIndex) {
        const float exactInterval = exact.intervalSec(pulseIndex, stepperOpts);
        const float exactNextBoundarySpeed =
                2.0F * stepperOpts.degreesPerPulse / exactInterval - exactBoundarySpeed;
        const float exactAccel =
                (exactNextBoundarySpeed * exactNextBoundarySpeed
                        - exactBoundarySpeed * exactBoundarySpeed)
                / (2.0F * stepperOpts.degreesPerPulse);
        exactMaxAccelError = std::max(
                exactMaxAccelError,
                std::abs(exactAccel - exact.acceleration));
        exactBoundarySpeed = exactNextBoundarySpeed;

        const float fallbackInterval = fallback.intervalSec(pulseIndex, stepperOpts);
        const float fallbackNextBoundarySpeed =
                2.0F * stepperOpts.degreesPerPulse / fallbackInterval - fallbackBoundarySpeed;
        const float fallbackAccel =
                (fallbackNextBoundarySpeed * fallbackNextBoundarySpeed
                        - fallbackBoundarySpeed * fallbackBoundarySpeed)
                / (2.0F * stepperOpts.degreesPerPulse);
        fallbackMaxAccelError = std::max(
                fallbackMaxAccelError,
                std::abs(fallbackAccel - exact.acceleration));
        fallbackBoundarySpeed = fallbackNextBoundarySpeed;
    }
    assert(exactMaxAccelError < fallbackMaxAccelError);

    pulse_series_t limited = exact;
    limited.limitWithDeg(-2.25F, stepperOpts);
    assert(limited.pulseCount == 50);
    assert(!limited.directionForward);
    assert(std::abs(limited.changeDeg(stepperOpts) + 2.25F) < 1e-5F);

    const float targetChangeDeg = 90.0F;
    const stepper_action_t calculated = calculateAction(
            0.0F,
            targetChangeDeg,
            0.0F,
            stepperOpts);
    assert(calculated.error.empty());
    assert(!calculated.seriesSequence.empty());

    float calculatedChangeDeg = 0.0F;
    for (const pulse_series_t& item : calculated.seriesSequence) {
        assert(item.intervalAlgorithm == pulse_interval_algorithm_t::constantAcceleration);
        calculatedChangeDeg += item.changeDeg(stepperOpts);
    }
    assert(std::abs(calculatedChangeDeg - targetChangeDeg)
            <= stepperOpts.degreesPerPulse / 2.0F + 1e-5F);

    const stepper_action_t fallbackCalculated = calculateAction(
            0.0F,
            targetChangeDeg,
            0.0F,
            stepperOpts,
            pulse_interval_algorithm_t::linearIntervalFallback);
    assert(fallbackCalculated.error.empty());
    assert(!fallbackCalculated.seriesSequence.empty());
    for (const pulse_series_t& item : fallbackCalculated.seriesSequence) {
        assert(item.intervalAlgorithm == pulse_interval_algorithm_t::linearIntervalFallback);
    }

    std::cout << "exact max acceleration error: " << exactMaxAccelError << '\n';
    std::cout << "fallback max acceleration error: " << fallbackMaxAccelError << '\n';
    std::cout << "stepper_motor_probe: OK\n";
    return EXIT_SUCCESS;
}
