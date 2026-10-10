if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(health_monitor
        health/main_monitor.cpp
        health/health_monitor.cpp
    )
    target_compile_definitions(health_monitor PRIVATE BASE_USE_HEALTH_MONITOR=1)
    target_link_libraries(health_monitor PRIVATE Threads::Threads)
endif()
