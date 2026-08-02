#include "stepper_motor.h"
#include "lib/mathlib.h"

StepperSeries::StepperSeries(
    uint64_t pulseCount, float firstSpeedDegPerSec, float lastSpeedDegPerSec, bool directionForward,
    const stepper_options_t& stepperOpts, interval_algorithm_t intervalAlgorithm
) {

    pulseCount_            = pulseCount;
    intervalAlgorithm_     = intervalAlgorithm;
    initialSpeedDegPerSec_ = firstSpeedDegPerSec;
    directionForward_      = directionForward;

    if (pulseCount_ == 0) {
        return;
    } else if ((directionForward && (firstSpeedDegPerSec < 0 || lastSpeedDegPerSec < 0))
           || (!directionForward && (firstSpeedDegPerSec > 0 || lastSpeedDegPerSec > 0))) {
        pulseCount_ = 0;
        return;
    }

    const float distanceDeg = static_cast<float>(pulseCount) * stepperOpts.degPulse;
    accelerationDegPerSec2_ = distanceDeg > 0.0F
                            ? (lastSpeedDegPerSec * lastSpeedDegPerSec - initialSpeedDegPerSec_ * initialSpeedDegPerSec_) / (2.0F * distanceDeg)
                            : 0.0F;

    const float fallbackInitialSpeed = std::max(std::abs(initialSpeedDegPerSec_), stepperOpts.speedAfterOnePulse());
    const float lastIntervalSec = stepperOpts.pulseInterval(lastSpeedDegPerSec);

    initialIntervalSec_ = stepperOpts.pulseInterval(fallbackInitialSpeed);
    intervalChangePerPulse_ = pulseCount > 1 ? (lastIntervalSec - initialIntervalSec_) / static_cast<float>(pulseCount - 1) : 0.0F;
}

float StepperSeries::totalRotationDeg(const stepper_options_t& stepperOpts) const {
    const float direction = directionForward_ ? 1.0F : -1.0F;
    return direction * static_cast<float>(pulseCount_) * stepperOpts.degPulse;
}

float StepperSeries::intervalSec(uint64_t pulseIndex, const stepper_options_t& stepperOpts) const {
    if (pulseIndex >= pulseCount_) {
        return 0.0F;
    }

    if (intervalAlgorithm_ == LINEAR_INTERVAL_ACCELERATION) {
        const float interval = initialIntervalSec_  + intervalChangePerPulse_ * static_cast<float>(pulseIndex);
        return isFinitePositive(interval) ? interval : 0.0F;
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

float StepperSeries::totalSec(const stepper_options_t& stepperOpts) const {
    float t = 0;
    for (uint64_t pulseIndex = 0; pulseIndex < pulseCount_; ++pulseIndex) {
        t += intervalSec(pulseIndex, stepperOpts);
    }
    return t;
}


float StepperSeries::finalSpeed(const stepper_options_t& stepperOpts) const {
    if (pulseCount_ == 0) {
        return 0.0F;
    }

    float speed = 0.0F;
    if (intervalAlgorithm_ == LINEAR_INTERVAL_ACCELERATION) {
        const float lastInterval = intervalSec(pulseCount_ - 1, stepperOpts);
        if (!isFinitePositive(lastInterval)) {
            return 0.0F;
        }
        speed = stepperOpts.degPulse / lastInterval;
    } else {
        const float distanceDeg = static_cast<float>(pulseCount_) * stepperOpts.degPulse;
        const float speedSquared = initialSpeedDegPerSec_ * initialSpeedDegPerSec_ + 2.0F * accelerationDegPerSec2_ * distanceDeg;
        if (speedSquared < -EPS) {
            return 0.0F;
        }
        speed = std::sqrt(std::max(0.0F, speedSquared));
    }

    return directionForward_ ? speed : -speed;
}

void StepperSeries::limitWithDeg(float targetDeg, const stepper_options_t& stepperOpts) {
    pulseCount_ = stepperOpts.pulsesForDeg(targetDeg, directionForward_);
    if (pulseCount_ == 0) {
        accelerationDegPerSec2_ = 0;
    }
}
