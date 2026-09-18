#include "health_monitor.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace {

constexpr int SCREEN_HEIGHT = 40;
constexpr auto UPDATE_INTERVAL = std::chrono::seconds(1);
constexpr double TEMP_WARNING_C = 80.0;
constexpr double MEMORY_AVAILABLE_WARNING_RATIO = 0.10;

struct Check {
    std::string text;
    bool problem = false;
};

std::atomic<bool> stopRequested = false;

std::string runCommand(const char* command);
std::string readFirstLine(const char* path);
std::string trimCopy(std::string value);
std::string platformName();
std::vector<std::string> readKernelWarnings();
std::vector<Check> collectChecks(const std::vector<std::string>& kernelBaseline);
void draw(const std::vector<Check>& checks, bool paused);
void onSignal(int signal);

#if defined(__linux__)
class Terminal final {
public:
    Terminal();
    ~Terminal();
    bool readChar(char& value) const;

private:
    termios original_{};
    bool configured_ = false;
};
#endif

} // namespace

int runHealthMonitor() {
#if !defined(__linux__)
    printf("health_monitor ERROR: Linux is required\n");
    return 1;
#else
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    Terminal terminal;
    const std::vector<std::string> kernelBaseline = readKernelWarnings();
    std::vector<Check> checks = collectChecks(kernelBaseline);
    bool paused = false;

    draw(checks, paused);
    auto nextUpdate = std::chrono::steady_clock::now() + UPDATE_INTERVAL;

    while (!stopRequested.load()) {
        char value = 0;
        if (terminal.readChar(value)) {
            if (value == ' ') {
                paused = !paused;
                if (paused) {
                    draw(checks, true);
                } else {
                    checks = collectChecks(kernelBaseline);
                    draw(checks, false);
                    nextUpdate = std::chrono::steady_clock::now() + UPDATE_INTERVAL;
                }
            } else if (value == 'q' || value == 'Q') {
                stopRequested.store(true);
            }
        }

        if (!paused && !stopRequested.load() && std::chrono::steady_clock::now() >= nextUpdate) {
            checks = collectChecks(kernelBaseline);
            draw(checks, false);
            nextUpdate = std::chrono::steady_clock::now() + UPDATE_INTERVAL;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
#endif
}

namespace {

const char* ON_RUN_COMMAND = "[health_monitor.runCommand()]";
std::string runCommand(const char* command) {
    std::string result;
    FILE* pipe = popen(command, "r");
    if (pipe == nullptr) {
        printf("%s ERROR: popen failed for: %s\n", ON_RUN_COMMAND, command);
        return result;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return trimCopy(result);
}

const char* ON_READ_FIRST_LINE = "[health_monitor.readFirstLine()]";
std::string readFirstLine(const char* path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return {};
    }
    std::string value;
    std::getline(input, value);
    return trimCopy(value);
}

std::string trimCopy(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string platformName() {
#if defined(SYSTEM_IS_RPI4)
    return "Raspberry Pi 4 / CM4";
#elif defined(SYSTEM_IS_RPI5)
    return "Raspberry Pi 5 / CM5";
#elif defined(SYSTEM_IS_DESKTOP)
    return "Desktop Linux";
#else
    return "Linux";
#endif
}

std::vector<std::string> readKernelWarnings() {
    std::vector<std::string> lines;
    const std::string output = runCommand("dmesg --level=err,warn 2>/dev/null");
    std::istringstream input(output);
    std::string line;
    while (std::getline(input, line)) {
        line = trimCopy(line);
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::vector<Check> collectChecks(const std::vector<std::string>& kernelBaseline) {
    std::vector<Check> checks;
    checks.push_back({"Platform:    " + platformName(), false});

#if defined(SYSTEM_IS_RPI4) || defined(SYSTEM_IS_RPI5)
    const std::string temperature = runCommand("vcgencmd measure_temp 2>/dev/null");
    bool temperatureProblem = false;
    const auto equals = temperature.find('=');
    if (equals != std::string::npos) {
        try {
            temperatureProblem = std::stod(temperature.substr(equals + 1)) >= TEMP_WARNING_C;
        } catch (...) {
            temperatureProblem = true;
        }
    } else {
        temperatureProblem = true;
    }
    checks.push_back({"Temp:        " + (temperature.empty() ? "<unavailable>" : temperature), temperatureProblem});

    const std::string throttled = runCommand("vcgencmd get_throttled 2>/dev/null");
    checks.push_back({"Throttled:   " + (throttled.empty() ? "<unavailable>" : throttled),
                      throttled.empty() || throttled != "throttled=0x0"});

    const std::string volts = runCommand("vcgencmd measure_volts core 2>/dev/null");
    checks.push_back({"Core volts:  " + (volts.empty() ? "<unavailable>" : volts), volts.empty()});
#else
    std::string thermal = readFirstLine("/sys/class/thermal/thermal_zone0/temp");
    if (!thermal.empty()) {
        try {
            const double value = std::stod(thermal) / 1000.0;
            std::ostringstream text;
            text << "Temp:        " << std::fixed << std::setprecision(1) << value << "'C";
            checks.push_back({text.str(), value >= TEMP_WARNING_C});
        } catch (...) {
            checks.push_back({"Temp:        <unreadable>", true});
        }
    } else {
        checks.push_back({"Temp:        <not exposed>", false});
    }
#endif

    const std::string frequencyRaw = readFirstLine("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (!frequencyRaw.empty()) {
        try {
            checks.push_back({"CPU freq:    " + std::to_string(std::stol(frequencyRaw) / 1000) + " MHz", false});
        } catch (...) {
            checks.push_back({"CPU freq:    <unreadable>", true});
        }
    } else {
        checks.push_back({"CPU freq:    <not exposed>", false});
    }

    std::ifstream loadInput("/proc/loadavg");
    double load1 = 0.0;
    double load5 = 0.0;
    double load15 = 0.0;
    if (loadInput >> load1 >> load5 >> load15) {
        const unsigned int cpuCount = std::max(1u, std::thread::hardware_concurrency());
        std::ostringstream text;
        text << "Load:        " << load1 << ' ' << load5 << ' ' << load15;
        checks.push_back({text.str(), load1 > static_cast<double>(cpuCount)});
    } else {
        checks.push_back({"Load:        <unavailable>", true});
    }

    std::ifstream memoryInput("/proc/meminfo");
    long long memoryTotalKb = 0;
    long long memoryAvailableKb = 0;
    long long swapTotalKb = 0;
    long long swapFreeKb = 0;
    std::string key;
    long long value = 0;
    std::string unit;
    while (memoryInput >> key >> value >> unit) {
        if (key == "MemTotal:") { memoryTotalKb = value; }
        if (key == "MemAvailable:") { memoryAvailableKb = value; }
        if (key == "SwapTotal:") { swapTotalKb = value; }
        if (key == "SwapFree:") { swapFreeKb = value; }
    }
    if (memoryTotalKb > 0) {
        const long long usedMb = (memoryTotalKb - memoryAvailableKb) / 1024;
        const long long totalMb = memoryTotalKb / 1024;
        const long long availableMb = memoryAvailableKb / 1024;
        std::ostringstream text;
        text << "Memory:      " << usedMb << '/' << totalMb << " MB used, " << availableMb << " MB available";
        const double availableRatio = static_cast<double>(memoryAvailableKb) / static_cast<double>(memoryTotalKb);
        checks.push_back({text.str(), availableRatio < MEMORY_AVAILABLE_WARNING_RATIO});
    } else {
        checks.push_back({"Memory:      <unavailable>", true});
    }

    if (swapTotalKb > 0) {
        checks.push_back({"Swap:        " + std::to_string((swapTotalKb - swapFreeKb) / 1024) + "/" +
                              std::to_string(swapTotalKb / 1024) + " MB used", false});
    } else {
        checks.push_back({"Swap:        disabled", false});
    }

    const std::string uptime = runCommand("uptime -p 2>/dev/null");
    checks.push_back({"Uptime:      " + (uptime.empty() ? "<unavailable>" : uptime), uptime.empty()});

    const std::vector<std::string> kernelNow = readKernelWarnings();
    if (kernelNow.size() > kernelBaseline.size()) {
        for (size_t i = kernelBaseline.size(); i < kernelNow.size(); ++i) {
            checks.push_back({"Kernel:      " + kernelNow[i], true});
        }
    }

    return checks;
}

void draw(const std::vector<Check>& checks, bool paused) {
    printf("\033[2J\033[H");

    std::vector<std::string> problems;
    std::cout << "=== Linux health monitor ===";
    if (paused) {
        std::cout << "  [PAUSED]";
    }
    std::cout << "\n\n";

    int lines = 2;
    for (const Check& check : checks) {
        std::cout << check.text << '\n';
        ++lines;
        if (check.problem) {
            problems.push_back(check.text);
        }
    }

    const int footerLines = problems.empty() ? 0 : static_cast<int>(problems.size()) + 1;
    while (lines < SCREEN_HEIGHT - footerLines) {
        std::cout << '\n';
        ++lines;
    }

    if (!problems.empty()) {
        std::cout << "=== PROBLEMS ===\n";
        for (const std::string& problem : problems) {
            std::cout << problem << '\n';
        }
    }
    std::cout.flush();
}

void onSignal(int signal) {
    static_cast<void>(signal);
    stopRequested.store(true);
}

#if defined(__linux__)
const char* ON_TERMINAL_CONSTRUCTOR = "[health_monitor.Terminal()]";
Terminal::Terminal() {
    if (!isatty(STDIN_FILENO)) {
        return;
    }
    if (tcgetattr(STDIN_FILENO, &original_) != 0) {
        printf("%s ERROR: tcgetattr failed\n", ON_TERMINAL_CONSTRUCTOR);
        return;
    }

    termios configured = original_;
    configured.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    configured.c_cc[VMIN] = 0;
    configured.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &configured) != 0) {
        printf("%s ERROR: tcsetattr failed\n", ON_TERMINAL_CONSTRUCTOR);
        return;
    }
    configured_ = true;
}

Terminal::~Terminal() {
    if (configured_) {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_);
    }
}

const char* ON_TERMINAL_READ_CHAR = "[health_monitor.Terminal.readChar()]";
bool Terminal::readChar(char& value) const {
    if (!configured_) {
        return false;
    }

    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeval timeout{};
    if (select(STDIN_FILENO + 1, &set, nullptr, nullptr, &timeout) < 0) {
        return false;
    }
    if (!FD_ISSET(STDIN_FILENO, &set)) {
        return false;
    }
    return ::read(STDIN_FILENO, &value, 1) == 1;
}
#endif

} // namespace
