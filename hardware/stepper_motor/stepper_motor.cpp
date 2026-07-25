#include "stepper_motor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <thread>

namespace {

constexpr double EPS = 1e-12;

bool isFinitePositive(double value) {
    return std::isfinite(value) && value > 0.0;
}

bool isDirection(int direction) {
    return direction == -1 || direction == 1;
}

std::uint64_t roundedPulseCount(double positionDeg, double degreesPerPulse) {
    return static_cast<std::uint64_t>(std::llround(std::abs(positionDeg) / degreesPerPulse));
}

double pulseInterval(double speedDegPerSec, double degreesPerPulse) {
    return degreesPerPulse / speedDegPerSec;
}

void appendSeries(
        std::vector<pulse_series_t>& result,
        std::uint64_t pulseCount,
        double firstSpeed,
        double lastSpeed,
        double degreesPerPulse,
        int direction,
        bool stopAfterSeries) {
    if (pulseCount == 0) {
        return;
    }

    const double firstInterval = pulseInterval(firstSpeed, degreesPerPulse);
    const double lastInterval = pulseInterval(lastSpeed, degreesPerPulse);
    const double intervalChange = pulseCount > 1
            ? (lastInterval - firstInterval) / static_cast<double>(pulseCount - 1)
            : 0.0;

    result.push_back({
            pulseCount,
            firstInterval,
            intervalChange,
            direction,
            stopAfterSeries
    });
}

} // namespace

const std::string ON_CALCULATE_ACTION = "on CalculateAction(): ";

stepper_action_t CalculateAction(const stepper_options_t& options) {
    stepper_action_t result;

    if (!isFinitePositive(options.freqMax)) {
        result.error = ON_CALCULATE_ACTION + "freqMax must be finite and greater than zero";
        return result;
    }
    if (!isFinitePositive(options.freqAllowed) || options.freqAllowed > options.freqMax) {
        result.error = ON_CALCULATE_ACTION + "freqAllowed must be finite, greater than zero and <= freqMax";
        return result;
    }
    if (!isFinitePositive(options.accelMax)) {
        result.error = ON_CALCULATE_ACTION + "accelMax must be finite and greater than zero";
        return result;
    }
    if (!isFinitePositive(options.degreesPerPulse)) {
        result.error = ON_CALCULATE_ACTION + "degreesPerPulse must be finite and greater than zero";
        return result;
    }
    if (!std::isfinite(options.currentSpeed) || !std::isfinite(options.targetPositionDeg)
            || !std::isfinite(options.targetSpeed)) {
        result.error = ON_CALCULATE_ACTION + "speed and position values must be finite";
        return result;
    }

    const std::uint64_t totalPulses = roundedPulseCount(
            options.targetPositionDeg,
            options.degreesPerPulse);
    if (totalPulses == 0) {
        if (std::abs(options.targetSpeed - options.currentSpeed) > EPS) {
            result.error = ON_CALCULATE_ACTION
                    + "a non-zero speed change cannot be performed without displacement";
        }
        return result;
    }

    const int direction = options.targetPositionDeg > 0.0 ? 1 : -1;
    if (options.currentSpeed * direction < -EPS) {
        result.error = ON_CALCULATE_ACTION
                + "currentSpeed points opposite to targetPositionDeg; braking/reversal is not supported";
        return result;
    }
    if (options.targetSpeed * direction < -EPS) {
        result.error = ON_CALCULATE_ACTION
                + "targetSpeed points opposite to targetPositionDeg";
        return result;
    }

    const double distance = static_cast<double>(totalPulses) * options.degreesPerPulse;
    const double maxSpeed = options.freqAllowed * options.degreesPerPulse;
    const double startSpeed = std::abs(options.currentSpeed);
    const double targetSpeed = std::abs(options.targetSpeed);

    if (startSpeed > maxSpeed + EPS || targetSpeed > maxSpeed + EPS) {
        result.error = ON_CALCULATE_ACTION
                + "currentSpeed and targetSpeed must not exceed freqAllowed * degreesPerPulse";
        return result;
    }

    const double peakByDistance = std::sqrt(std::max(
            0.0,
            options.accelMax * distance
                    + (startSpeed * startSpeed + targetSpeed * targetSpeed) / 2.0));
    const double peakSpeed = std::min(maxSpeed, peakByDistance);

    const double accelDistance = std::max(
            0.0,
            (peakSpeed * peakSpeed - startSpeed * startSpeed) / (2.0 * options.accelMax));
    const double decelDistance = std::max(
            0.0,
            (peakSpeed * peakSpeed - targetSpeed * targetSpeed) / (2.0 * options.accelMax));

    std::uint64_t accelPulses = static_cast<std::uint64_t>(
            std::llround(accelDistance / options.degreesPerPulse));
    std::uint64_t decelPulses = static_cast<std::uint64_t>(
            std::llround(decelDistance / options.degreesPerPulse));

    if (accelPulses + decelPulses > totalPulses) {
        const std::uint64_t overflow = accelPulses + decelPulses - totalPulses;
        if (decelPulses >= overflow) {
            decelPulses -= overflow;
        } else {
            accelPulses -= overflow - decelPulses;
            decelPulses = 0;
        }
    }
    const std::uint64_t cruisePulses = totalPulses - accelPulses - decelPulses;

    const double firstMovingSpeed = std::max(
            options.degreesPerPulse * options.freqAllowed / 1'000'000.0,
            std::sqrt(2.0 * options.accelMax * options.degreesPerPulse));
    const double accelerationStart = std::max(startSpeed, firstMovingSpeed);
    const double decelerationEnd = std::max(targetSpeed, firstMovingSpeed);

    appendSeries(
            result.series,
            accelPulses,
            std::min(accelerationStart, peakSpeed),
            peakSpeed,
            options.degreesPerPulse,
            direction,
            false);
    appendSeries(
            result.series,
            cruisePulses,
            peakSpeed,
            peakSpeed,
            options.degreesPerPulse,
            direction,
            false);
    appendSeries(
            result.series,
            decelPulses,
            peakSpeed,
            std::min(decelerationEnd, peakSpeed),
            options.degreesPerPulse,
            direction,
            targetSpeed <= EPS);

    if (targetSpeed <= EPS && !result.series.empty() && decelPulses == 0) {
        result.series.back().stopAfterSeries = true;
    }

    return result;
}

const std::string ON_EVALUATE_SERIES = "on evaluateSeries(): ";

series_evaluation_t evaluateSeries(
        const pulse_series_t& series,
        double currentSpeed,
        double degreesPerPulse) {
    series_evaluation_t result;

    if (!std::isfinite(currentSpeed)) {
        result.error = ON_EVALUATE_SERIES + "currentSpeed must be finite";
        return result;
    }
    if (!isFinitePositive(degreesPerPulse)) {
        result.error = ON_EVALUATE_SERIES + "degreesPerPulse must be finite and greater than zero";
        return result;
    }
    if (series.pulseCount == 0) {
        result.finalSpeed = currentSpeed;
        return result;
    }
    if (!isDirection(series.direction)) {
        result.error = ON_EVALUATE_SERIES + "direction must be -1 or +1";
        return result;
    }
    if (!isFinitePositive(series.initialIntervalSec) || !std::isfinite(series.intervalChangeSec)) {
        result.error = ON_EVALUATE_SERIES + "interval values are invalid";
        return result;
    }

    const double lastInterval = series.initialIntervalSec
            + series.intervalChangeSec * static_cast<double>(series.pulseCount - 1);
    if (!isFinitePositive(lastInterval)) {
        result.error = ON_EVALUATE_SERIES + "the final pulse interval must be greater than zero";
        return result;
    }

    result.durationSec = static_cast<double>(series.pulseCount)
            * (series.initialIntervalSec + lastInterval) / 2.0;
    result.displacementDeg = static_cast<double>(series.direction)
            * static_cast<double>(series.pulseCount) * degreesPerPulse;
    result.finalSpeed = series.stopAfterSeries
            ? 0.0
            : static_cast<double>(series.direction) * degreesPerPulse / lastInterval;
    return result;
}

const std::string ON_ACTION = "on action(): ";

bool action(
        const pulse_series_t& series,
        const pulse_callback_t& pulseCallback,
        std::string& error) {
    error.clear();

    if (series.pulseCount == 0) {
        return true;
    }
    if (!isDirection(series.direction)) {
        error = ON_ACTION + "direction must be -1 or +1";
        return false;
    }
    if (!pulseCallback) {
        error = ON_ACTION + "pulseCallback is empty";
        return false;
    }
    if (!isFinitePositive(series.initialIntervalSec) || !std::isfinite(series.intervalChangeSec)) {
        error = ON_ACTION + "interval values are invalid";
        return false;
    }

    auto nextPulseTime = std::chrono::steady_clock::now();
    for (std::uint64_t pulseIndex = 0; pulseIndex < series.pulseCount; ++pulseIndex) {
        const double intervalSec = series.initialIntervalSec
                + series.intervalChangeSec * static_cast<double>(pulseIndex);
        if (!isFinitePositive(intervalSec)) {
            error = ON_ACTION + "pulse interval must remain greater than zero";
            return false;
        }

        nextPulseTime += std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(intervalSec));
        std::this_thread::sleep_until(nextPulseTime);

        if (!pulseCallback(series.direction)) {
            error = ON_ACTION + "pulseCallback failed";
            return false;
        }
    }

    return true;
}
