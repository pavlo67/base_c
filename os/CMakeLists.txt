add_executable(health_monitor
        health/main_monitor.cpp
        health/health_monitor.cpp
)
target_link_libraries(health_monitor PRIVATE Threads::Threads)
