#include "stepper_motor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#include "lib/mathlib.h"

inline float pulseInterval(float speedDegPerSec, float degreesPerPulse) {
    return degreesPerPulse / speedDegPerSec;
}
void appendSeries(std::vector<pulse_series_t>& seriesSequence, std::uint64_t pulseCount, float firstSpeedDegPerSec, float lastSpeedDegPerSec, bool directionForward, const stepper_options_t& stepperOpts) {
    if (pulseCount == 0) { return; }

    const float firstInterval = pulseInterval(firstSpeedDegPerSec, stepperOpts.degreesPerPulse);
    const float lastInterval = pulseInterval(lastSpeedDegPerSec, stepperOpts.degreesPerPulse);
    const float intervalChange = pulseCount > 1 ? (lastInterval - firstInterval) / static_cast<float>(pulseCount - 1) : 0.0;

    seriesSequence.push_back({pulseCount,firstInterval, intervalChange, directionForward});
}

pulse_series_t getFastestSeries(float currentSpeed, float changeSpeed, const stepper_options_t& stepperOpts) {
    // TODO!!!
    return {};
}

bool getAcceleratedSequence(std::vector<pulse_series_t>& seriesSequence, float currentSpeed, float targetChangeDeg, const stepper_options_t& stepperOpts) {
    if (targetChangeDeg * currentSpeed < -EPS) {
        return false;
    }

    // TODO!!! get maximal acceleration for accelerationDeg <= targetChangeDeg / 2
    // TODO!!! get maximal acceleration downward for accelerationDownDeg <= targetChangeDeg - accelerationDeg
    // TODO!!! add cruise serias wit max speed for targetChangeDeg - accelerationDeg - accelerationDownDeg

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

stepper_action_t calculateAction(float currentSpeed, float targetChangeDeg, float targetSpeed, const stepper_options_t& stepperOpts) {
    stepper_action_t result;

    // it should be checked at system initialization
    // if (!optionsIsOk(stepperOpts, result.error)) { return result; }

    if (!std::isfinite(currentSpeed) || !std::isfinite(targetChangeDeg) || !std::isfinite(targetSpeed)) {
        result.error = "speed and position values must be finite";
        return result;
    }

    const float speedMaxAllowed = stepperOpts.freqMaxAllowed * stepperOpts.degreesPerPulse;
    if (std::abs(targetSpeed) > speedMaxAllowed) {
        result.error = "targetSpeed must not exceed freqAllowed * degreesPerPulse";
        return result;
    }

    // sad if so, but we can't change the actual state of the system
    // if (std::abs(currentSpeed) > speedMaxAllowed)

    const bool targetDirectionForward = targetChangeDeg > 0.0;
    if (currentSpeed * (targetDirectionForward ? 1. : -1.) < -EPS) {
        result.error = "targetChangeDeg is opposite to currentSpeed; braking/reversal is not supported";
        return result;
    }

    if (targetSpeed * currentSpeed < -EPS) {
        result.error = "targetSpeed is opposite to currentSpeed";
        return result;
    }


    if (std::abs(targetSpeed - currentSpeed) < DELTA_SPEED_EPS) {
        if (!getAcceleratedSequence(result.seriesSequence, currentSpeed, targetChangeDeg, stepperOpts)) {
            result.error = "accelerationSequence for fixed speed isn't calculated correctly";
        }
        return result;
    }

    pulse_series_t fastestSeries = getFastestSeries(currentSpeed, targetSpeed - currentSpeed, stepperOpts);

    float changeDegMin = fastestSeries.changeDeg(stepperOpts);
    if (std::abs(changeDegMin - targetChangeDeg) < stepperOpts.degreesPerPulse) {
        result.seriesSequence.push_back(fastestSeries);
        return result;
    } else if (std::abs(changeDegMin) > std::abs(targetChangeDeg)) {
        fastestSeries.limitWithDeg(targetChangeDeg, stepperOpts);
        float changeDeg = fastestSeries.changeDeg(stepperOpts);
        result.error = (std::abs(changeDeg - targetChangeDeg) < stepperOpts.degreesPerPulse)
                     ? "fastest series is cutted to targetChangeDeg" : "fastest series isn't cutted to targetChangeDeg correctly";
        result.seriesSequence.push_back(fastestSeries);
        return result;
    } else if (std::abs(targetSpeed) > std::abs(currentSpeed)) {
        result.seriesSequence.push_back(fastestSeries);
        if (!getAcceleratedSequence(result.seriesSequence, fastestSeries.targetSpeed(stepperOpts), targetChangeDeg, stepperOpts)) {
            result.error = "accelerationSequence for target speed isn't calculated correctly";
        }
        return result;
    } else {
        if (!getAcceleratedSequence(result.seriesSequence, currentSpeed, targetChangeDeg, stepperOpts)) {
            result.error = "accelerationSequence for current speed isn't calculated correctly";
        }
        result.seriesSequence.push_back(fastestSeries);
    }


    // const std::uint64_t totalPulses = std::llround(std::abs(targetChangeDeg) / options.degreesPerPulse);
    // if (totalPulses == 0) {
    //     if (std::abs(targetSpeed - currentSpeed) > EPS) {
    //         result.error = "a non-zero speed change cannot be performed without displacement";
    //     }
    //     return result;
    // }

    // const float changeDegMax = float(totalPulses) * stepperOpts.degreesPerPulse;
    // const float peakByDistance = std::sqrt(std::max(
    //         0.0,
    //         stepperOpts.accelMax * changeDegMax
    //                 + (startSpeed * startSpeed + targetSpeed * targetSpeed) / 2.0));
    // const float peakSpeed = std::min(speedMaxAllowed, peakByDistance);
    //
    // const double accelDistance = std::max(
    //         0.0,
    //         (peakSpeed * peakSpeed - startSpeed * startSpeed) / (2.0 * stepperOpts.accelMax));
    // const double decelDistance = std::max(
    //         0.0,
    //         (peakSpeed * peakSpeed - targetSpeed * targetSpeed) / (2.0 * stepperOpts.accelMax));
    //
    // std::uint64_t accelPulses = static_cast<std::uint64_t>(
    //         std::llround(accelDistance / stepperOpts.degreesPerPulse));
    // std::uint64_t decelPulses = static_cast<std::uint64_t>(
    //         std::llround(decelDistance / stepperOpts.degreesPerPulse));
    //
    // if (accelPulses + decelPulses > totalPulses) {
    //     const std::uint64_t overflow = accelPulses + decelPulses - totalPulses;
    //     if (decelPulses >= overflow) {
    //         decelPulses -= overflow;
    //     } else {
    //         accelPulses -= overflow - decelPulses;
    //         decelPulses = 0;
    //     }
    // }
    // const std::uint64_t cruisePulses = totalPulses - accelPulses - decelPulses;
    //
    // const double firstMovingSpeed = std::max(
    //         stepperOpts.degreesPerPulse * stepperOpts.freqMaxAllowed / 1'000'000.0,
    //         std::sqrt(2.0 * stepperOpts.accelMax * stepperOpts.degreesPerPulse));
    // const double accelerationStart = std::max(startSpeed, firstMovingSpeed);
    // const double decelerationEnd = std::max(targetSpeed, firstMovingSpeed);
    //
    // appendSeries(
    //         result.series,
    //         accelPulses,
    //         std::min(accelerationStart, peakSpeed),
    //         peakSpeed,
    //         stepperOpts.degreesPerPulse,
    //         targetDirectionForward);
    // appendSeries(
    //         result.series,
    //         cruisePulses,
    //         peakSpeed,
    //         peakSpeed,
    //         stepperOpts.degreesPerPulse,
    //         targetDirectionForward);
    // appendSeries(
    //         result.series,
    //         decelPulses,
    //         peakSpeed,
    //         std::min(decelerationEnd, peakSpeed),
    //         stepperOpts.degreesPerPulse,
    //         targetDirectionForward);
    //
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
