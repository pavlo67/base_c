#include "os.h"

#include <cerrno>
#include <cstring>
#include <system_error>

#if defined(SYSTEM_IS_DESKTOP) || defined(SYSTEM_IS_RPI4) || defined(SYSTEM_IS_RPI5)
    #include <sys/reboot.h>
    #include <unistd.h>
#endif

namespace {
    bool changePowerState(int command, std::string& error) {
        error.clear();
    #ifdef MACHINA_TESTING
        static_cast<void>(command);
        return true;
    #else
        sync();
        if (reboot(command) == 0) { return true; }
        error = std::strerror(errno);
        return false;
    #endif
    }
} // namespace

bool executableDirectory(std::filesystem::path& directory, std::string& error) {
    directory.clear();
    error.clear();
    #if defined(SYSTEM_IS_DESKTOP) || defined(SYSTEM_IS_RPI4) || defined(SYSTEM_IS_RPI5)
        std::error_code pathError;
        const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", pathError);
        if (pathError || executable.empty()) {
            error = pathError ? pathError.message() : "empty executable path";
            return false;
        }
        directory = executable.parent_path();
        return true;
    #else
        error = "executable path is unavailable on this platform";
        return false;
    #endif
}

bool rebootOS(std::string& error) {
    #ifdef MACHINA_TESTING
        return changePowerState(0, error);
    #else
        return changePowerState(RB_AUTOBOOT, error);
    #endif
}

bool shutdownOS(std::string& error) {
    #ifdef MACHINA_TESTING
        return changePowerState(0, error);
    #else
        return changePowerState(RB_POWER_OFF, error);
    #endif
}
