#include "stepper_motor.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "_base_defines.h"
#include "lib/mathlib.h"

StepperMotorSeries getFastestSeries(float initialSpeedDegPerSec, float finalSpeedDegPerSec, const stepper_motor_options_t& stepperOpts, stepper_motor_algorithm_t intervalAlgorithm) {
    const float speedMax = std::min(stepperOpts.speedMaxDegSec,stepperOpts.freqMax * stepperOpts.degPulse);
    if (std::abs(finalSpeedDegPerSec) > speedMax) {
        finalSpeedDegPerSec = finalSpeedDegPerSec >= 0 ? speedMax : -speedMax;
    }

    const float directionSpeed = std::abs(finalSpeedDegPerSec) > EPS ? finalSpeedDegPerSec : initialSpeedDegPerSec;
    const bool directionForward = directionSpeed >= 0.0F;

    const float distanceDeg = std::abs(finalSpeedDegPerSec * finalSpeedDegPerSec - initialSpeedDegPerSec * initialSpeedDegPerSec) / (2.0F * stepperOpts.accelMaxDegSec2);
    const uint64_t pulseCount = std::max<uint64_t>(1, static_cast<uint64_t>(std::ceil(distanceDeg / stepperOpts.degPulse)));

    StepperMotorSeries s(pulseCount, initialSpeedDegPerSec,finalSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm);

    return s;
}

bool addAcceleratedSeries(stepper_motor_series_sequence_t& seriesSequence, float baseSpeedDegPerSec, float targetRotationDeg, const stepper_motor_options_t& stepperOpts, stepper_motor_algorithm_t intervalAlgorithm) {
    const float    baseSpeed        = std::abs(baseSpeedDegPerSec);
    const float    speedMax         = std::min(stepperOpts.speedMaxDegSec,stepperOpts.freqMax * stepperOpts.degPulse);
    const bool     directionForward = targetRotationDeg > 0.0F;
    const uint64_t totalPulses      = stepperOpts.pulsesForDeg(targetRotationDeg, directionForward);

    POINT(2, 1)
    if (std::abs(targetRotationDeg) < stepperOpts.degPulse) {
        POINT(2, 2)
        return true;
    } else if (targetRotationDeg * baseSpeedDegPerSec < 0) {
        POINT(2, 3)
        return false;
    } else if (baseSpeed > speedMax + EPS) {
        POINT(2, 4)
        return false;
    } else if (totalPulses == 0) {
        POINT(2, 5)
        return true;
    }
    POINT(2, 6)

    const float absRotationDeg   = static_cast<float>(totalPulses) * stepperOpts.degPulse;
    const float peakSpeed        = std::sqrt(baseSpeed * baseSpeed + stepperOpts.accelMaxDegSec2 * absRotationDeg); // кінематичне рівняння для totalRotationDeg/2
    const float peakSpeedAllowed = std::min(speedMax, peakSpeed);
    const float accelerationDeg  = std::max(0.0F,(peakSpeedAllowed * peakSpeedAllowed - baseSpeed * baseSpeed) / (2.0F * stepperOpts.accelMaxDegSec2));

    const uint64_t accelerationPulses    = std::min(static_cast<uint64_t>(std::floor(accelerationDeg / stepperOpts.degPulse)), totalPulses / 2);
    const uint64_t cruisePulses          = totalPulses - 2 * accelerationPulses;
    const float    actualPeakSpeed       = std::sqrt(baseSpeed * baseSpeed + 2.0F * stepperOpts.accelMaxDegSec2  * static_cast<float>(accelerationPulses) * stepperOpts.degPulse);
    const float    cruiseSpeedDegPerSec  = (targetRotationDeg >= 0) ? actualPeakSpeed : -actualPeakSpeed;

    if (accelerationPulses) {
        seriesSequence.seq.push_back(StepperMotorSeries(accelerationPulses, baseSpeedDegPerSec,cruiseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }
    if (cruisePulses) {
        seriesSequence.seq.push_back(StepperMotorSeries(cruisePulses, cruiseSpeedDegPerSec,cruiseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }
    if (accelerationPulses) {
        seriesSequence.seq.push_back(StepperMotorSeries(accelerationPulses, cruiseSpeedDegPerSec,baseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }

    return true;
}

bool optionsIsOk(const stepper_motor_options_t& stepperOpts, std::string& error) {
    if (!isFinitePositive(stepperOpts.freqMax)) {
        error = "freqMax must be finite and greater than zero";
        return false;
    }
    if (!isFinitePositive(stepperOpts.speedMaxDegSec)) {
        error = "freqAllowed must be finite positive";
        return false;
    }
    if (!isFinitePositive(stepperOpts.accelMaxDegSec2)) {
        error = "accelMax must be finite and greater than zero";
        return false;
    }
    if (!isFinitePositive(stepperOpts.degPulse)) {
        error = "degreesPerPulse must be finite and greater than zero";
        return false;
    }

    return true;
}

stepper_motor_series_sequence_t getSeriesSequence(
        float currentSpeedDegPerSec, float totalRotationDeg, float targetSpeedDegPerSec,
        const stepper_motor_options_t& stepperOpts, stepper_motor_algorithm_t intervalAlgorithm) {

    stepper_motor_series_sequence_t result;

    // it should be checked at system initialization
    // if (!optionsIsOk(stepperOpts, result.error)) { return result; }

    if (!std::isfinite(currentSpeedDegPerSec) || !std::isfinite(totalRotationDeg) || !std::isfinite(targetSpeedDegPerSec)) {
        result.error = "speed and position values must be finite";
        return result;
    }

    const float speedMax = std::min(stepperOpts.speedMaxDegSec,stepperOpts.freqMax * stepperOpts.degPulse);
    if (std::abs(targetSpeedDegPerSec) > speedMax) {
        result.error = "targetSpeed must not exceed freqAllowed * degreesPerPulse";
        return result;
    }

    // sad if so, but we can't change the actual state of the system
    // if (std::abs(currentSpeed) > speedMax)

    const bool targetDirectionForward = totalRotationDeg > 0.0;
    if (currentSpeedDegPerSec * (targetDirectionForward ? 1. : -1.) < -EPS) {
        result.error = "targetChangeDeg is opposite to currentSpeed; braking/reversal is not supported";
        return result;
    }

    // printf("\ntotalRotationDeg: %f --> targetDirectionForward: %d\n\n", totalRotationDeg, targetDirectionForward);

    if (targetSpeedDegPerSec * currentSpeedDegPerSec < -EPS) {
        result.error = "targetSpeed is opposite to currentSpeed";
        return result;
    }

    POINT(1,0)

    if (std::abs(targetSpeedDegPerSec - currentSpeedDegPerSec) <= std::max(SPEED_EPS, std::abs(targetSpeedDegPerSec) * RESULT_EPS_RATIO)) {
        if (!addAcceleratedSeries(result, currentSpeedDegPerSec, totalRotationDeg, stepperOpts, intervalAlgorithm)) {
            result.error = "accelerationSequence for fixed speed isn't calculated correctly";
            // ??? and what
        }
        return result;
    }

    POINT(2,0)

    StepperMotorSeries fastestSeries = getFastestSeries(currentSpeedDegPerSec, targetSpeedDegPerSec, stepperOpts, intervalAlgorithm);

    POINT(3,0)

    float changeDegMin = fastestSeries.totalRotationDeg(stepperOpts);
    if (std::abs(changeDegMin - totalRotationDeg) < stepperOpts.degPulse) {

        POINT(3,1)

        result.seq.push_back(fastestSeries);
        return result;
    } else if (std::abs(changeDegMin) > std::abs(totalRotationDeg)) {

        POINT(3,2)

        fastestSeries.limitWithDeg(totalRotationDeg, stepperOpts);
        float changeDeg = fastestSeries.totalRotationDeg(stepperOpts);
        result.error = (std::abs(changeDeg - totalRotationDeg) < stepperOpts.degPulse)
                     ? "fastest series is cutted to targetChangeDeg" : "fastest series isn't cutted to targetChangeDeg correctly";
        result.seq.push_back(fastestSeries);
        return result;
    } else if (std::abs(targetSpeedDegPerSec) > std::abs(currentSpeedDegPerSec)) {

        POINT(3,3)

        result.seq.push_back(fastestSeries);
        const float remainingChangeDeg = totalRotationDeg - fastestSeries.totalRotationDeg(stepperOpts);
        if (!addAcceleratedSeries(result, fastestSeries.finalSpeed(stepperOpts), remainingChangeDeg, stepperOpts, intervalAlgorithm)) {
            result.error = "accelerationSequence for target speed isn't calculated correctly";
            // ??? and what
        }
        return result;
    } else {

        POINT(3,4)

        const float remainingChangeDeg = totalRotationDeg - fastestSeries.totalRotationDeg(stepperOpts);
        if (!addAcceleratedSeries(result, currentSpeedDegPerSec, remainingChangeDeg, stepperOpts, intervalAlgorithm)) {
            result.error = "accelerationSequence for current speed isn't calculated correctly";
        }
        result.seq.push_back(fastestSeries);
    }

    return result;
}
