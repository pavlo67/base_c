add_executable(pan_tilt_probe
        pan_tilt/pan_tilt_probe.cpp
        pan_tilt/pan_tilt_stepper/pan_tilt_stepper.cpp

        ../lib/json/json.cpp
)

# target_link_libraries(hello PRIVATE Threads::Threads)
