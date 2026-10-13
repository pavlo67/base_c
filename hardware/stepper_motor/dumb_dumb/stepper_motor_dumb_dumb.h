#ifndef BASE_CPP_STEPPER_MOTOR_DUMB_DUMB_H
#define BASE_CPP_STEPPER_MOTOR_DUMB_DUMB_H

#include "../stepper_motor.h"

#include <condition_variable>
#include <thread>

// A deliberately simple motor: its background clock never restarts. Commands
// replace the remaining signed pulse count observed by that clock.
class StepperMotorDumbDumb final : public StepperMotor {
public:
    explicit StepperMotorDumbDumb(const StepperMotorRunConfig& config);
    ~StepperMotorDumbDumb() override;

    int action(moment at) override;
    int replaceMove(float angle) override;
    [[nodiscard]] bool hasBackgroundScheduler() const override { return true; }
    [[nodiscard]] bool hasPendingBackgroundMove() const override;
    [[nodiscard]] int backgroundMoveResult() const override;

protected:
    void prepareSequence() override {}

private:
    int configurePinsLocked();
    void runBackground(std::stop_token stop);

    mutable std::mutex mutex_;
    std::condition_variable_any wake_;
    std::jthread worker_;
    int64_t remainingPulses_ = 0;
    bool pinsConfigured_ = false;
    int result_ = Gpio::SUCCESS;
};

#endif // BASE_CPP_STEPPER_MOTOR_DUMB_DUMB_H
