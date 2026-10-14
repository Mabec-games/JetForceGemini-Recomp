#pragma once

#include <cstdint>

namespace jfg::platform {

// Native window description handed to the RT64 shell on non-Windows hosts.
// On Linux RT64's Vulkan backend creates its surface from an Xlib window, so
// this carries the X11 Display* and Window id. Windows passes its HWND
// directly and does not use this type.
struct NativeWindowHandle {
    void* display = nullptr;
    std::uintptr_t window = 0U;
};

}  // namespace jfg::platform
