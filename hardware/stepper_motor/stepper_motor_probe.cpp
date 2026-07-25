#include "stepper_motor.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>

constexpr double EPS = 1e-9;

int main() {
    const stepper_options_t options {
            .freqMax = 20'000.0,
            .freqAllowed = 5'000.0,
            .accelMax = 720.0,
            .currentSpeed = 0.0,
            .targetPositionDeg = 90.0,
            .targetSpeed = 0.0,
            .degreesPerPulse = 0.045
    };

    const stepper_action_t calculated = CalculateAction(options);
    assert(calculated.error.empty());
    assert(!calculated.series.empty());

    double speed = options.currentSpeed;
    double displacement = 0.0;
    for (const pulse_series_t& series : calculated.series) {
        const series_evaluation_t evaluated = evaluateSeries(
                series,
                speed,
                options.degreesPerPulse);
        assert(evaluated.error.empty());
        displacement += evaluated.displacementDeg;
        speed = evaluated.finalSpeed;
    }

    assert(std::abs(displacement - options.targetPositionDeg) <= options.degreesPerPulse / 2.0 + EPS);
    assert(std::abs(speed - options.targetSpeed) <= EPS);

    std::cout << "stepper_motor_probe: OK\n";
    return EXIT_SUCCESS;
}
