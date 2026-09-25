#include "hardware/stepper_motor/config/platform_config.h"
#include "hardware/gpio/gpio.h"

#include <chrono>
#include <cstdio>
#include <thread>

constexpr const char* HARDWARE_CONFIG_PATH = "machina.yaml";
constexpr unsigned BLINK_DELAY_US = 500000;
constexpr int BLINKS_CNT = 5;

int main() {
    printf("[GPIO] Load pan hardware from %s\n", HARDWARE_CONFIG_PATH);
    std::array<StepperMotorRunConfig, 2> motors;
    if (!loadPlatformMotorConfig(Config(HARDWARE_CONFIG_PATH), motors)) { return 1; }
    const auto& pan = motors[0];
    auto& gpio = Gpio::instance();
    if (gpio.initialize() < 0) {
        printf("[main()] ERROR: GPIO initialization failed\n");
        return 1;
    }
    int result = 0;
    for (const unsigned pin : {pan.pinStep_, pan.pinDir_, pan.pinEna_}) {
        printf("[GPIO] Blink BCM%u\n", pin);
        int code = gpio.setMode(pin, GpioMode::output);
        for (int i = 0; i < BLINKS_CNT && code >= 0; ++i) {
            code = gpio.write(pin, 1);
            if (code < 0) { break; }
            std::this_thread::sleep_for(std::chrono::microseconds(BLINK_DELAY_US));
            code = gpio.write(pin, 0);
            if (code < 0) { break; }
            std::this_thread::sleep_for(std::chrono::microseconds(BLINK_DELAY_US));
        }
        if (code < 0) {
            printf("[main()] ERROR: BCM%u blink failed: %d\n", pin, code);
            result = 1;
        }
        const int cleanup = gpio.setMode(pin, GpioMode::input);
        if (cleanup < 0) {
            printf("[main()] ERROR: BCM%u cleanup failed: %d\n", pin, cleanup);
            result = 1;
        }
        if (result != 0) { break; }
    }
    const int terminated = gpio.terminate();
    if (terminated < 0) {
        printf("[main()] ERROR: GPIO termination failed: %d\n", terminated);
        result = 1;
    }
    return result;
}
