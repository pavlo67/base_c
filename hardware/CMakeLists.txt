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
        stepper_motor/pulse_series.cpp
)
target_link_libraries(stepper_motor PUBLIC Threads::Threads)

# probe_90_180_90 -------------------------------------

add_executable(probe_90_180_90
        stepper_motor/_probe/probe_90_180_90.cpp
)
target_link_libraries(probe_90_180_90 PRIVATE stepper_motor ${RPI_LIBS})

# probe_infinite --------------------------------------

add_executable(probe_infinite
        stepper_motor/_probe/probe_infinite.cpp
)
target_link_libraries(probe_infinite PRIVATE stepper_motor ${RPI_LIBS})

