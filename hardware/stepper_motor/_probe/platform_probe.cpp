#include "lib/strlib.h"

#include "hardware/gpio/gpio.h"
#include "hardware/stepper_motor/config/platform_config.h"
#include "hardware/stepper_motor/dumb/stepper_motor_dumb.h"
#include "hardware/stepper_motor/smart/stepper_motor_smart.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

constexpr const char* CONFIG_PATH = "_env/machina.yaml";

int main(int argc, char** argv) {
    if (argc < 3 || (argc - 1) % 2 != 0) {
        printf("ERROR: supply pan/tilt angle pairs, e.g. platform_probe 10 0 -5 1\n");
        return 1;
    }
    std::vector<std::array<float, 2>> movements;
    movements.reserve((argc - 1) / 2);
    for (int i = 1; i < argc; i += 2) {
        std::array<float, 2> angles{};
        if (!parseFiniteFloat(argv[i], angles[0]) || !parseFiniteFloat(argv[i + 1], angles[1])) {
            printf("ERROR: invalid pan/tilt pair at arguments %d and %d\n", i, i + 1);
            return 1;
        }
        movements.push_back(angles);
    }

    const Config config(CONFIG_PATH);
    StepperMotorProbeConfig probe;
    if (!loadStepperMotorProbeConfig(config, probe)) { return 1; }
    for (size_t axis = 0; axis < probe.motors_.size(); ++axis) {
        const auto& motor = probe.motors_[axis];
        printf("[PRB] %s pins (BCM): PUL/STEP=%d DIR=%d ENA=%d\n",
            axis == 0 ? "pan" : "tilt", motor.pinStep_, motor.pinDir_, motor.pinEna_);
    }
    fflush(stdout);
    auto& gpio = Gpio::instance();
    if (gpio.initialize() < 0) {
        printf("ERROR: GPIO initialization failed\n");
        return 1;
    }

    int result = 0;
    {
        const std::lock_guard<std::mutex> execution(stepperMotorExecutionMutex());
        for (size_t pair = 0; pair < movements.size(); ++pair) {
            printf("[PRB] Pair %zu/%zu: pan=%g deg, tilt=%g deg, class=%s\n", pair + 1,
                movements.size(), movements[pair][0], movements[pair][1],
                probe.kind_ == StepperMotorKind::smart ? "Smart" : "Dumb");
            std::array<std::unique_ptr<StepperMotor>, 2> motors;
            std::array<bool, Gpio::PIN_COUNT> usedPins{};
            std::array<int, 2> statuses{};
            bool failed = false;
            for (size_t axis = 0; axis < motors.size(); ++axis) {
                if (probe.kind_ == StepperMotorKind::smart) {
                    motors[axis] = std::make_unique<StepperMotorSmart>(probe.motors_[axis]);
                } else {
                    const auto& dumb = probe.dumb_[axis];
                    motors[axis] = std::make_unique<StepperMotorDumb>(probe.motors_[axis],
                        dumb.speedDegPerSec_, dumb.pauseAfterSeries_, dumb.remainingPulsesTolerance_);
                }
                statuses[axis] = motors[axis]->prepare(movements[pair][axis], usedPins);
                if (statuses[axis] < 0) { failed = true; break; }
            }
            if (failed) { result = 1; break; }
            for (;;) {
                bool active = false;
                Clock::time_point wake = Clock::time_point::max();
                for (size_t axis = 0; axis < motors.size(); ++axis) {
                    if (statuses[axis] != StepperMotor::RUNNING) { continue; }
                    statuses[axis] = motors[axis]->update();
                    if (statuses[axis] < 0) { failed = true; break; }
                    if (statuses[axis] == StepperMotor::RUNNING) {
                        active = true;
                        wake = std::min(wake, motors[axis]->nextWake());
                    }
                }
                if (failed || !active) { break; }
                std::this_thread::sleep_until(wake);
            }
            if (failed) {
                for (auto& motor : motors) { if (motor) { (void)motor->stop(); } }
                result = 1;
                break;
            }
            for (size_t axis = 0; axis < motors.size(); ++axis) {
                motors[axis]->result().log(probe.motors_[axis].options_, axis == 0 ? "[pan] real" : "[tilt] real",
                    probe.motors_[axis].verbose_, probe.motors_[axis].expecterInterval_);
            }
        }
    }

    if (gpio.terminate() < 0) {
        printf("ERROR: GPIO termination failed\n");
        result = 1;
    }
    return result;
}
