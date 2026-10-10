#include "window.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <chrono>
#include <thread>

namespace {
    constexpr const char* ON_IS_WINDOW_ENVIRONMENT_AVAILABLE = "[isWindowEnvironmentAvailable()]";
    constexpr const char* ON_POSITION_GSTREAMER_WINDOW_TOP_LEFT = "[positionGstreamerWindowTopLeft()]";
    constexpr const char* ON_POSITION_WINDOW_RIGHT_OF_WINDOW = "[positionWindowRightOfWindow()]";
    constexpr auto WINDOW_LOOKUP_DELAY = std::chrono::milliseconds(100);
    constexpr int WINDOW_BOTTOM_RESERVED_HEIGHT = 200;
    constexpr int WINDOW_RESERVED_WIDTH = 10;

    struct ScreenLayout {
        int width = 0;
        int height = 0;
    };

    Display* openDisplay(std::string& error, const char* context) {
        Display* display = XOpenDisplay(nullptr);
        if (display == nullptr) {
            error = std::string(context) + " ERROR: cannot connect to X11 display";
        }
        return display;
    }

    ScreenLayout getScreenLayout(Display* display) {
        const int screen = DefaultScreen(display);
        return {DisplayWidth(display, screen), DisplayHeight(display, screen)};
    }

    bool isEwmhAtomSupported(Display* display, Atom requiredAtom) {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long bytesAfter = 0;
        unsigned char* value = nullptr;
        const Atom supportedAtom = XInternAtom(display, "_NET_SUPPORTED", False);
        const int result = XGetWindowProperty(display, DefaultRootWindow(display), supportedAtom, 0, ~0L, False,
                                              XA_ATOM, &actualType, &actualFormat, &itemCount, &bytesAfter, &value);
        if (result != Success || actualType != XA_ATOM || actualFormat != 32 || value == nullptr) {
            if (value != nullptr) {
                XFree(value);
            }
            return false;
        }
        const auto* atoms = reinterpret_cast<Atom*>(value);
        for (unsigned long i = 0; i < itemCount; ++i) {
            if (atoms[i] == requiredAtom) {
                XFree(value);
                return true;
            }
        }
        XFree(value);
        return false;
    }

    bool areWindowMoveResizeRequestsSupported(Display* display, std::string& error, const char* context) {
        const Atom moveResizeAtom = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", False);
        if (!isEwmhAtomSupported(display, moveResizeAtom)) {
            error = std::string(context) + " ERROR: window manager does not support _NET_MOVERESIZE_WINDOW";
            return false;
        }
        return true;
    }

    bool isWindowStateRequestSupported(Display* display, std::string& error, const char* context) {
        const Atom stateAtom = XInternAtom(display, "_NET_WM_STATE", False);
        if (!isEwmhAtomSupported(display, stateAtom)) {
            error = std::string(context) + " ERROR: window manager does not support _NET_WM_STATE";
            return false;
        }
        return true;
    }

    Window getActiveWindow(Display* display) {
        const Atom activeWindowAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long bytesAfter = 0;
        unsigned char* value = nullptr;
        const int result = XGetWindowProperty(display, DefaultRootWindow(display), activeWindowAtom, 0, 1, False,
                                              XA_WINDOW, &actualType, &actualFormat, &itemCount, &bytesAfter, &value);
        if (result != Success || actualType != XA_WINDOW || actualFormat != 32 || itemCount != 1 || value == nullptr) {
            if (value != nullptr) {
                XFree(value);
            }
            return None;
        }
        const Window window = *reinterpret_cast<Window*>(value);
        XFree(value);
        return window;
    }

    bool isGstreamerWindow(Display* display, Window window) {
        XClassHint hint{};
        if (XGetClassHint(display, window, &hint) == 0) {
            return false;
        }
        const bool isGstreamer = hint.res_class != nullptr && std::string(hint.res_class) == "GStreamer";
        if (hint.res_name != nullptr) {
            XFree(hint.res_name);
        }
        if (hint.res_class != nullptr) {
            XFree(hint.res_class);
        }
        return isGstreamer;
    }

    Window findGstreamerWindow(Display* display) {
        Atom actualType = None;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long bytesAfter = 0;
        unsigned char* value = nullptr;
        const Atom clientListAtom = XInternAtom(display, "_NET_CLIENT_LIST", False);
        const int result = XGetWindowProperty(display, DefaultRootWindow(display), clientListAtom, 0, ~0L, False,
                                              XA_WINDOW, &actualType, &actualFormat, &itemCount, &bytesAfter, &value);
        if (result != Success || actualType != XA_WINDOW || actualFormat != 32 || value == nullptr) {
            if (value != nullptr) {
                XFree(value);
            }
            return None;
        }
        const auto* windows = reinterpret_cast<Window*>(value);
        for (unsigned long i = 0; i < itemCount; ++i) {
            if (isGstreamerWindow(display, windows[i])) {
                const Window window = windows[i];
                XFree(value);
                return window;
            }
        }
        XFree(value);
        return None;
    }

    void removeWindowMaximization(Display* display, Window window) {
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = 0;
        event.xclient.data.l[1] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
        event.xclient.data.l[2] = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
        XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }

    bool moveResizeWindow(Display* display, Window window, int x, int y, int width, int height,
                          std::string& error, const char* context) {
        removeWindowMaximization(display, window);
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = NorthWestGravity | (CWX | CWY | CWWidth | CWHeight) << 8;
        event.xclient.data.l[1] = x;
        event.xclient.data.l[2] = y;
        event.xclient.data.l[3] = width;
        event.xclient.data.l[4] = height;
        if (XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event) == 0) {
            error = std::string(context) + " ERROR: cannot send window geometry request";
            return false;
        }
        XFlush(display);
        return true;
    }

    bool moveWindow(Display* display, Window window, int x, int y, std::string& error, const char* context) {
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type = XInternAtom(display, "_NET_MOVERESIZE_WINDOW", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = NorthWestGravity | (CWX | CWY) << 8;
        event.xclient.data.l[1] = x;
        event.xclient.data.l[2] = y;
        if (XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event) == 0) {
            error = std::string(context) + " ERROR: cannot send window position request";
            return false;
        }
        XFlush(display);
        return true;
    }
} // namespace

bool isWindowEnvironmentAvailable(std::string& error) {
    error.clear();
    Display* display = openDisplay(error, ON_IS_WINDOW_ENVIRONMENT_AVAILABLE);
    if (display == nullptr) {
        return false;
    }
    XCloseDisplay(display);
    return true;
}

bool getActiveWindowId(WindowId& windowId, std::string& error) {
    error.clear();
    windowId = 0;
    Display* display = openDisplay(error, "[getActiveWindowId()]");
    if (display == nullptr) {
        return false;
    }
    const Window window = getActiveWindow(display);
    if (window == None) {
        error = "[getActiveWindowId()] ERROR: active window is unavailable";
        XCloseDisplay(display);
        return false;
    }
    windowId = window;
    XCloseDisplay(display);
    return true;
}

bool positionGstreamerWindowTopLeft(const std::atomic_bool& positioningActive, WindowId& windowId, std::string& error) {
    error.clear();
    windowId = 0;
    Display* display = openDisplay(error, ON_POSITION_GSTREAMER_WINDOW_TOP_LEFT);
    if (display == nullptr) {
        return false;
    }
    if (!areWindowMoveResizeRequestsSupported(display, error, ON_POSITION_GSTREAMER_WINDOW_TOP_LEFT)) {
        XCloseDisplay(display);
        return false;
    }
    while (positioningActive.load()) {
        const Window window = findGstreamerWindow(display);
        if (window != None) {
            const bool positioned = moveWindow(display, window, 0, 0, error, ON_POSITION_GSTREAMER_WINDOW_TOP_LEFT);
            if (positioned) {
                windowId = window;
            }
            XCloseDisplay(display);
            return positioned;
        }
        std::this_thread::sleep_for(WINDOW_LOOKUP_DELAY);
    }
    XCloseDisplay(display);
    return false;
}

bool positionWindowRightOfWindow(WindowId windowId, WindowId leftWindowId, std::string& error) {
    error.clear();
    Display* display = openDisplay(error, ON_POSITION_WINDOW_RIGHT_OF_WINDOW);
    if (display == nullptr) {
        return false;
    }
    if (!areWindowMoveResizeRequestsSupported(display, error, ON_POSITION_WINDOW_RIGHT_OF_WINDOW) ||
        !isWindowStateRequestSupported(display, error, ON_POSITION_WINDOW_RIGHT_OF_WINDOW)) {
        XCloseDisplay(display);
        return false;
    }
    XWindowAttributes leftAttributes;
    if (XGetWindowAttributes(display, static_cast<Window>(leftWindowId), &leftAttributes) == 0) {
        error = std::string(ON_POSITION_WINDOW_RIGHT_OF_WINDOW) + " ERROR: cannot get GStreamer window geometry";
        XCloseDisplay(display);
        return false;
    }
    const ScreenLayout screen = getScreenLayout(display);
    const int x = leftAttributes.width;
    const int height = screen.height - WINDOW_BOTTOM_RESERVED_HEIGHT;
    const int width = screen.width - x - WINDOW_RESERVED_WIDTH;
    if (width <= 0 || height <= 0) {
        error = std::string(ON_POSITION_WINDOW_RIGHT_OF_WINDOW) + " ERROR: no available space for control client window";
        XCloseDisplay(display);
        return false;
    }
    const bool positioned = moveResizeWindow(display, static_cast<Window>(windowId), x + WINDOW_RESERVED_WIDTH, 0, width, height, error,
                                              ON_POSITION_WINDOW_RIGHT_OF_WINDOW);
    XCloseDisplay(display);
    return positioned;
}
