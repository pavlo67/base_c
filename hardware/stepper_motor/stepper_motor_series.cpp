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

    if (intervalAlgorithm_ == LINEAR_INTERVAL_ACCELERATION) {
        const float interval = initialIntervalSec_  + intervalChangePerPulse_ * static_cast<float>(pulseIndex);
        return isFinitePositive(interval) ? interval : 0.0F;
    }

    if (expectedPulsesCount_ == 1 && initialSpeedDegPerSec_ == 0 && accelerationDegPerSec2_ == 0) {
        const float peak = std::min(std::sqrt(stepperOpts.accelMaxDegSec2 * stepperOpts.degPulse),
            std::min(stepperOpts.speedMaxDegSec, stepperOpts.freqMax * stepperOpts.degPulse));
        return stepperOpts.degPulse / peak + peak / stepperOpts.accelMaxDegSec2;
    }

    const float distanceBefore = static_cast<float>(pulseIndex) * stepperOpts.degPulse;
    const float speedBeforeSquared = initialSpeedDegPerSec_ * initialSpeedDegPerSec_ + 2.0F * accelerationDegPerSec2_ * distanceBefore;
    // const float speedAfterSquared  = initialSpeedDegPerSec_ * initialSpeedDegPerSec_ + 2.0F * accelerationDegPerSec2_ * (distanceBefore + stepperOpts.degreesPerPulse);
    const float speedAfterSquared  = speedBeforeSquared + 2.0F * accelerationDegPerSec2_ * stepperOpts.degPulse;

    if (speedBeforeSquared < -EPS || speedAfterSquared < -EPS) {
        return 0.0F;
    }

    const float speedBefore = std::sqrt(std::max(0.0F, speedBeforeSquared));
    const float speedAfter  = std::sqrt(std::max(0.0F, speedAfterSquared));
    const float speedSum    = speedBefore + speedAfter;
    return speedSum > EPS ? 2.0F * stepperOpts.degPulse / speedSum : 0.0F;
}

float StepperMotorSeries::idealTotalSec(const stepper_motor_options_t& stepperOpts) const {
    float t = 0;
    for (uint64_t pulseIndex = 0; pulseIndex < expectedPulsesCount_; ++pulseIndex) {
        t += idealIntervalSec(pulseIndex, stepperOpts);
    }
    return t;
}

float StepperMotorSeries::idealFinalSpeed(const stepper_motor_options_t& stepperOpts) const {
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
    intervalIndex_ = 0;
    started_ = finished_ = repeatLastInterval_ = false;
    onPulse_ = {};
    maxSpeedDegSec_ = maxAccelerationDegSec2_ = 0;
}

float StepperMotorSeries::totalRotationDeg(const stepper_motor_options_t& stepperOpts) const {
    return (directionForward_ ? 1.0F : -1.0F) * pulsesCount_ * stepperOpts.degPulse;
}

float StepperMotorSeries::totalSec() const {
    return started_ ? static_cast<double>(observedAt_ - startedAt_) / SECOND : 0.0F;
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
        const float interval = idealIntervalSec(0, stepperOpts);
        if (!isFinitePositive(interval)) {
            finished_ = true;
            return 0.0F;
        }
        activeInterval_ = std::max<duration>(1, std::llround(static_cast<double>(interval) * SECOND));
        nextPulseAt_ = recalculateAfter_ = at + activeInterval_;
        return static_cast<double>(activeInterval_) / SECOND;
    }

    // A pending PWM period is latched only at the end of the current period.
    while (nextPulseAt_ <= at) {
        const float speed = stepperOpts.degPulse * SECOND / activeInterval_;
        const float previousSpeed = lastInterval_ ? std::abs(finalSpeed(stepperOpts)) : std::abs(initialSpeedDegPerSec_);
        const double separationSec = lastInterval_ ?
            (static_cast<double>(lastInterval_) + activeInterval_) / (2.0 * SECOND) :
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
        if (minimumPulsesCount_ && intervalIndex_ >= expectedPulsesCount_ && pulsesCount_ >= minimumPulsesCount_) {
            // The remaining integer pulse count is known before this tail starts.
            finished_ = true;
            observedAt_ = lastPulseAt_;
            return 0.0F;
        }
    }
    observedAt_ = at;
    if (at < recalculateAfter_) {
        return static_cast<double>(pendingInterval_ ? pendingInterval_ : activeInterval_) / SECOND;
    }
    if (repeatLastInterval_ && minimumPulsesCount_ == 0 && intervalIndex_ + 1 >= expectedPulsesCount_) {
        return static_cast<double>(activeInterval_) / SECOND;
    }
    if (intervalIndex_ >= expectedPulsesCount_ && pulsesCount_ < minimumPulsesCount_) {
        return static_cast<double>(activeInterval_) / SECOND;
    }
    ++intervalIndex_;
    const float interval = idealIntervalSec(intervalIndex_, stepperOpts);
    if (!isFinitePositive(interval)) {
        if (intervalIndex_ >= expectedPulsesCount_ && pulsesCount_ < minimumPulsesCount_) {
            return static_cast<double>(activeInterval_) / SECOND;
        }
        finished_ = true;
        return 0.0F;
    }
    const duration period = std::max<duration>(1, std::llround(static_cast<double>(interval) * SECOND));
    if (lastPulseAt_ == at) {
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
