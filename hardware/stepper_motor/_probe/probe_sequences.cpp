#include <thread>
#include <iostream>
#include <unistd.h>

#include "lib/time.h"
#include "lib/mathlib.h"

#include "hardware/hardware.h"
#include "hardware/stepper_motor/stepper_motor.h"


const stepper_options_t STEPPER_OPTS {
    .freqMax         = FREQ_MAX_DEFAULT,
    .degPulse        = DEG_PULSE_DEFAULT,
    .speedMaxDegSec  = SPEED_MAX_DEG_SEC,
    .accelMaxDegSec2 = ACCEL_MAX_DEG_SEC2
};

const bool VERBOSE = true;

void pulse(float intervalSec) {
    gpioWrite(PIN_STEP, 1);
    usleep(PULSE_HIGH_US_MIN);
    gpioWrite(PIN_STEP, 0);
    usleep(int(intervalSec * 1e6));
}

void moveSeries(const StepperSeries& series, stepper_options_t stepperOpts, const std::string& verboseLabel) {
    if (VERBOSE) {
        series.log(stepperOpts, verboseLabel.c_str());
    }

    gpioWrite(PIN_DIR, series.directionForward_ ? 1 : 0);
    usleep(PULSE_HIGH_US_MIN);

    float speed         = series.initialSpeedDegPerSec_;
    float accelErrorMax = 0;
    float intervalPrev  = series.initialSpeedDegPerSec_ < EPS ? 0 :  STEPPER_OPTS.degPulse / series.initialSpeedDegPerSec_;

    for (uint64_t pulseIndex = 0; pulseIndex < series.pulseCount_; ++pulseIndex) {
        const float intervalSec  = series.intervalSec(pulseIndex, STEPPER_OPTS);

        pulse(intervalSec);

        const float nextSpeed = 2.0F * STEPPER_OPTS.degPulse / intervalSec - speed;
        const float accel     = std::abs(intervalSec - intervalPrev) <= EPS ? 0
                              : (nextSpeed * nextSpeed - speed * speed) / (2.0F * STEPPER_OPTS.degPulse);

        // printf("interval: %f sec, speed: %f, nextSpeed: %f, accel: %f, accelerationDegPerSec2_: %f, accelErrorMax: %f\n", interval, speed, nextSpeed, accel, series.accelerationDegPerSec2_, accelErrorMax);

        accelErrorMax = std::max(accelErrorMax,std::abs(accel - series.accelerationDegPerSec2_));
        speed         = nextSpeed;
        intervalPrev  = intervalSec;
    }
    if (VERBOSE && accelErrorMax > std::max(ACCELERATION_EPS, std::abs(series.accelerationDegPerSec2_) * RESULT_EPS_RATIO)) {
        printf("%s: accelErrorMax: %9.3f is greater then limit: %f\n",
            verboseLabel.c_str(), accelErrorMax, std::max(ACCELERATION_EPS, std::abs(series.accelerationDegPerSec2_) * RESULT_EPS_RATIO));
    }
}

int seriesI = 0;

void move(const stepper_sequence_t& sequence) {
    float calculatedTotalRotationDeg = 0.0F;
    for (const StepperSeries& series : sequence.seriesSequence) {
        calculatedTotalRotationDeg += series.totalRotationDeg(STEPPER_OPTS);
        moveSeries(series, STEPPER_OPTS, "series_" + std::to_string(seriesI++));
    }
}

int main() {
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio init failed\n";
        return 1;
    }

    gpioSetMode(PIN_STEP, PI_OUTPUT);
    gpioSetMode(PIN_DIR,  PI_OUTPUT);
    gpioSetMode(PIN_ENA,  PI_OUTPUT);

    gpioWrite(PIN_ENA, 0); // Для більшості DM542: ENA LOW = enabled
    usleep(500);

    move(calculateSequence(0.0F, 90,0.0F, STEPPER_OPTS));
    move(calculateSequence(0.0F, -180,0.0F, STEPPER_OPTS));
    move(calculateSequence(0.0F, 90,0.0F, STEPPER_OPTS));

    gpioWrite(PIN_ENA, 1);   // stop / disable
    gpioWrite(PIN_DIR, 0);

    gpioTerminate();
    return 0;
}