#include <chrono>
#include <cstdio>
#include <iostream>
#include <thread>

#include "hardware/hardware.h"
#include "hardware/stepper_motor/stepper_motor.h"

const stepper_motor_options_t STEPPER_OPTS {
    .freqMax         = FREQ_MAX_DEFAULT,
    .degPulse        = DEG_PULSE_DEFAULT,
    .speedMaxDegSec  = SPEED_MAX_DEG_SEC,
    .accelMaxDegSec2 = ACCEL_MAX_DEG_SEC2
};
const duration STEPPER_INTERVAL = 5 * MILLISECOND;
const float ROTATIONS[] = {90.0F}; // , -180.0F, 90.0F


int main() {
    if (Gpio::instance().initialize() < 0) {
        std::cout << "ERROR: GPIO initialization failed\n";
        return 1;
    }
    int result = 0;
    for (const float rotation : ROTATIONS) {
        const auto clocked = getSeriesSequence(0.0F, rotation, 0.0F, STEPPER_INTERVAL, STEPPER_OPTS);
        const auto ideal = getSeriesSequence(0.0F, rotation, 0.0F, 0, STEPPER_OPTS);
        if (!clocked.error.empty() || !ideal.error.empty()) {
            std::cout << "ERROR: " << clocked.error << " " << ideal.error << "\n";
            result = 1;
            break;
        }
        printf("\nTarget: %.3f deg; timer: %.3f ms; simulated PWM statistics\n",
            rotation, static_cast<double>(STEPPER_INTERVAL) / MILLISECOND);
        clocked.log(STEPPER_OPTS, "clocked");
        ideal.log(STEPPER_OPTS, "ideal");
        if (move(clocked, PIN_STEP, PIN_DIR, PIN_ENA, STEPPER_INTERVAL,
                STEPPER_OPTS, PULSE_HIGH_US_MIN * MICROSECOND) < 0) {
            result = 1;
            break;
        }
    }
    Gpio::instance().terminate();
    return result;
}

