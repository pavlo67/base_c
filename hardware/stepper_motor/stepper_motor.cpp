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

static bool calculateAcceleratedSeries(StepperMotorSeriesSequence& seriesSequence, float baseSpeedDegPerSec, float targetRotationDeg, duration expecterInterval, const stepper_motor_options_t& stepperOpts, stepper_motor_algorithm_t intervalAlgorithm) {
    const float    baseSpeed        = std::abs(baseSpeedDegPerSec);
    const float    speedMax         = std::min(stepperOpts.speedMaxDegSec,stepperOpts.freqMax * stepperOpts.degPulse);
    const bool     directionForward = targetRotationDeg > 0.0F;
    const uint64_t totalPulses      = stepperOpts.pulsesForDeg(targetRotationDeg, directionForward);

    POINT(2, 1)
    if (totalPulses == 0) {
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

    const moment startedAt = seriesSequence.seq.empty() ? 0 : seriesSequence.seq.back().observedAt_;
    if (expecterInterval && totalPulses > 1 && baseSpeed < speedMax) {
        // Advance the acceleration law once per accepted timer update. Repeated
        // PWM pulses contribute to displacement, but do not advance that law.
        StepperMotorSeries acceleration = getFastestSeries(baseSpeedDegPerSec,
            directionForward ? speedMax : -speedMax, stepperOpts, intervalAlgorithm);
        acceleration.stopAfterPulses_ = (totalPulses + 1) / 2;
        acceleration = evaluateSeries(acceleration, expecterInterval, stepperOpts, startedAt,
            (totalPulses + 1) / 2);
        seriesSequence.seq.push_back(acceleration);
        const float peak = acceleration.finalSpeed(stepperOpts);
        StepperMotorSeries deceleration = getFastestSeries(peak, baseSpeedDegPerSec, stepperOpts, intervalAlgorithm);
        deceleration.minimumPulsesCount_ = totalPulses > acceleration.pulsesCount_
            ? totalPulses - acceleration.pulsesCount_ : 0;
        seriesSequence.seq.push_back(evaluateSeries(deceleration, expecterInterval, stepperOpts, acceleration.observedAt_));
        return true;
    }

    const float absRotationDeg   = static_cast<float>(totalPulses) * stepperOpts.degPulse;
    const float peakSpeed        = std::sqrt(baseSpeed * baseSpeed + stepperOpts.accelMaxDegSec2 * absRotationDeg); // кінематичне рівняння для totalRotationDeg/2
    const float peakSpeedAllowed = std::min(speedMax, peakSpeed);
    const float accelerationDeg  = std::max(0.0F,(peakSpeedAllowed * peakSpeedAllowed - baseSpeed * baseSpeed) / (2.0F * stepperOpts.accelMaxDegSec2));

    const uint64_t accelerationPulses    = std::min(static_cast<uint64_t>(std::floor(accelerationDeg / stepperOpts.degPulse)), totalPulses / 2);
    const uint64_t cruisePulses          = totalPulses - 2 * accelerationPulses;
    const float    actualPeakSpeed       = std::sqrt(baseSpeed * baseSpeed + 2.0F * stepperOpts.accelMaxDegSec2  * static_cast<float>(accelerationPulses) * stepperOpts.degPulse);
    const float    cruiseSpeedDegPerSec  = (targetRotationDeg >= 0) ? actualPeakSpeed : -actualPeakSpeed;

    const size_t firstNewSeries = seriesSequence.seq.size();
    if (accelerationPulses) {
        seriesSequence.seq.push_back(StepperMotorSeries(accelerationPulses, baseSpeedDegPerSec,cruiseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }
    if (cruisePulses) {
        seriesSequence.seq.push_back(StepperMotorSeries(cruisePulses, cruiseSpeedDegPerSec,cruiseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }
    if (accelerationPulses) {
        seriesSequence.seq.push_back(StepperMotorSeries(accelerationPulses, cruiseSpeedDegPerSec,baseSpeedDegPerSec, directionForward, stepperOpts, intervalAlgorithm));
    }

    moment at = startedAt;
    for (size_t i = firstNewSeries; i < seriesSequence.seq.size(); ++i) {
        seriesSequence.seq[i] = evaluateSeries(seriesSequence.seq[i], expecterInterval, stepperOpts, at);
        at = seriesSequence.seq[i].observedAt_;
    }
    return true;
}

bool addAcceleratedSeries(StepperMotorSeriesSequence& seriesSequence, float baseSpeedDegPerSec,
        float targetRotationDeg, duration expecterInterval, const stepper_motor_options_t& stepperOpts,
        stepper_motor_algorithm_t intervalAlgorithm) {
    const bool forward = targetRotationDeg >= 0;
    const uint64_t pulses = stepperOpts.pulsesForDeg(targetRotationDeg, forward);
    if (baseSpeedDegPerSec != 0 || pulses == 0) {
        return calculateAcceleratedSeries(seriesSequence, baseSpeedDegPerSec, targetRotationDeg,
            expecterInterval, stepperOpts, intervalAlgorithm);
    }
    // Reserve the last requested pulse for a finite slow interval. Braking over
    // the preceding distance keeps its original acceleration limit.
    const float precedingDeg = (forward ? 1.0F : -1.0F) * static_cast<float>(pulses - 1) * stepperOpts.degPulse;
    if (!calculateAcceleratedSeries(seriesSequence, baseSpeedDegPerSec, precedingDeg,
            expecterInterval, stepperOpts, intervalAlgorithm)) { return false; }
    StepperMotorSeries terminal(1, 0, 0, forward, stepperOpts, CONSTANT_ACCELERATION);
    terminal.terminalInterval_ = 100 * MILLISECOND;
    const moment at = seriesSequence.seq.empty() ? 0 : seriesSequence.seq.back().observedAt_;
    seriesSequence.seq.push_back(evaluateSeries(terminal, expecterInterval, stepperOpts, at));
    return true;
}

const std::string ON_OPTIONS_IS_OK = "on optionsIsOk(): ";
bool optionsIsOk(const stepper_motor_options_t& stepperOpts, std::string& error) {
    if (!isFinitePositive(stepperOpts.freqMax)) {
        error = ON_OPTIONS_IS_OK + "freqMax must be finite and greater than zero";
        return false;
    }
    if (!isFinitePositive(stepperOpts.speedMaxDegSec)) {
        error = ON_OPTIONS_IS_OK + "freqAllowed must be finite positive";
        return false;
    }
    if (!isFinitePositive(stepperOpts.accelMaxDegSec2)) {
        error = ON_OPTIONS_IS_OK + "accelMax must be finite and greater than zero";
        return false;
    }
    if (!isFinitePositive(stepperOpts.degPulse)) {
        error = ON_OPTIONS_IS_OK + "degreesPerPulse must be finite and greater than zero";
        return false;
    }

    return true;
}

const std::string ON_GET_SERIES_SEQUENCE = "on getSeriesSequence(): ";
static StepperMotorSeriesSequence calculateSeriesSequence(
        float currentSpeedDegPerSec, float totalRotationDeg, float targetSpeedDegPerSec,
        duration expecterInterval, const stepper_motor_options_t& stepperOpts, stepper_motor_algorithm_t intervalAlgorithm) {

    StepperMotorSeriesSequence result;

    // it should be checked at system initialization
    // if (!optionsIsOk(stepperOpts, result.error)) { return result; }

    if (!std::isfinite(currentSpeedDegPerSec) || !std::isfinite(totalRotationDeg) || !std::isfinite(targetSpeedDegPerSec)) {
        result.error = ON_GET_SERIES_SEQUENCE + "speed and position values must be finite";
        return result;
    }

    const float speedMax = std::min(stepperOpts.speedMaxDegSec,stepperOpts.freqMax * stepperOpts.degPulse);
    if (std::abs(targetSpeedDegPerSec) > speedMax) {
        result.error = ON_GET_SERIES_SEQUENCE + "targetSpeed must not exceed freqAllowed * degreesPerPulse";
        return result;
    }

    // sad if so, but we can't change the actual state of the system
    // if (std::abs(currentSpeed) > speedMax)

    const bool targetDirectionForward = totalRotationDeg > 0.0;
    if (currentSpeedDegPerSec * (targetDirectionForward ? 1. : -1.) < -EPS) {
        result.error = ON_GET_SERIES_SEQUENCE + "targetChangeDeg is opposite to currentSpeed; braking/reversal is not supported";
        return result;
    }

    // printf("\ntotalRotationDeg: %f --> targetDirectionForward: %d\n\n", totalRotationDeg, targetDirectionForward);

    if (targetSpeedDegPerSec * currentSpeedDegPerSec < -EPS) {
        result.error = ON_GET_SERIES_SEQUENCE + "targetSpeed is opposite to currentSpeed";
        return result;
    }

    POINT(1,0)

    if (std::abs(targetSpeedDegPerSec - currentSpeedDegPerSec) <= std::max(SPEED_EPS, std::abs(targetSpeedDegPerSec) * RESULT_EPS_RATIO)) {
        if (!addAcceleratedSeries(result, currentSpeedDegPerSec, totalRotationDeg, expecterInterval, stepperOpts, intervalAlgorithm)) {
            result.error = ON_GET_SERIES_SEQUENCE + "accelerationSequence for fixed speed isn't calculated correctly";
            // ??? and what
        }
        return result;
    }

    POINT(2,0)

    StepperMotorSeries fastestSeries = getFastestSeries(currentSpeedDegPerSec, targetSpeedDegPerSec, stepperOpts, intervalAlgorithm);

    POINT(3,0)

    fastestSeries = evaluateSeries(fastestSeries, expecterInterval, stepperOpts);
    float changeDegMin = fastestSeries.totalRotationDeg(stepperOpts);
    if (std::abs(changeDegMin - totalRotationDeg) < stepperOpts.degPulse) {

        POINT(3,1)

        result.seq.push_back(fastestSeries);
        return result;
    } else if (std::abs(changeDegMin) > std::abs(totalRotationDeg)) {

        POINT(3,2)

        if (expecterInterval) {
            fastestSeries = evaluateSeries(fastestSeries, expecterInterval, stepperOpts, 0,
                stepperOpts.pulsesForDeg(totalRotationDeg, fastestSeries.directionForward_));
        } else {
            fastestSeries.limitWithDeg(totalRotationDeg, stepperOpts);
            fastestSeries = evaluateSeries(fastestSeries, 0, stepperOpts);
        }
        float changeDeg = fastestSeries.totalRotationDeg(stepperOpts);
        result.error = ON_GET_SERIES_SEQUENCE + ((std::abs(changeDeg - totalRotationDeg) < stepperOpts.degPulse)
                     ? "fastest series is cutted to targetChangeDeg" : "fastest series isn't cutted to targetChangeDeg correctly");
        result.seq.push_back(fastestSeries);
        return result;
    } else if (std::abs(targetSpeedDegPerSec) > std::abs(currentSpeedDegPerSec)) {

        POINT(3,3)

        result.seq.push_back(fastestSeries);
        const float remainingChangeDeg = totalRotationDeg - fastestSeries.totalRotationDeg(stepperOpts);
        if (!addAcceleratedSeries(result, expecterInterval ? fastestSeries.finalSpeed(stepperOpts) : fastestSeries.idealFinalSpeed(stepperOpts), remainingChangeDeg, expecterInterval, stepperOpts, intervalAlgorithm)) {
            result.error = ON_GET_SERIES_SEQUENCE + "accelerationSequence for target speed isn't calculated correctly";
            // ??? and what
        }
        return result;
    } else {

        POINT(3,4)

        const float remainingChangeDeg = totalRotationDeg - fastestSeries.totalRotationDeg(stepperOpts);
        if (!addAcceleratedSeries(result, currentSpeedDegPerSec, remainingChangeDeg, expecterInterval, stepperOpts, intervalAlgorithm)) {
            result.error = ON_GET_SERIES_SEQUENCE + "accelerationSequence for current speed isn't calculated correctly";
        }
        const moment at = result.seq.empty() ? 0 : result.seq.back().observedAt_;
        result.seq.push_back(evaluateSeries(fastestSeries, expecterInterval, stepperOpts, at));
    }

    return result;
}

StepperMotorSeriesSequence getSeriesSequence(
        float currentSpeedDegPerSec, float totalRotationDeg, float targetSpeedDegPerSec,
        duration expecterInterval, const stepper_motor_options_t& stepperOpts,
        stepper_motor_algorithm_t intervalAlgorithm) {
    auto result = calculateSeriesSequence(currentSpeedDegPerSec, totalRotationDeg, targetSpeedDegPerSec,
        expecterInterval, stepperOpts, intervalAlgorithm);
    if (!result.error.empty() || result.seq.empty()) { return result; }
    const uint64_t targetPulses = stepperOpts.pulsesForDeg(totalRotationDeg, totalRotationDeg >= 0);
    uint64_t actualPulses = 0;
    for (const auto& series : result.seq) { actualPulses += series.pulsesCount_; }
    if (actualPulses < targetPulses) {
        auto& last = result.seq.back();
        last.minimumPulsesCount_ = last.pulsesCount_ + targetPulses - actualPulses;
        last = evaluateSeries(last, expecterInterval, stepperOpts, last.startedAt_);
    }
    return result;
}

namespace {
struct SeriesStatistics {
    double time_ = 0;
    double rotation_ = 0;
    uint64_t pulses_ = 0;
    uint64_t expected_ = 0;
    float finalSpeed_ = 0;
    float maxSpeed_ = 0;
    float maxAcceleration_ = 0;

    void add(const StepperMotorSeries& series, const stepper_motor_options_t& options, float acceleration) {
        time_ += series.totalSec();
        rotation_ += series.totalRotationDeg(options);
        pulses_ += series.pulsesCount_;
        expected_ += series.expectedPulsesCount_;
        if (series.pulsesCount_) { finalSpeed_ = series.finalSpeed(options); }
        maxSpeed_ = std::max(maxSpeed_, series.maxSpeedDegSec_);
        maxAcceleration_ = std::max(maxAcceleration_, acceleration);
    }

    void log(const std::string& label) const {
        printf("%s: time=%.9f s, rotation=%.6f deg, pulses=%lu, expected=%lu, finalSpeed=%.6f deg/s, maxSpeed=%.6f deg/s, maxAcceleration=%.6f deg/s^2\n",
            label.c_str(), time_, rotation_, pulses_, expected_, finalSpeed_, maxSpeed_, maxAcceleration_);
    }
};
}

void StepperMotorSeriesSequence::log(const stepper_motor_options_t& stepperOpts, const char* label, bool verbose) const {
    SeriesStatistics total;
    SeriesStatistics block;
    std::string blockPhase;
    duration previousInterval = 0;
    float previousSpeed = 0;
    for (const auto& series : seq) {
        const std::string phase = series.terminalInterval_ || series.accelerationDegPerSec2_ < 0 ? "Deceleration"
            : series.accelerationDegPerSec2_ > 0 ? "Acceleration" : "Cruise";
        if (phase != blockPhase) {
            if (!blockPhase.empty()) { block.log(std::string(label) + ": " + blockPhase + " block total"); }
            block = {};
            blockPhase = phase;
            printf("\n%s: %s\n", label, phase.c_str());
        }
        if (verbose) { series.log(stepperOpts, label); }
        float acceleration = series.maxAccelerationDegSec2_;
        if (previousInterval && series.pulsesCount_) {
            const duration firstInterval = series.firstPulseAt_ - series.startedAt_;
            const float firstSpeed = (series.directionForward_ ? 1.0F : -1.0F) * stepperOpts.degPulse * SECOND / firstInterval;
            const double dt = (static_cast<double>(previousInterval) + firstInterval) / (2.0 * SECOND);
            acceleration = std::max(acceleration, static_cast<float>(std::abs(firstSpeed - previousSpeed) / dt));
        }
        total.add(series, stepperOpts, acceleration);
        block.add(series, stepperOpts, acceleration);
        if (series.pulsesCount_) {
            previousInterval = series.lastInterval_;
            previousSpeed = series.finalSpeed(stepperOpts);
        }
    }
    if (!blockPhase.empty()) { block.log(std::string(label) + ": " + blockPhase + " block total"); }
    total.log("\n" + std::string(label) + " TOTAL");
}
