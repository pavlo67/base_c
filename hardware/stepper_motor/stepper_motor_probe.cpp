#include "stepper_motor.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

constexpr double EPS = 1e-9;

int main() {
    const stepper_options_t stepperOpts {
            .freqMax = 20'000.0,
            .freqMaxAllowed = 5'000.0,
            .accelMax = 720.0,
            .degreesPerPulse = 0.045
    };

    const float initialSpeed = 0.0;
    const float targetChangeDeg = 90.0;
    const float targetSpeed = 0.0;

    const stepper_action_t calculated = calculateAction(initialSpeed, targetChangeDeg, targetSpeed, stepperOpts);
    assert(calculated.error.empty());
    assert(!calculated.seriesSequence.empty());

    float currentSpeed = initialSpeed;

    double displacement = 0.0;
    for (const pulse_series_t& series : calculated.seriesSequence) {
        const series_evaluation_t evaluated = evaluateSeries(
                series,
                currentSpeed,
                stepperOpts.degreesPerPulse);
        assert(evaluated.error.empty());
        displacement += evaluated.displacementDeg;
        currentSpeed = evaluated.finalSpeed;
    }

    assert(std::abs(displacement - targetChangeDeg) <= stepperOpts.degreesPerPulse / 2.0 + EPS);
    assert(std::abs(currentSpeed - targetSpeed) <= EPS);

    std::cout << "stepper_motor_probe: OK\n";
    return EXIT_SUCCESS;
}
