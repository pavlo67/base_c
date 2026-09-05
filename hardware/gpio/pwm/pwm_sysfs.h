#pragma once

#include "../gpio.h"
#include <array>
#include <cstdint>
#include <string>

// Injectable sysfs transport: tests exercise ordering and failures without devices.
class PwmSysfsIo {
public:
    virtual ~PwmSysfsIo() = default;
    virtual int findChip(unsigned pin, std::string& chip) = 0;
    virtual int read(const std::string& path, std::string& value) = 0;
    virtual int write(const std::string& path, const std::string& value) = 0;
};

class PwmSysfs {
public:
    explicit PwmSysfs(PwmSysfsIo& io) : io_(io) {}
    int claim(unsigned pin, unsigned channel);
    int apply(unsigned channel, const PwmSettings& settings);
    int release(unsigned channel);
    int close();
private:
    struct Channel {
        std::string chip;
        bool owned = false;
    };
    PwmSysfsIo& io_;
    std::array<Channel, 4> channels_{};
};

#if (defined(SYSTEM_IS_RPI5) && SYSTEM_IS_RPI5) || defined(GPIO_SYSFS_TEST)
class Rpi5PwmSysfsIo final : public PwmSysfsIo {
public:
    explicit Rpi5PwmSysfsIo(std::string pwmRoot = "/sys/class/pwm",
                            std::string treeRoot = "/sys/firmware/devicetree/base")
        : pwmRoot_(pwmRoot), treeRoot_(treeRoot) {}
    int findChip(unsigned pin, std::string& chip) override;
    int read(const std::string& path, std::string& value) override;
    int write(const std::string& path, const std::string& value) override;
private:
    std::string pwmRoot_;
    std::string treeRoot_;
};
#endif
