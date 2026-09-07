#ifndef BASE_DEFINES_H
#define BASE_DEFINES_H

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
#define PROCESS_INFO_STEP  400
#define SAVE_IMAGES_EXT    ".png"

#define RUN_PROBE true

#ifndef STEPPER_MOTOR_PROBE_VERBOSE
#define STEPPER_MOTOR_PROBE_VERBOSE false
#endif
#ifndef STEPPER_MOTOR_PROBE_HARDWARE_PWM
#define STEPPER_MOTOR_PROBE_HARDWARE_PWM true
#endif

#endif // BASE_DEFINES_H

