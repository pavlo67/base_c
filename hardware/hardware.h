#ifndef BASE_CPP_HARDWARE_H
#define BASE_CPP_HARDWARE_H

#if SYSTEM_IS_RPI4
    #include <pigpio.h>

#else
    #define PI_INPUT  0
    #define PI_OUTPUT 1

    inline int  gpioWrite  (unsigned pin, unsigned value) { return 0; }
    inline int  gpioSetMode(unsigned pin, unsigned mode)  { return 0; }
    inline int  gpioInitialise()                          { return 1; }
    inline void gpioTerminate()                           { }

#endif

constexpr int PIN_STEP = 17; // GPIO17 -> PUL-
constexpr int PIN_DIR  = 25; // GPIO25 -> DIR-
constexpr int PIN_ENA  = 24; // GPIO24 -> ENA-

constexpr float FREQ_MAX_DEFAULT   = 8000;   // 5 rpm * 1600 microsteps per rotation
constexpr float DEG_PULSE_DEFAULT  = 0.225F;

constexpr float SPEED_MAX_DEG_SEC =  180.0F;
constexpr float ACCEL_MAX_DEG_SEC2 = 720.0F;

constexpr int PULSE_HIGH_US_MIN = 10;


#endif //BASE_CPP_HARDWARE_H
