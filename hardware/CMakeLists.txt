add_executable(rpi4_probe
        rpi4/rpi4_probe.cpp
)
if (SYSTEM STREQUAL "RPI4")
    target_link_libraries(rpi4_probe PRIVATE pigpio rt)
endif()

# -----------------------------------------------------

add_executable(pan_tlt_probe
        pan_tlt/pan_tlt_probe.cpp
        pan_tlt/pan_tlt_stepper/pan_tlt_stepper.cpp

        ../lib/json/json.cpp
        pan_tlt/pan_tlt_stepper/pan_tlt_stepper_cfg.cpp
)


# stepper_motor library -------------------------------

add_library(stepper_motor
        stepper_motor/stepper_motor.cpp
        stepper_motor/pulse_series.cpp
)
target_link_libraries(stepper_motor PUBLIC Threads::Threads)

# stepper_motor_test ----------------------------------

add_executable(stepper_motor_test
        stepper_motor/stepper_motor_test.cpp
)
target_link_libraries(stepper_motor_test PRIVATE stepper_motor GTest::gtest_main)
add_test(NAME stepper_motor_test COMMAND stepper_motor_test)

# stepper_motor_probe ---------------------------------

add_executable(stepper_motor_probe
        stepper_motor/_probe/main_probe.cpp
)
target_link_libraries(stepper_motor_test PRIVATE stepper_motor)


