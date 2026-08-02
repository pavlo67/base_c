#include "stepper_motor.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "_defines.h"
#include "lib/mathlib.h"

StepperSeries getFastestSeries(float initialSpeedDegPerSec, float finalSpeedDegPerSec, const stepper_options_t& stepperOpts, interval_algorithm_t intervalAlgorithm) {
    const float speedMax = std::min(stepperOpts.speedMaxDegSec,stepperOpts.freqMax * stepperOpts.degPulse);
    if (std::abs(finalSpeedDegPerSec) > speedMax) {
        finalSpeedDegPerSec = finalSpeedDegPerSec >= 0 ? speedMax : -speedMax;
    }

    const float directionSpeed = std::abs(finalSpeedDegPerSec) > EPS ? finalSpeedDegPerSec : initialSpeedDegPerSec;
    const bool directionForward = directionSpeed >= 0.0F;

    const float distanceDeg = std::abs(finalSpeedDegPerSec * finalSpeedDegPerSec - initialSpeedDegPerSec * initialSpeedDegPerSec) / (2.0F * stepperOpts.accelMaxDegSec2);
    const uint64_t pulseCount = std::max<uint64_t>(1, static_cast<uint64_t>(std::ceil(distanceDeg / stepperOpts.degPulse)));

    StepperSeries s(pulseCount, initialSpeedDegPerSec,finalSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm);

    return s;
}

bool getAcceleratedSequence(std::vector<StepperSeries>& seriesSequence, float baseSpeedDegPerSec, float targetRotationDeg, const stepper_options_t& stepperOpts, interval_algorithm_t intervalAlgorithm) {
    const float         baseSpeed        = std::abs(baseSpeedDegPerSec);
    const float         speedMax         = std::min(stepperOpts.speedMaxDegSec,stepperOpts.freqMax * stepperOpts.degPulse);
    const bool          directionForward = targetRotationDeg > 0.0F;
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
        seriesSequence.push_back(StepperSeries(accelerationPulses, baseSpeedDegPerSec,cruiseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }
    if (cruisePulses) {
        seriesSequence.push_back(StepperSeries(cruisePulses, cruiseSpeedDegPerSec,cruiseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }
    if (accelerationPulses) {
        seriesSequence.push_back(StepperSeries(accelerationPulses, cruiseSpeedDegPerSec,baseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }

    return true;
}

bool optionsIsOk(const stepper_options_t& stepperOpts, std::string& error) {
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

stepper_sequence_t calculateSequence(
        float currentSpeedDegPerSec, float totalRotationDeg, float targetSpeedDegPerSec,
        const stepper_options_t& stepperOpts, interval_algorithm_t intervalAlgorithm) {

    stepper_sequence_t result;

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
        if (!getAcceleratedSequence(result.seriesSequence, currentSpeedDegPerSec, totalRotationDeg, stepperOpts, intervalAlgorithm)) {
            result.error = "accelerationSequence for fixed speed isn't calculated correctly";
        }
        return result;
    }

    POINT(2,0)

    StepperSeries fastestSeries = getFastestSeries(currentSpeedDegPerSec, targetSpeedDegPerSec, stepperOpts, intervalAlgorithm);

    POINT(3,0)

    float changeDegMin = fastestSeries.totalRotationDeg(stepperOpts);
    if (std::abs(changeDegMin - totalRotationDeg) < stepperOpts.degPulse) {

        POINT(3,1)

        result.seriesSequence.push_back(fastestSeries);
        return result;
    } else if (std::abs(changeDegMin) > std::abs(totalRotationDeg)) {

        POINT(3,2)

        fastestSeries.limitWithDeg(totalRotationDeg, stepperOpts);
        float changeDeg = fastestSeries.totalRotationDeg(stepperOpts);
        result.error = (std::abs(changeDeg - totalRotationDeg) < stepperOpts.degPulse)
                     ? "fastest series is cutted to targetChangeDeg" : "fastest series isn't cutted to targetChangeDeg correctly";
        result.seriesSequence.push_back(fastestSeries);
        return result;
    } else if (std::abs(targetSpeedDegPerSec) > std::abs(currentSpeedDegPerSec)) {

        POINT(3,3)

        result.seriesSequence.push_back(fastestSeries);
        const float remainingChangeDeg = totalRotationDeg - fastestSeries.totalRotationDeg(stepperOpts);
        if (!getAcceleratedSequence(result.seriesSequence, fastestSeries.finalSpeed(stepperOpts), remainingChangeDeg, stepperOpts, intervalAlgorithm)) {
            result.error = "accelerationSequence for target speed isn't calculated correctly";
        }
        return result;
    } else {

        POINT(3,4)

        const float remainingChangeDeg = totalRotationDeg - fastestSeries.totalRotationDeg(stepperOpts);
        if (!getAcceleratedSequence(result.seriesSequence, currentSpeedDegPerSec, remainingChangeDeg, stepperOpts, intervalAlgorithm)) {
            result.error = "accelerationSequence for current speed isn't calculated correctly";
        }
        result.seriesSequence.push_back(fastestSeries);
    }

    return result;
}

// const std::string ON_EVALUATE_SERIES = "on evaluateSeries(): ";
//
// series_evaluation_t evaluateSeries(
//         const pulse_series_t& series,
//         double currentSpeed,
//         double degreesPerPulse) {
//     series_evaluation_t result;
//
//     if (!std::isfinite(currentSpeed)) {
//         result.error = ON_EVALUATE_SERIES + "currentSpeed must be finite";
//         return result;
//     }
//     if (!isFinitePositive(degreesPerPulse)) {
//         result.error = ON_EVALUATE_SERIES + "degreesPerPulse must be finite and greater than zero";
//         return result;
//     }
//     if (series.pulseCount == 0) {
//         result.finalSpeed = currentSpeed;
//         return result;
//     }
//     if (!isDirection(series.direction)) {
//         result.error = ON_EVALUATE_SERIES + "direction must be -1 or +1";
//         return result;
//     }
//     if (!isFinitePositive(series.initialIntervalSec) || !std::isfinite(series.intervalChangePerPulse)) {
//         result.error = ON_EVALUATE_SERIES + "interval values are invalid";
//         return result;
//     }
//
//     const double lastInterval = series.initialIntervalSec
//             + series.intervalChangePerPulse * static_cast<double>(series.pulseCount - 1);
//     if (!isFinitePositive(lastInterval)) {
//         result.error = ON_EVALUATE_SERIES + "the final pulse interval must be greater than zero";
//         return result;
//     }
//
//     result.durationSec = static_cast<double>(series.pulseCount)
//             * (series.initialIntervalSec + lastInterval) / 2.0;
//     result.displacementDeg = static_cast<double>(series.direction)
//             * static_cast<double>(series.pulseCount) * degreesPerPulse;
//     result.finalSpeed = static_cast<double>(series.direction) * degreesPerPulse / lastInterval;
//     return result;
// }
//
// const std::string ON_ACTION = "on action(): ";
//
// bool action(
//         const pulse_series_t& series,
//         const pulse_callback_t& pulseCallback,
//         std::string& error) {
//     error.clear();
//
//     if (series.pulseCount == 0) {
//         return true;
//     }
//     if (!isDirection(series.direction)) {
//         error = ON_ACTION + "direction must be -1 or +1";
//         return false;
//     }
//     if (!pulseCallback) {
//         error = ON_ACTION + "pulseCallback is empty";
//         return false;
//     }
//     if (!isFinitePositive(series.initialIntervalSec) || !std::isfinite(series.intervalChangePerPulse)) {
//         error = ON_ACTION + "interval values are invalid";
//         return false;
//     }
//
//     auto nextPulseTime = std::chrono::steady_clock::now();
//     for (uint64_t pulseIndex = 0; pulseIndex < series.pulseCount; ++pulseIndex) {
//         const double intervalSec = series.initialIntervalSec
//                 + series.intervalChangePerPulse * static_cast<double>(pulseIndex);
//         if (!isFinitePositive(intervalSec)) {
//             error = ON_ACTION + "pulse interval must remain greater than zero";
//             return false;
//         }
//
//         nextPulseTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
//                 std::chrono::duration<double>(intervalSec));
//         std::this_thread::sleep_until(nextPulseTime);
//
//         if (!pulseCallback(series.direction)) {
//             error = ON_ACTION + "pulseCallback failed";
//             return false;
//         }
//     }
//
//     return true;
// }
