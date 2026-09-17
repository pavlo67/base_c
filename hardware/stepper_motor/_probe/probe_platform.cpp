#include "hardware/gpio/gpio.h"
#include "machina/config/platform_config.h"
#include "hardware/stepper_motor/stepper_motor.h"
#include "hardware/stepper_motor/smart/stepper_motor_smart.h"
#include "lib/number_parse.h"

#include <cstdio>
#include <vector>

constexpr const char* CONFIG_PATH = "_env/machina.yaml";
constexpr size_t AXIS = 0; // 0 = pan, 1 = tilt

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("ERROR: supply relative platform angles in degrees, e.g. probe_platform +90 -45 180\n");
        return 1;
    }
    std::vector<float> rotations;
    for (int i = 1; i < argc; ++i) {
        float angle = 0;
        if (!parseFiniteFloat(argv[i], angle)) {
            printf("ERROR: invalid angle at argument %d\n", i);
            return 1;
        }
        rotations.push_back(angle);
    }
    const Config config(CONFIG_PATH);
    std::array<StepperMotorRunConfig, 2> motors{};
    if (!loadPlatformMotorConfig(config, motors)) { return 1; }
    const char* axis = AXIS == 0 ? "pan" : "tilt";
    auto& gpio = Gpio::instance();
    if (gpio.initialize() < 0) { return 1; }
    int result = 0;
    for (size_t i = 0; i < rotations.size(); ++i) {
        printf("[PRB] %s move %zu/%zu\n", axis, i + 1, rotations.size());
        if (StepperMotorSmart(motors[AXIS]).probe(rotations[i], false, axis) < 0) { result = 1; break; }
    }
    if (gpio.terminate() < 0) { result = 1; }
    return result;
}
