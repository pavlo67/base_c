#ifndef BASE_CPP_STEPPER_MOTOR_H
#define BASE_CPP_STEPPER_MOTOR_H

#include <array>
#include <algorithm>
#include <chrono>
#include <mutex>

#include "hardware/gpio/gpio.h"

#include "stepper_motor_series.h"

// GPIO lifecycle belongs to the caller. Serialize calls for each motor.
std::mutex& stepperMotorExecutionMutex();

class StepperMotor {
public:
    static constexpr int RUNNING = 0;
    static constexpr int COMPLETE = 1;
    static constexpr int TIME_LIMIT = -10008;

    explicit StepperMotor(const StepperMotorRunConfig& config);
    virtual ~StepperMotor();
    StepperMotor(const StepperMotor&) = delete;
    StepperMotor& operator=(const StepperMotor&) = delete;

    // No sleeps: one update at a monotonic timestamp. Negative results are errors.
    virtual int action(moment at) = 0;
    // rotationDeg is signed degrees. Optional estimates log ideal and clocked plans.
    // The real output receives runtime/partial results even on failure.
    int probe(float rotationDeg, bool withEstimates, const std::string& label = "motor", StepperMotorSeriesSequence* real = nullptr);
    int prepare(float angle, std::array<bool, Gpio::PIN_COUNT>& usedPins);
    int update();
    [[nodiscard]] Clock::time_point nextWake() const { return std::min(next_, deadline_); }
    [[nodiscard]] const StepperMotorSeriesSequence& result() const { return sequence_; }
    int stop();

protected:
    virtual StepperMotorSeriesSequence planSequence(float angle, duration expecterInterval) const;
    virtual void prepareSequence() = 0;
    void setSequence(const StepperMotorSeriesSequence& sequence);
    int initialize(moment at);
    StepperMotorRunConfig config_;
    StepperMotorSeriesSequence sequence_;
    bool enableConfigured_ = false;
    bool stepConfigured_ = false;
    bool directionConfigured_ = false;
    bool initialized_ = false;
    moment lastAt_ = 0;
    uint64_t accelerationTicks_ = 0;
    moment readyAt_ = 0;
    moment phaseStartedAt_ = 0;
    size_t index_ = 0;
    int status_ = COMPLETE;
    unsigned frequency_ = 0;
    Clock::time_point next_{};
    Clock::time_point deadline_{};
    std::chrono::nanoseconds tick_{};
    bool started_ = false;

private:
    int runReal(const std::string& label, StepperMotorSeriesSequence* real);
};

#endif // BASE_CPP_STEPPER_MOTOR_H
