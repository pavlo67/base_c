#include "os.h"

#include <cerrno>
#include <cstring>

#ifndef MACHINA_TESTING
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
