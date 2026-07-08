#ifndef BASE_CPP_HARDWARE_H
#define BASE_CPP_HARDWARE_H

#if SYSTEM_IS_RPI4
    #include <pigpio.h>
#else
    #define gpioWrite(pin, value)
    #define gpioSetMode(pin, mode)
    #define gpioInitialise()  1
    #define gpioTerminate()

#endif


#endif //BASE_CPP_HARDWARE_H
