add_executable(pan_tlt_probe
        pan_tlt/pan_tlt_probe.cpp
        pan_tlt/pan_tlt_stepper/pan_tlt_stepper.cpp

        ../lib/json/json.cpp
        pan_tlt/pan_tlt_stepper/pan_tlt_stepper_cfg.cpp
)

# target_link_libraries(hello PRIVATE Threads::Threads)

add_executable(stepper_motor_probe
        stepper_motor/main_probe.cpp
)
