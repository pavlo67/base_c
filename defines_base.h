#ifndef DEFINES_BASE_H
#define DEFINES_BASE_H

#include <string>

extern std::string COMMIT;

#define POINTS_LOG     false
#if POINTS_LOG
#define POINT(label1, label2) printf("%d-%d\n", label1, label2);
#else
#define POINT(label1, label2)
#endif

#define JSON_ERROR         false
#define TIMING_LOG         true
#define SUPPRESS_INIT_INFO true

#define STEPPER_MOTOR_PROBE_VERBOSE false
#define STEPPER_MOTOR_PROBE_HARDWARE_PWM false

#endif // DEFINES_BASE_H

