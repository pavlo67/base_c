#include "hardware/stepper_motor/platform_config.h"
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
    for (size_t i = 0; i < rotations.size(); ++i) {
        printf("[PRB] %s move %zu/%zu\n", axis, i + 1, rotations.size());
        if (stepperMotorRun(motors[AXIS], rotations[i], axis) < 0) { return 1; }
    }
    return 0;
}
