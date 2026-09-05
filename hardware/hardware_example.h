#ifndef BASE_CPP_HARDWARE_H
#define BASE_CPP_HARDWARE_H

#include "gpio/gpio.h"
#include <array>

// pon 25 is bad!!!

inline constexpr int PIN_STEP = 17; // GPIO17 -> PUL-
inline constexpr int PIN_DIR  = 24; // GPIO24 -> DIR-
inline constexpr int PIN_ENA  = 27; // GPIO27 -> ENA-

inline constexpr float FREQ_MAX_DEFAULT   = 8000;   // 5 rpm * 1600 microsteps per rotation
inline constexpr float DEG_PULSE_DEFAULT  = 0.225F;

inline constexpr float SPEED_MAX_DEG_SEC =  180.0F;
inline constexpr float ACCEL_MAX_DEG_SEC2 = 720.0F;

inline constexpr int PULSE_HIGH_US_MIN = 15;


// BCM GPIO numbers, NOT physical header positions. Programmer must select free pins.
// These pins are driven automatically by gpio_test and gpio_probe.
inline constexpr std::array<unsigned, 2> GPIO_TEST_PINS = {17, 24};
inline constexpr unsigned GPIO_PROBE_PWM_PIN = GPIO_TEST_PINS[0];
inline constexpr unsigned GPIO_PROBE_PWM_RANGE = 100;
inline constexpr unsigned GPIO_PROBE_PWM_DUTY = 25;
inline constexpr unsigned GPIO_PROBE_PWM_FREQUENCY = 1000;
inline constexpr unsigned GPIO_PROBE_PWM_DURATION_MS = 1000;


#endif //BASE_CPP_HARDWARE_H

