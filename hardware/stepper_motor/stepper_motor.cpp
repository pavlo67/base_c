#include "stepper_motor.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "lib/mathlib.h"

PulseSeries getFastestSeries(float initialSpeedDegPerSec, float finalSpeedDegPerSec, const stepper_options_t& stepperOpts, interval_algorithm_t intervalAlgorithm) {
    const float directionSpeed = std::abs(finalSpeedDegPerSec) > EPS ? finalSpeedDegPerSec : initialSpeedDegPerSec;
    const bool directionForward = directionSpeed >= 0.0F;

    const float distanceDeg = std::abs(finalSpeedDegPerSec * finalSpeedDegPerSec - initialSpeedDegPerSec * initialSpeedDegPerSec) / (2.0F * stepperOpts.accelMax);
    const std::uint64_t pulseCount = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::ceil(distanceDeg / stepperOpts.degreesPerPulse)));

    PulseSeries s(pulseCount, initialSpeedDegPerSec,finalSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm);

    return s;
}

bool getAcceleratedSequence(std::vector<PulseSeries>& seriesSequence, float baseSpeedDegPerSec, float totalRotationDeg, const stepper_options_t& stepperOpts, interval_algorithm_t intervalAlgorithm) {
    if (std::abs(totalRotationDeg) < stepperOpts.degreesPerPulse / 2.0F) { return true; }

    if (totalRotationDeg * baseSpeedDegPerSec < -EPS) { return false; }

    const bool directionForward = totalRotationDeg > 0.0F;
    const float baseSpeed = std::abs(baseSpeedDegPerSec);
    const float speedMaxAllowed = stepperOpts.freqMaxAllowed * stepperOpts.degreesPerPulse;

    if (baseSpeed > speedMaxAllowed + EPS) { return false; }

    const std::uint64_t totalPulses = stepperOpts.pulsesForDeg(totalRotationDeg);
    if (totalPulses == 0) { return true; }

    const float totalDeg = static_cast<float>(totalPulses) * stepperOpts.degreesPerPulse;
    const float peakByDistance = std::sqrt(baseSpeed * baseSpeed + stepperOpts.accelMax * totalDeg);
    const float peakSpeed = std::min(speedMaxAllowed, peakByDistance);
    const float accelerationDeg = std::max(0.0F,(peakSpeed * peakSpeed - baseSpeed * baseSpeed) / (2.0F * stepperOpts.accelMax));

    auto accelerationPulses = static_cast<std::uint64_t>(std::floor(accelerationDeg / stepperOpts.degreesPerPulse));
    accelerationPulses = std::min(accelerationPulses, totalPulses / 2);
    const std::uint64_t cruisePulses = totalPulses - 2 * accelerationPulses;
    const float actualPeakSpeed = std::sqrt(baseSpeed * baseSpeed + 2.0F * stepperOpts.accelMax  * static_cast<float>(accelerationPulses) * stepperOpts.degreesPerPulse);

    if (accelerationPulses) {
        seriesSequence.push_back(PulseSeries(accelerationPulses, baseSpeed,actualPeakSpeed, directionForward, stepperOpts, intervalAlgorithm));
    }
    if (cruisePulses) {
        seriesSequence.push_back(PulseSeries(cruisePulses, actualPeakSpeed,actualPeakSpeed, directionForward, stepperOpts, intervalAlgorithm));
    }
    if (accelerationPulses) {
        seriesSequence.push_back(PulseSeries(cruisePulses, actualPeakSpeed,baseSpeed, directionForward, stepperOpts, intervalAlgorithm));
    }

    return true;
}

bool optionsIsOk(const stepper_options_t& stepperOpts, std::string& error) {
    if (!isFinitePositive(stepperOpts.freqMax)) {
        error = "freqMax must be finite and greater than zero";
        return false;
    }
    if (!isFinitePositive(stepperOpts.freqMaxAllowed) || stepperOpts.freqMaxAllowed > stepperOpts.freqMax) {
        error = "freqAllowed must be finite, greater than zero and <= freqMax";
        return false;
    }
    if (!isFinitePositive(stepperOpts.accelMax)) {
        error = "accelMax must be finite and greater than zero";
        return false;
    }
    if (!isFinitePositive(stepperOpts.degreesPerPulse)) {
        error = "degreesPerPulse must be finite and greater than zero";
        return false;
    }

    return true;
}

stepper_action_t calculateAction(
        float currentSpeedDegPerSec, float totalRotationDeg, float targetSpeedDegPerSec,
        const stepper_options_t& stepperOpts, interval_algorithm_t intervalAlgorithm) {

    stepper_action_t result;

    // it should be checked at system initialization
    // if (!optionsIsOk(stepperOpts, result.error)) { return result; }

    if (!std::isfinite(currentSpeedDegPerSec) || !std::isfinite(totalRotationDeg) || !std::isfinite(targetSpeedDegPerSec)) {
        result.error = "speed and position values must be finite";
        return result;
    }

    const float speedMaxAllowed = stepperOpts.freqMaxAllowed * stepperOpts.degreesPerPulse;
    if (std::abs(targetSpeedDegPerSec) > speedMaxAllowed) {
        result.error = "targetSpeed must not exceed freqAllowed * degreesPerPulse";
        return result;
    }

    // sad if so, but we can't change the actual state of the system
    // if (std::abs(currentSpeed) > speedMaxAllowed)

    const bool targetDirectionForward = totalRotationDeg > 0.0;
    if (currentSpeedDegPerSec * (targetDirectionForward ? 1. : -1.) < -EPS) {
        result.error = "targetChangeDeg is opposite to currentSpeed; braking/reversal is not supported";
        return result;
    }

    if (targetSpeedDegPerSec * currentSpeedDegPerSec < -EPS) {
        result.error = "targetSpeed is opposite to currentSpeed";
        return result;
    }


    if (std::abs(targetSpeedDegPerSec - currentSpeedDegPerSec) < SPEED_DEG_PER_SEC_EPS) {
        if (!getAcceleratedSequence(result.seriesSequence, currentSpeedDegPerSec, totalRotationDeg, stepperOpts, intervalAlgorithm)) {
            result.error = "accelerationSequence for fixed speed isn't calculated correctly";
        }
        return result;
    }

    PulseSeries fastestSeries = getFastestSeries(currentSpeedDegPerSec, targetSpeedDegPerSec, stepperOpts, intervalAlgorithm);

    float changeDegMin = fastestSeries.totalRotationDeg(stepperOpts);
    if (std::abs(changeDegMin - totalRotationDeg) < stepperOpts.degreesPerPulse) {
        result.seriesSequence.push_back(fastestSeries);
        return result;
    } else if (std::abs(changeDegMin) > std::abs(totalRotationDeg)) {
        fastestSeries.limitWithDeg(totalRotationDeg, stepperOpts);
        float changeDeg = fastestSeries.totalRotationDeg(stepperOpts);
        result.error = (std::abs(changeDeg - totalRotationDeg) < stepperOpts.degreesPerPulse)
                     ? "fastest series is cutted to targetChangeDeg" : "fastest series isn't cutted to targetChangeDeg correctly";
        result.seriesSequence.push_back(fastestSeries);
        return result;
    } else if (std::abs(targetSpeedDegPerSec) > std::abs(currentSpeedDegPerSec)) {
        result.seriesSequence.push_back(fastestSeries);
        const float remainingChangeDeg = totalRotationDeg - fastestSeries.totalRotationDeg(stepperOpts);
        if (!getAcceleratedSequence(result.seriesSequence, fastestSeries.finalSpeed(stepperOpts), remainingChangeDeg, stepperOpts, intervalAlgorithm)) {
            result.error = "accelerationSequence for target speed isn't calculated correctly";
        }
        return result;
    } else {
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
//     for (std::uint64_t pulseIndex = 0; pulseIndex < series.pulseCount; ++pulseIndex) {
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
