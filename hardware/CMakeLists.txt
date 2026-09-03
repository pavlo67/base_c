add_executable(rpi4_probe
        rpi4/rpi4_probe.cpp
)
target_link_libraries(rpi4_probe PRIVATE ${RPI_LIBS})

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
        stepper_motor/stepper_series.cpp
)
target_link_libraries(stepper_motor PUBLIC Threads::Threads)

# stepper_motor test ----------------------------------

add_executable(stepper_motor_test
        stepper_motor/stepper_motor_test.cpp
)
target_link_libraries(stepper_motor_test PRIVATE stepper_motor  GTest::gtest_main)
add_test(NAME stepper_motor_test COMMAND stepper_motor_test)

# probe_infinite --------------------------------------

add_executable(probe_infinite
        stepper_motor/_probe/probe_infinite.cpp
)
target_link_libraries(probe_infinite PRIVATE stepper_motor ${RPI_LIBS})

# probe_180_360_180 -------------------------------------

add_executable(probe_180_360_180
        stepper_motor/_probe/probe_180_360_180.cpp
)
target_link_libraries(probe_180_360_180 PRIVATE stepper_motor ${RPI_LIBS})

# probe_sequences -------------------------------------

add_executable(probe_sequences
        stepper_motor/_probe/probe_sequences.cpp
)
target_link_libraries(probe_sequences PRIVATE stepper_motor ${RPI_LIBS})

