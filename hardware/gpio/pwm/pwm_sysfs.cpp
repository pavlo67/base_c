#include "pwm_sysfs.h"
#include <cerrno>
#include <charconv>
#include <chrono>
#include <thread>

int PwmSysfs::claim(unsigned pin, unsigned channel) {
    if (channel >= channels_.size()) { return Gpio::NOT_SUPPORTED; }
    auto& state = channels_[channel];
    if (state.owned) { return Gpio::CHANNEL_BUSY; }
    std::string chip;
    int result = io_.findChip(pin, chip);
    if (result < 0) { return result; }
    std::string count;
    result = io_.read(chip + "/npwm", count);
    if (result < 0) { return result; }
    unsigned npwm = 0;
    const auto parsed = std::from_chars(count.data(), count.data() + count.size(), npwm);
    if (parsed.ec != std::errc{} || npwm <= channel) { return Gpio::NOT_SUPPORTED; }
    // Never adopt an exported channel: it may belong to another process.
    result = io_.write(chip + "/export", std::to_string(channel));
    if (result < 0) { return result == -EBUSY ? Gpio::CHANNEL_BUSY : result; }
    state = {chip, true};
    const auto path = chip + "/pwm" + std::to_string(channel);
    // sysfs creation and udev permission updates may lag behind export.
    for (unsigned attempt = 0; attempt < 50; ++attempt) {
        result = io_.write(path + "/enable", "0");
        if (result != -ENOENT && result != -EACCES) { break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (result >= 0) { result = io_.write(path + "/polarity", "normal"); }
    if (result >= 0) { result = apply(channel, PwmSettings{}); }
    if (result < 0) {
        // Retain ownership if cleanup fails, so close() can retry.
        if (io_.write(chip + "/unexport", std::to_string(channel)) >= 0) { state = {}; }
    }
    return result;
}

int PwmSysfs::apply(unsigned channel, const PwmSettings& settings) {
    if (channel >= channels_.size() || !channels_[channel].owned) { return Gpio::WRONG_MODE; }
    if (!settings.frequency || settings.frequency > 10000 || !settings.range ||
        settings.range > 40000 || settings.duty > settings.range) { return Gpio::INVALID_ARGUMENT; }
    const auto path = channels_[channel].chip + "/pwm" + std::to_string(channel);
    const uint64_t period = 1000000000ULL / settings.frequency;
    const uint64_t duty = settings.enabled ? period * settings.duty / settings.range : 0;
    // First reduce duty to zero: an old duty must not exceed the new period.
    int result = io_.write(path + "/duty_cycle", "0");
    if (result >= 0) { result = io_.write(path + "/period", std::to_string(period)); }
    if (result >= 0) { result = io_.write(path + "/duty_cycle", std::to_string(duty)); }
    // OFF is zero duty with the controller enabled: a disabled PWM need not be LOW.
    if (result >= 0) { result = io_.write(path + "/enable", "1"); }
    if (result < 0) { io_.write(path + "/duty_cycle", "0"); }
    return result;
}

int PwmSysfs::release(unsigned channel) {
    if (channel >= channels_.size()) { return Gpio::INVALID_ARGUMENT; }
    auto& state = channels_[channel];
    if (!state.owned) { return 0; }
    const auto path = state.chip + "/pwm" + std::to_string(channel);
    int result = io_.write(path + "/duty_cycle", "0");
    if (result < 0) { return result; }
    result = io_.write(path + "/enable", "0");
    if (result < 0) { return result; }
    result = io_.write(state.chip + "/unexport", std::to_string(channel));
    if (result >= 0) { state = {}; }
    return result;
}

int PwmSysfs::close() {
    int result = 0;
    for (unsigned channel = 0; channel < channels_.size(); ++channel) {
        const int released = release(channel);
        if (released < 0 && result == 0) { result = released; }
    }
    return result;
}
