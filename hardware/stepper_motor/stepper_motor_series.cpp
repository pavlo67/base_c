#include "stepper_motor.h"
#include "lib/mathlib.h"

StepperMotorSeries::StepperMotorSeries(
    uint64_t pulsesCount, float firstSpeedDegPerSec, float lastSpeedDegPerSec, bool directionForward,
    const stepper_motor_options_t& stepperOpts, stepper_motor_algorithm_t intervalAlgorithm
) {

    // printf("pulseCount: %d, firstSpeedDegPerSec: %f, lastSpeedDegPerSec: %f, directionForward: %d\n", pulseCount, firstSpeedDegPerSec, lastSpeedDegPerSec, directionForward);

    expectedPulsesCount_   = pulsesCount;
    pulsesCount_           = 0;
    intervalAlgorithm_     = intervalAlgorithm;
    initialSpeedDegPerSec_ = firstSpeedDegPerSec;
    directionForward_      = directionForward;

    if (expectedPulsesCount_ == 0) {
        return;
    } else if ((directionForward && (firstSpeedDegPerSec < 0 || lastSpeedDegPerSec < 0))
           || (!directionForward && (firstSpeedDegPerSec > 0 || lastSpeedDegPerSec > 0))) {
        expectedPulsesCount_ = 0;
        return;
    }

    const float distanceDeg = static_cast<float>(pulsesCount) * stepperOpts.degPulse;
    accelerationDegPerSec2_ = distanceDeg > 0.0F
                            ? (lastSpeedDegPerSec * lastSpeedDegPerSec - initialSpeedDegPerSec_ * initialSpeedDegPerSec_) / (2.0F * distanceDeg)
                            : 0.0F;

    const float fallbackInitialSpeed = std::max(std::abs(initialSpeedDegPerSec_), stepperOpts.speedAfterOnePulse());
    const float lastIntervalSec = stepperOpts.pulseInterval(lastSpeedDegPerSec);

    initialIntervalSec_ = stepperOpts.pulseInterval(fallbackInitialSpeed);
    intervalChangePerPulse_ = pulsesCount > 1 ? (lastIntervalSec - initialIntervalSec_) / static_cast<float>(pulsesCount - 1) : 0.0F;
}

float StepperMotorSeries::expectedRotationDeg(const stepper_motor_options_t& stepperOpts) const {
    const float direction = directionForward_ ? 1.0F : -1.0F;
    return direction * static_cast<float>(expectedPulsesCount_) * stepperOpts.degPulse;
}

float StepperMotorSeries::idealIntervalSec(uint64_t pulseIndex, const stepper_motor_options_t& stepperOpts) const {
    if (pulseIndex >= expectedPulsesCount_) {
        return 0.0F;
    }

    if (!brakingModel_.empty()) {
        const auto next = std::upper_bound(brakingModel_.begin(), brakingModel_.end(), pulseIndex,
            [](uint64_t pulses, const StepperMotorBrakingPoint& point) { return pulses < point.pulses_; });
        return static_cast<double>((next == brakingModel_.begin() ? next : next - 1)->interval_) / SECOND;
    }

    if (intervalAlgorithm_ == LINEAR_INTERVAL_ACCELERATION) {
        const float interval = initialIntervalSec_  + intervalChangePerPulse_ * static_cast<float>(pulseIndex);
        return isFinitePositive(interval) ? interval : 0.0F;
    }

    if (expectedPulsesCount_ == 1 && initialSpeedDegPerSec_ == 0 && accelerationDegPerSec2_ == 0) {
        const float peak = std::min(std::sqrt(stepperOpts.accelMaxDegSec2 * stepperOpts.degPulse),
            std::min(stepperOpts.speedMaxDegSec, stepperOpts.freqMax * stepperOpts.degPulse));
        return std::max(stepperOpts.degPulse / peak + peak / stepperOpts.accelMaxDegSec2,
            static_cast<float>(terminalInterval_) / SECOND);
    }

    const float distanceBefore = static_cast<float>(pulseIndex) * stepperOpts.degPulse;
    const double speedBeforeSquared = static_cast<double>(initialSpeedDegPerSec_) * initialSpeedDegPerSec_
        + 2.0 * accelerationDegPerSec2_ * distanceBefore;
    // const float speedAfterSquared  = initialSpeedDegPerSec_ * initialSpeedDegPerSec_ + 2.0F * accelerationDegPerSec2_ * (distanceBefore + stepperOpts.degreesPerPulse);
    const double speedAfterSquared = speedBeforeSquared + 2.0 * accelerationDegPerSec2_ * stepperOpts.degPulse;

    const double tolerance = std::max(1.0, static_cast<double>(initialSpeedDegPerSec_) * initialSpeedDegPerSec_) * 1e-6;
    if (speedBeforeSquared < -tolerance || speedAfterSquared < -tolerance) {
        return 0.0F;
    }

    const float speedBefore = std::sqrt(std::max(0.0, speedBeforeSquared));
    const float speedAfter  = std::sqrt(std::max(0.0, speedAfterSquared));
    const float speedSum    = speedBefore + speedAfter;
    return speedSum > EPS ? 2.0F * stepperOpts.degPulse / speedSum : 0.0F;
}

float StepperMotorSeries::scheduledIntervalSec(uint64_t pulseIndex, const stepper_motor_options_t& stepperOpts) {
    frequencyLimited_ = false;
    if (!brakingModel_.empty()) {
        return idealIntervalSec(pulseIndex, stepperOpts);
    }
    double interval = idealIntervalSec(pulseIndex, stepperOpts);
    if (!(interval > 0) || !std::isfinite(interval)) { return 0; }
    if (terminalInterval_) {
        interval = std::max(interval, static_cast<double>(terminalInterval_) / SECOND);
    }
    if (livePwm_) {
        // Match the integer-Hz command range, rounding down to respect speed limits.
        const double maxFrequency = std::min({10000.0, static_cast<double>(stepperOpts.freqMax),
            static_cast<double>(stepperOpts.speedMaxDegSec) / stepperOpts.degPulse});
        const double requested = cruiseFrequency_ && accelerationDegPerSec2_ == 0
            ? static_cast<double>(cruiseFrequency_) : 1.0 / interval;
        double frequency = std::floor(std::min(requested, maxFrequency) + 1e-6);
        if (frequency < 1) { return 0; }
        const double requestedFrequency = frequency;
        const double previousFrequency = activeInterval_ ? static_cast<double>(SECOND) / activeInterval_
            : std::abs(initialSpeedDegPerSec_) / stepperOpts.degPulse;
        if (previousFrequency >= 1 && previousFrequency <= maxFrequency) {
            const double previousCommand = std::round(previousFrequency);
            while (frequency != previousCommand) {
                const double acceleration = 2.0 * stepperOpts.degPulse * std::abs(frequency - previousFrequency)
                    / (1.0 / frequency + 1.0 / previousFrequency);
                if (acceleration <= stepperOpts.accelMaxDegSec2) { break; }
                frequency += frequency < previousCommand ? 1 : -1;
            }
        }
        frequencyLimited_ = frequency != requestedFrequency;
        if (frequencyLimited_ && frequency == std::round(previousFrequency)) { return 0; }
        interval = 1.0 / frequency;
    }
    return static_cast<float>(interval);
}

float StepperMotorSeries::idealTotalSec(const stepper_motor_options_t& stepperOpts) const {
    float t = 0;
    for (uint64_t pulseIndex = 0; pulseIndex < expectedPulsesCount_; ++pulseIndex) {
        t += idealIntervalSec(pulseIndex, stepperOpts);
    }
    return t;
}

float StepperMotorSeries::idealFinalSpeed(const stepper_motor_options_t& stepperOpts) const {
    if (!brakingModel_.empty()) { return brakingFinalSpeed_; }
    if (expectedPulsesCount_ == 0) {
        return 0.0F;
    }

    float speed = 0.0F;
    if (intervalAlgorithm_ == LINEAR_INTERVAL_ACCELERATION) {
        const float lastInterval = idealIntervalSec(expectedPulsesCount_ - 1, stepperOpts);
        if (!isFinitePositive(lastInterval)) {
            return 0.0F;
        }
        speed = stepperOpts.degPulse / lastInterval;
    } else {
        const float distanceDeg = static_cast<float>(expectedPulsesCount_) * stepperOpts.degPulse;
        const float speedSquared = initialSpeedDegPerSec_ * initialSpeedDegPerSec_ + 2.0F * accelerationDegPerSec2_ * distanceDeg;
        if (speedSquared < -EPS) {
            return 0.0F;
        }
        speed = std::sqrt(std::max(0.0F, speedSquared));
    }

    return directionForward_ ? speed : -speed;
}

void StepperMotorSeries::limitWithDeg(float targetDeg, const stepper_motor_options_t& stepperOpts) {
    reset();
    minimumPulsesCount_ = 0;
    expectedPulsesCount_ = stepperOpts.pulsesForDeg(targetDeg, directionForward_);
    if (expectedPulsesCount_ == 0) {
        accelerationDegPerSec2_ = 0;
    }
}

void StepperMotorSeries::reset() {
    pulsesCount_ = 0;
    firstPulseAt_ = lastPulseAt_ = startedAt_ = observedAt_ = nextPulseAt_ = recalculateAfter_ = 0;
    activeInterval_ = pendingInterval_ = lastInterval_ = 0;
    initialPulseInterval_ = setupDelay_ = 0;
    intervalIndex_ = 0;
    started_ = finished_ = repeatLastInterval_ = frequencyLimited_ = false;
    onPulse_ = {};
    maxSpeedDegSec_ = maxAccelerationDegSec2_ = 0;
}

float StepperMotorSeries::totalRotationDeg(const stepper_motor_options_t& stepperOpts) const {
    return (directionForward_ ? 1.0F : -1.0F) * pulsesCount_ * stepperOpts.degPulse;
}

float StepperMotorSeries::totalSec() const {
    return started_ ? (static_cast<double>(observedAt_ - startedAt_) + setupDelay_) / SECOND : 0.0F;
}

float StepperMotorSeries::totalSec(const stepper_motor_options_t&) const {
    return totalSec();
}

float StepperMotorSeries::finalSpeed(const stepper_motor_options_t& stepperOpts) const {
    return lastInterval_ ? (directionForward_ ? 1.0F : -1.0F) *
        stepperOpts.degPulse * SECOND / lastInterval_ : 0.0F;
}

float StepperMotorSeries::intervalSec(moment at, const stepper_motor_options_t& stepperOpts) {
    if (finished_ || (started_ && at < observedAt_)) {
        return finished_ ? 0.0F : static_cast<double>(pendingInterval_ ? pendingInterval_ : activeInterval_) / SECOND;
    }
    if (!started_) {
        started_ = true;
        startedAt_ = observedAt_ = at;
        const float interval = scheduledIntervalSec(0, stepperOpts);
        if (!isFinitePositive(interval)) {
            finished_ = true;
            return 0.0F;
        }
        activeInterval_ = livePwm_ ? static_cast<duration>(std::llround(static_cast<double>(SECOND) / std::llround(1.0 / interval)))
            : std::max<duration>(1, std::llround(static_cast<double>(interval) * SECOND));
        nextPulseAt_ = recalculateAfter_ = at + activeInterval_;
        return static_cast<double>(activeInterval_) / SECOND;
    }

    // A pending PWM period is latched only at the end of the current period.
    while (nextPulseAt_ <= at) {
        const float speed = stepperOpts.degPulse * SECOND / activeInterval_;
        const duration previousInterval = lastInterval_ ? lastInterval_ : initialPulseInterval_;
        const float previousSpeed = previousInterval ? stepperOpts.degPulse * SECOND / previousInterval
            : std::abs(initialSpeedDegPerSec_);
        const double separationSec = previousInterval ?
            (static_cast<double>(previousInterval) + activeInterval_) / (2.0 * SECOND) :
            static_cast<double>(activeInterval_) / (2.0 * SECOND);
        maxSpeedDegSec_ = std::max(maxSpeedDegSec_, speed);
        maxAccelerationDegSec2_ = std::max(maxAccelerationDegSec2_,
            static_cast<float>(std::abs(speed - previousSpeed) / separationSec));
        if (pulsesCount_ == 0) { firstPulseAt_ = nextPulseAt_; }
        if (onPulse_) { onPulse_(nextPulseAt_); }
        ++pulsesCount_;
        lastPulseAt_ = nextPulseAt_;
        lastInterval_ = activeInterval_;
        if (pendingInterval_) {
            activeInterval_ = pendingInterval_;
            pendingInterval_ = 0;
        }
        nextPulseAt_ += activeInterval_;
        if (!livePwm_ && minimumPulsesCount_ && intervalIndex_ >= expectedPulsesCount_ && pulsesCount_ >= minimumPulsesCount_) {
            // The remaining integer pulse count is known before this tail starts.
            finished_ = true;
            observedAt_ = lastPulseAt_;
            return 0.0F;
        }
    }
    observedAt_ = at;
    if (at < recalculateAfter_ && brakingModel_.empty()) {
        return static_cast<double>(pendingInterval_ ? pendingInterval_ : activeInterval_) / SECOND;
    }
    if (repeatLastInterval_ && minimumPulsesCount_ == 0 && intervalIndex_ + 1 >= expectedPulsesCount_) {
        return static_cast<double>(activeInterval_) / SECOND;
    }
    if (intervalIndex_ >= expectedPulsesCount_ && pulsesCount_ < minimumPulsesCount_) {
        return static_cast<double>(activeInterval_) / SECOND;
    }
    if (!brakingModel_.empty()) { intervalIndex_ = pulsesCount_; }
    else if (!frequencyLimited_) { ++intervalIndex_; }
    const float interval = scheduledIntervalSec(intervalIndex_, stepperOpts);
    if (!isFinitePositive(interval)) {
        if (intervalIndex_ >= expectedPulsesCount_ && pulsesCount_ < minimumPulsesCount_) {
            return static_cast<double>(activeInterval_) / SECOND;
        }
        finished_ = true;
        return 0.0F;
    }
    const duration period = livePwm_ ? static_cast<duration>(std::llround(static_cast<double>(SECOND) / std::llround(1.0 / interval)))
        : std::max<duration>(1, std::llround(static_cast<double>(interval) * SECOND));
    if (livePwm_) {
        // Estimate elapsed pulses using the old command up to this actual update.
        // Backend phase/quantization is not observable through the GPIO API.
        if (period != activeInterval_) { nextPulseAt_ = at + period; }
        activeInterval_ = period;
        pendingInterval_ = 0;
        recalculateAfter_ = at + period;
    } else if (lastPulseAt_ == at) {
        activeInterval_ = period;
        nextPulseAt_ = recalculateAfter_ = at + period;
    } else {
        pendingInterval_ = period;
        recalculateAfter_ = nextPulseAt_ + period;
    }
    return static_cast<double>(period) / SECOND;
}

StepperMotorSeries evaluateSeries(StepperMotorSeries series, duration expecterInterval,
        const stepper_motor_options_t& stepperOpts, moment startedAt, uint64_t stopAfterPulses,
        const std::function<void(moment)>& onPulse) {
    series.reset();
    series.repeatLastInterval_ = stopAfterPulses != 0;
    series.onPulse_ = onPulse;
    float interval = series.intervalSec(startedAt, stepperOpts);
    moment at = startedAt;
    while (interval > 0.0F) {
        at = expecterInterval ? at + (expecterInterval - at % expecterInterval) : series.nextPulseAt_;
        interval = series.intervalSec(at, stepperOpts);
        if (stopAfterPulses && series.pulsesCount_ >= stopAfterPulses) { break; }
    }
    series.onPulse_ = {};
    return series;
}
