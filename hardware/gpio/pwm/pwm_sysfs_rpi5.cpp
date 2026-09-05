#if (defined(SYSTEM_IS_RPI5) && SYSTEM_IS_RPI5) || defined(GPIO_SYSFS_TEST)

#include "pwm_sysfs.h"
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <vector>

int Rpi5PwmSysfsIo::read(const std::string& path, std::string& value) {
    value.clear();
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) { return -errno; }
    char buffer[1024];
    int result = 0;
    while (true) {
        const auto count = ::read(fd, buffer, sizeof(buffer));
        if (count < 0 && errno == EINTR) { continue; }
        if (count < 0) { result = -errno; break; }
        if (count == 0) { break; }
        value.append(buffer, count);
    }
    ::close(fd);
    return result;
}

int Rpi5PwmSysfsIo::write(const std::string& path, const std::string& value) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) { return -errno; }
    ssize_t count;
    do { count = ::write(fd, value.data(), value.size()); } while (count < 0 && errno == EINTR);
    const int result = count < 0 ? -errno : (static_cast<std::size_t>(count) == value.size() ? 0 : -EIO);
    const int closed = ::close(fd);
    return result < 0 ? result : (closed < 0 ? -errno : 0);
}

int Rpi5PwmSysfsIo::findChip(unsigned pin, std::string& chip) {
    std::error_code error;
    const std::filesystem::path root(pwmRoot_);
    std::filesystem::directory_iterator it(root, error), end;
    if (error) { return -error.value(); }
    for (; it != end; it.increment(error)) {
        if (error) { return -error.value(); }
        const auto node = it->path() / "device/of_node";
        std::string compatible;
        if (read((node / "compatible").string(), compatible) < 0 ||
            compatible.find("raspberrypi,rp1-pwm") == std::string::npos) { continue; }
        // Match the header controller, not PWM1 used by the onboard fan.
        const auto canonical = std::filesystem::canonical(node, error);
        if (error) { return -error.value(); }
        if (canonical.filename() != "pwm@98000") { continue; }
        std::string handles;
        if (read((node / "pinctrl-0").string(), handles) < 0 || handles.empty() || handles.size() % 4) {
            continue;
        }
        // Follow live Device Tree phandles. Raspberry Pi firmware may expose
        // either legacy brcm,pins/function cells or RP1 pins/function strings.
        std::filesystem::recursive_directory_iterator nodes(treeRoot_, error), nodesEnd;
        if (error) { return -error.value(); }
        for (; nodes != nodesEnd; nodes.increment(error)) {
            if (error) { return -error.value(); }
            if (nodes->path().filename() != "phandle") { continue; }
            std::string handle;
            if (read(nodes->path().string(), handle) < 0 || handle.size() != 4) { continue; }
            bool matches = false;
            for (std::size_t offset = 0; offset < handles.size(); offset += 4) {
                if (handles.compare(offset, 4, handle) == 0) { matches = true; break; }
            }
            if (!matches) { continue; }
            const auto group = nodes->path().parent_path();
            std::string pins, function;
            if (read((group / "pins").string(), pins) == 0 && read((group / "function").string(), function) == 0) {
                const std::string wanted = "gpio" + std::to_string(pin) + '\0';
                const bool found = (std::string(1, '\0') + pins).find(std::string(1, '\0') + wanted) != std::string::npos;
                if (found && function == std::string("pwm0\0", 5)) { chip = it->path().string(); return 0; }
            }
            if (read((group / "brcm,pins").string(), pins) < 0 ||
                read((group / "brcm,function").string(), function) < 0) { continue; }
            const unsigned expected = pin >= 18 ? 2 : 4; // Legacy ALT5 / ALT0 encoding.
            for (std::size_t offset = 0; offset + 4 <= pins.size(); offset += 4) {
                const std::string wantedPin = std::string(3, '\0') + static_cast<char>(pin);
                const auto functionOffset = function.size() == 4 ? 0 : offset;
                const std::string wantedFunction = std::string(3, '\0') + static_cast<char>(expected);
                if (pins.compare(offset, 4, wantedPin) == 0 && functionOffset + 4 <= function.size() &&
                    function.compare(functionOffset, 4, wantedFunction) == 0) {
                    chip = it->path().string();
                    return 0;
                }
            }
        }
    }
    return Gpio::PWM_NOT_CONFIGURED;
}

#endif // RPI5
