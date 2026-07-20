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

#endif //BASE_CPP_HARDWARE_H
