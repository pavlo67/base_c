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
target_link_libraries(gpio_test PRIVATE hardware_gpio GTest::gtest_main)
add_test(NAME gpio_test COMMAND gpio_test)
set_tests_properties(gpio_test PROPERTIES RUN_SERIAL TRUE LABELS "gpio")

# Contract tests always use the desktop implementation, even on a Raspberry Pi.
add_executable(gpio_contract_test gpio/gpio_contract_test.cpp gpio/gpio.cpp gpio/platforms/gpio_desktop.cpp)
target_compile_definitions(gpio_contract_test PRIVATE GPIO_DESKTOP_TEST=1)
target_link_libraries(gpio_contract_test PRIVATE GTest::gtest_main)
add_test(NAME gpio_contract_test COMMAND gpio_contract_test)

add_executable(gpio_probe
        gpio/_probes/gpio_probe.cpp
)
target_link_libraries(gpio_probe PRIVATE hardware_gpio)

# -----------------------------------------------------

add_executable(pan_tlt_probe
        pan_tlt/pan_tlt_probe.cpp
        pan_tlt/pan_tlt_stepper/pan_tlt_stepper.cpp
        pan_tlt/pan_tlt_stepper/pan_tlt_stepper_cfg.cpp
)
target_link_libraries(pan_tlt_probe PRIVATE hardware_gpio)
if(TARGET JsonCpp::JsonCpp)
    target_link_libraries(pan_tlt_probe PRIVATE JsonCpp::JsonCpp)
else()
    target_link_libraries(pan_tlt_probe PRIVATE jsoncpp_lib)
endif()

# stepper_motor library -------------------------------

add_library(stepper_motor
        stepper_motor/stepper_motor.cpp
        stepper_motor/stepper_series.cpp
)
target_link_libraries(stepper_motor PUBLIC Threads::Threads hardware_gpio)

# stepper_motor test ----------------------------------

add_executable(stepper_motor_test
        stepper_motor/stepper_motor_test.cpp
)
target_link_libraries(stepper_motor_test PRIVATE stepper_motor GTest::gtest_main)
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

add_executable(pwm_sysfs_test gpio/pwm/pwm_sysfs_test.cpp gpio/pwm/pwm_sysfs.cpp gpio/pwm/pwm_sysfs_rpi5.cpp)
target_compile_definitions(pwm_sysfs_test PRIVATE GPIO_SYSFS_TEST=1)
target_link_libraries(pwm_sysfs_test PRIVATE GTest::gtest_main Threads::Threads)
add_test(NAME pwm_sysfs_test COMMAND pwm_sysfs_test)
