#ifndef BASE_OS_WINDOW_WINDOW_H
#define BASE_OS_WINDOW_WINDOW_H

#include <atomic>
#include <string>

using WindowId = unsigned long;

bool isWindowEnvironmentAvailable(std::string& error);
bool getActiveWindowId(WindowId& windowId, std::string& error);
bool positionGstreamerWindowTopLeft(const std::atomic_bool& positioningActive, WindowId& windowId, std::string& error);
bool positionWindowRightOfWindow(WindowId windowId, WindowId leftWindowId, std::string& error);

#endif // BASE_OS_WINDOW_WINDOW_H
