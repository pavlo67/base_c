add_executable(pan_tilt_probe
        pan_tlt/pan_tilt_probe.cpp
        pan_tlt/pan_tlt_stepper/pan_tilt_stepper.cpp

        ../lib/json/json.cpp
        pan_tlt/pan_tlt_stepper/pan_tilt_stepper_config.cpp
)

# target_link_libraries(hello PRIVATE Threads::Threads)
