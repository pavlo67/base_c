#include <cstdio>

#include "_base_defines.h"
#include "hardware/hardware.h"

#ifndef STEPPER_MOTOR_PROBE_VERBOSE
#define STEPPER_MOTOR_PROBE_VERBOSE false
#endif
#ifndef STEPPER_MOTOR_PROBE_HARDWARE_PWM
#define STEPPER_MOTOR_PROBE_HARDWARE_PWM true
#endif
#include "hardware/stepper_motor/stepper_motor.h"

const stepper_motor_options_t STEPPER_OPTS {
    .freqMax         = FREQ_MAX_DEFAULT,
    .degPulse        = DEG_PULSE_DEFAULT,
    .speedMaxDegSec  = SPEED_MAX_DEG_SEC,
    .accelMaxDegSec2 = ACCEL_MAX_DEG_SEC2
};
const duration STEPPER_INTERVAL = 5 * MILLISECOND;
const duration RUN_TIME_LIMIT = 60 * SECOND;
// Hardware PWM needs a routed PWM pin; wire STEP to BCM18 when enabled.
const unsigned HARDWARE_PWM_PIN_STEP = 18;
const float ROTATIONS[] = {90.0F}; // , -180.0F, 90.0F


int main() {
    bool hardwarePwm = STEPPER_MOTOR_PROBE_HARDWARE_PWM;
#if defined(SYSTEM_IS_DESKTOP) && SYSTEM_IS_DESKTOP
    if (hardwarePwm) {
        printf("Warning: hardware PWM is unavailable on desktop; STEPPER_MOTOR_PROBE_HARDWARE_PWM=OFF\n");
        hardwarePwm = false;
    }
#endif
    const StepperMotorRunConfig config {
        .pinStep_ = hardwarePwm ? HARDWARE_PWM_PIN_STEP : static_cast<unsigned>(PIN_STEP),
        .pinDir_ = PIN_DIR,
        .pinEna_ = PIN_ENA,
        .options_ = STEPPER_OPTS,
        .expecterInterval_ = STEPPER_INTERVAL,
        .pulseHigh_ = PULSE_HIGH_US_MIN * MICROSECOND,
        .timeLimit_ = RUN_TIME_LIMIT,
        .hardwarePwm_ = hardwarePwm,
        .verbose_ = STEPPER_MOTOR_PROBE_VERBOSE
    };
    for (const float rotation : ROTATIONS) {
        if (run(config, rotation, "probe") < 0) { return 1; }
    }
    return 0;
}
