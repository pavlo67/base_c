string(TOLOWER "${SYSTEM}" GPIO_PLATFORM_SOURCE)
add_library(hardware_gpio gpio/gpio.cpp gpio/platforms/gpio_${GPIO_PLATFORM_SOURCE}.cpp)
if(SYSTEM STREQUAL "RPI5")
    target_sources(hardware_gpio PRIVATE gpio/pwm/pwm_sysfs.cpp gpio/pwm/pwm_sysfs_rpi5.cpp)
endif()
target_include_directories(hardware_gpio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
if(GPIO_INCLUDE_DIR)
    target_include_directories(hardware_gpio PRIVATE ${GPIO_INCLUDE_DIR})
endif()
target_link_libraries(hardware_gpio PUBLIC Threads::Threads ${RPI_LIBS})

add_executable(gpio_test gpio/gpio_test.cpp)
set_target_properties(gpio_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/_test>")
target_link_libraries(gpio_test PRIVATE hardware_gpio GTest::gtest_main)
add_test(NAME gpio_test COMMAND gpio_test)
set_tests_properties(gpio_test PROPERTIES RUN_SERIAL TRUE LABELS "gpio")

## Contract tests always use the desktop implementation, even on a Raspberry Pi.
#add_executable(gpio_contract_test gpio/gpio_contract_test.cpp gpio/gpio.cpp gpio/platforms/gpio_desktop.cpp)
#target_compile_definitions(gpio_contract_test PRIVATE GPIO_DESKTOP_TEST=1)
#target_link_libraries(gpio_contract_test PRIVATE GTest::gtest_main)
#add_test(NAME gpio_contract_test COMMAND gpio_contract_test)

add_executable(gpio_probe
        gpio/_probes/gpio_probe.cpp
)
set_target_properties(gpio_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/_probe>")
target_link_libraries(gpio_probe PRIVATE hardware_gpio)

# stepper_motor library -------------------------------

add_library(stepper_motor
        stepper_motor/stepper_motor.cpp
        stepper_motor/stepper_motor_series_helpers.cpp
        stepper_motor/smart/stepper_motor_smart.cpp
        stepper_motor/dumb/stepper_motor_dumb.cpp
        stepper_motor/stepper_motor_series.cpp
)
target_link_libraries(stepper_motor PUBLIC Threads::Threads hardware_gpio)

# stepper_motor test ----------------------------------

add_executable(stepper_motor_test
        stepper_motor/stepper_motor_test.cpp
        stepper_motor/smart/stepper_motor_smart_test.cpp
        stepper_motor/dumb/stepper_motor_dumb_test.cpp
)
set_target_properties(stepper_motor_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/_test>")
target_link_libraries(stepper_motor_test PRIVATE stepper_motor GTest::gtest_main)
add_test(NAME stepper_motor_test COMMAND stepper_motor_test)

# probe_pulses ---------------------------------------

add_executable(probe_pulses stepper_motor/_probe/probe_pulses.cpp)
set_target_properties(probe_pulses PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/_probe>")
target_link_libraries(probe_pulses PRIVATE hardware_gpio)

# Shared platform mechanics, YAML configuration and probe.
add_library(stepper_platform stepper_motor/config/platform_config.cpp ../lib/config/config.cpp)
target_link_libraries(stepper_platform PUBLIC stepper_motor yaml-cpp::yaml-cpp)
add_executable(probe_platform stepper_motor/_probe/probe_platform.cpp)
set_target_properties(probe_platform PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/_probe>")
target_link_libraries(probe_platform PRIVATE stepper_platform)
add_executable(stepper_platform_test stepper_motor/config/platform_config_test.cpp)
set_target_properties(stepper_platform_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/_test>")
target_link_libraries(stepper_platform_test PRIVATE stepper_platform GTest::gtest_main)
add_test(NAME stepper_platform_test COMMAND stepper_platform_test)

add_executable(pwm_sysfs_test gpio/pwm/pwm_sysfs_test.cpp gpio/pwm/pwm_sysfs.cpp gpio/pwm/pwm_sysfs_rpi5.cpp)
set_target_properties(pwm_sysfs_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_SOURCE_DIR}/_test>")
target_compile_definitions(pwm_sysfs_test PRIVATE GPIO_SYSFS_TEST=1)
target_link_libraries(pwm_sysfs_test PRIVATE GTest::gtest_main Threads::Threads)
add_test(NAME pwm_sysfs_test COMMAND pwm_sysfs_test)
