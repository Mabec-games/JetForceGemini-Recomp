#pragma once

// SDL2 host window, keyboard/mouse and gamepad layer for the live runtime on
// Linux. It produces exactly what the Win32 window procedure produces:
// Windows virtual-key codes in a WindowInputMailbox, relative mouse motion
// while the game has captured the pointer, and an XInput-ordered gamepad
// sample. Controller mappings, pc-input files and the launcher therefore keep
// one key-code vocabulary on every platform.

#include <SDL.h>
#include <SDL_syswm.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <mutex>
#include <string_view>
#include <thread>

#include "jfg/platform/native_window.hpp"
#include "jfg/runtime/window_input_mailbox.hpp"

#if defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace jfg::platform {

namespace vk {
inline constexpr int back = 0x08, tab = 0x09, enter = 0x0D, shift = 0x10,
                     control = 0x11, menu = 0x12, pause = 0x13, caps = 0x14,
                     escape = 0x1B, space = 0x20, prior = 0x21, next = 0x22,
                     end = 0x23, home = 0x24, left = 0x25, up = 0x26,
                     right = 0x27, down = 0x28, snapshot = 0x2C, insert = 0x2D,
                     del = 0x2E, lwin = 0x5B, rwin = 0x5C, apps = 0x5D,
                     numpad0 = 0x60, multiply = 0x6A, add = 0x6B,
                     subtract = 0x6D, decimal = 0x6E, divide = 0x6F, f1 = 0x70,
                     numlock = 0x90, scroll = 0x91, oem_1 = 0xBA,
                     oem_plus = 0xBB, oem_comma = 0xBC, oem_minus = 0xBD,
                     oem_period = 0xBE, oem_2 = 0xBF, oem_3 = 0xC0,
                     oem_4 = 0xDB, oem_5 = 0xDC, oem_6 = 0xDD, oem_7 = 0xDE;
inline constexpr int lbutton = 0x01, rbutton = 0x02, mbutton = 0x04,
                     xbutton1 = 0x05, xbutton2 = 0x06;
}  // namespace vk

// Physical-key translation (layout independent, like the Win32 path which
// reports the key the mapping UI recorded). Returns 0 for unmapped keys.
[[nodiscard]] inline int virtual_key_from_scancode(const SDL_Scancode scancode) noexcept {
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z)
        return 'A' + (scancode - SDL_SCANCODE_A);
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9)
        return '1' + (scancode - SDL_SCANCODE_1);
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12)
        return vk::f1 + (scancode - SDL_SCANCODE_F1);
    if (scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_9)
        return vk::numpad0 + 1 + (scancode - SDL_SCANCODE_KP_1);
    switch (scancode) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_KP_0: return vk::numpad0;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER: return vk::enter;
    case SDL_SCANCODE_ESCAPE: return vk::escape;
    case SDL_SCANCODE_BACKSPACE: return vk::back;
    case SDL_SCANCODE_TAB: return vk::tab;
    case SDL_SCANCODE_SPACE: return vk::space;
    case SDL_SCANCODE_MINUS: return vk::oem_minus;
    case SDL_SCANCODE_EQUALS: return vk::oem_plus;
    case SDL_SCANCODE_LEFTBRACKET: return vk::oem_4;
    case SDL_SCANCODE_RIGHTBRACKET: return vk::oem_6;
    case SDL_SCANCODE_BACKSLASH: return vk::oem_5;
    case SDL_SCANCODE_SEMICOLON: return vk::oem_1;
    case SDL_SCANCODE_APOSTROPHE: return vk::oem_7;
    case SDL_SCANCODE_GRAVE: return vk::oem_3;
    case SDL_SCANCODE_COMMA: return vk::oem_comma;
    case SDL_SCANCODE_PERIOD: return vk::oem_period;
    case SDL_SCANCODE_SLASH: return vk::oem_2;
    case SDL_SCANCODE_CAPSLOCK: return vk::caps;
    case SDL_SCANCODE_PRINTSCREEN: return vk::snapshot;
    case SDL_SCANCODE_SCROLLLOCK: return vk::scroll;
    case SDL_SCANCODE_PAUSE: return vk::pause;
    case SDL_SCANCODE_INSERT: return vk::insert;
    case SDL_SCANCODE_HOME: return vk::home;
    case SDL_SCANCODE_PAGEUP: return vk::prior;
    case SDL_SCANCODE_DELETE: return vk::del;
    case SDL_SCANCODE_END: return vk::end;
    case SDL_SCANCODE_PAGEDOWN: return vk::next;
    case SDL_SCANCODE_RIGHT: return vk::right;
    case SDL_SCANCODE_LEFT: return vk::left;
    case SDL_SCANCODE_DOWN: return vk::down;
    case SDL_SCANCODE_UP: return vk::up;
    case SDL_SCANCODE_NUMLOCKCLEAR: return vk::numlock;
    case SDL_SCANCODE_KP_DIVIDE: return vk::divide;
    case SDL_SCANCODE_KP_MULTIPLY: return vk::multiply;
    case SDL_SCANCODE_KP_MINUS: return vk::subtract;
    case SDL_SCANCODE_KP_PLUS: return vk::add;
    case SDL_SCANCODE_KP_PERIOD: return vk::decimal;
    case SDL_SCANCODE_APPLICATION: return vk::apps;
    // WM_KEYDOWN reports the side-neutral codes.
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT: return vk::shift;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL: return vk::control;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT: return vk::menu;
    case SDL_SCANCODE_LGUI: return vk::lwin;
    case SDL_SCANCODE_RGUI: return vk::rwin;
    default: return 0;
    }
}

[[nodiscard]] inline int virtual_key_from_mouse_button(const std::uint8_t button) noexcept {
    switch (button) {
    case SDL_BUTTON_LEFT: return vk::lbutton;
    case SDL_BUTTON_RIGHT: return vk::rbutton;
    case SDL_BUTTON_MIDDLE: return vk::mbutton;
    case SDL_BUTTON_X1: return vk::xbutton1;
    case SDL_BUTTON_X2: return vk::xbutton2;
    default: return 0;
    }
}

// SDL's subsystem initialisation is not reentrant; the window thread and the
// guest thread both need subsystems, so serialise every call.
[[nodiscard]] inline bool init_sdl_subsystem(const std::uint32_t flags) noexcept {
    static std::mutex mutex;
    const std::lock_guard lock(mutex);
    return SDL_InitSubSystem(flags) == 0;
}

class SdlLiveWindow final {
public:
    SdlLiveWindow(WindowInputMailbox& input, std::atomic<bool>& captured,
                  const bool visible)
        : input_(input), captured_(captured) {
        std::promise<void> ready;
        auto initialized = ready.get_future();
        owner_ = std::thread([this, visible, &ready] { run(visible, ready); });
        initialized.get();
    }

    ~SdlLiveWindow() {
        stop_.store(true);
        if (owner_.joinable())
            owner_.join();
    }

    SdlLiveWindow(const SdlLiveWindow&) = delete;
    SdlLiveWindow& operator=(const SdlLiveWindow&) = delete;

    // Null when the window could not be created.
    [[nodiscard]] void* native_handle() const noexcept {
        return handle_.window == 0U ? nullptr : const_cast<NativeWindowHandle*>(&handle_);
    }
    [[nodiscard]] std::uint32_t thread_id() const noexcept { return thread_id_; }
    [[nodiscard]] bool relative_mouse_available() const noexcept { return mouse_ok_; }

    // Keep the client area at width:height (the VI's presentation aspect).
    void set_aspect(const std::uint32_t width, const std::uint32_t height) noexcept {
        if (width == 0U || height == 0U)
            return;
        aspect_width_.store(width);
        aspect_height_.store(height);
        aspect_dirty_.store(true);
    }

private:
    void run(const bool visible, std::promise<void>& ready) {
#if defined(__linux__)
        thread_id_ = static_cast<std::uint32_t>(::syscall(SYS_gettid));
#endif
        // RT64's Vulkan backend creates its surface from an Xlib window, so
        // prefer X11 (XWayland on Wayland sessions) unless the user chose.
        if (std::getenv("SDL_VIDEODRIVER") == nullptr)
            SDL_SetHint("SDL_VIDEODRIVER", "x11");
        SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        if (init_sdl_subsystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            window_ = SDL_CreateWindow("Mabecs Jet Force Gemini Recomp",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 640, 480,
                SDL_WINDOW_RESIZABLE | (visible ? SDL_WINDOW_SHOWN : SDL_WINDOW_HIDDEN));
        }
        if (window_ != nullptr) {
            SDL_SysWMinfo info;
            SDL_VERSION(&info.version);
            if (SDL_GetWindowWMInfo(window_, &info) == SDL_TRUE &&
                info.subsystem == SDL_SYSWM_X11) {
                handle_.display = info.info.x11.display;
                handle_.window = static_cast<std::uintptr_t>(info.info.x11.window);
                mouse_ok_ = true;
                // The Linux launcher's Window/Full screen switch. F11 still toggles.
                const char* const start_fullscreen = std::getenv("JFG_START_FULLSCREEN");
                if (visible && start_fullscreen != nullptr && std::string_view(start_fullscreen) == "1")
                    SDL_SetWindowFullscreen(window_, static_cast<std::uint32_t>(SDL_WINDOW_FULLSCREEN_DESKTOP));
            } else {
                // Wayland-only sessions have no Xlib surface for RT64.
                SDL_DestroyWindow(window_);
                window_ = nullptr;
            }
        }
        ready.set_value();
        if (window_ == nullptr) {
            if (SDL_WasInit(SDL_INIT_VIDEO) != 0U)
                SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
            return;
        }
        loop();
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    }

    void loop() {
        bool relative = false;
        while (!stop_.load()) {
            SDL_Event event;
            if (SDL_WaitEventTimeout(&event, 8) != 0) {
                do {
                    handle(event);
                } while (SDL_PollEvent(&event) != 0);
            }
            const bool want = captured_.load();
            if (want != relative) {
                SDL_SetRelativeMouseMode(want ? SDL_TRUE : SDL_FALSE);
                SDL_ShowCursor(want ? SDL_DISABLE : SDL_ENABLE);
                relative = want;
            }
            apply_aspect();
        }
    }

    void handle(const SDL_Event& event) {
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            if (event.key.repeat != 0)
                return;
            const bool down = event.type == SDL_KEYDOWN;
            if (down && event.key.keysym.scancode == SDL_SCANCODE_F11) {
                toggle_fullscreen();
                return;
            }
            const int key = virtual_key_from_scancode(event.key.keysym.scancode);
            if (key > 0)
                input_.key(static_cast<std::size_t>(key), down);
            return;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            const int key = virtual_key_from_mouse_button(event.button.button);
            if (key > 0)
                input_.key(static_cast<std::size_t>(key), event.type == SDL_MOUSEBUTTONDOWN);
            return;
        }
        case SDL_MOUSEMOTION:
            if (captured_.load())
                input_.motion(event.motion.xrel, event.motion.yrel);
            return;
        case SDL_MOUSEWHEEL:
            input_.wheel(event.wheel.y * 120);
            return;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                captured_.store(false);
                input_.lose_focus();
            } else if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                input_.close();
            }
            return;
        case SDL_QUIT:
            input_.close();
            return;
        default:
            return;
        }
    }

    void toggle_fullscreen() noexcept {
        const bool full = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0U;
        SDL_SetWindowFullscreen(window_, full ? 0U : static_cast<std::uint32_t>(SDL_WINDOW_FULLSCREEN_DESKTOP));
    }

    void apply_aspect() noexcept {
        if (!aspect_dirty_.exchange(false))
            return;
        const std::uint32_t flags = SDL_GetWindowFlags(window_);
        if ((flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP |
                      SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) != 0U)
            return;
        int width = 0, height = 0;
        SDL_GetWindowSize(window_, &width, &height);
        if (height <= 0)
            return;
        const std::uint64_t numerator = aspect_width_.load();
        const std::uint64_t denominator = aspect_height_.load();
        const auto target = static_cast<int>(
            (static_cast<std::uint64_t>(height) * numerator + denominator / 2U) / denominator);
        if (target > 0 && target != width)
            SDL_SetWindowSize(window_, target, height);
    }

    WindowInputMailbox& input_;
    std::atomic<bool>& captured_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> aspect_dirty_{false};
    std::atomic<std::uint32_t> aspect_width_{4U}, aspect_height_{3U};
    NativeWindowHandle handle_{};
    SDL_Window* window_ = nullptr;
    bool mouse_ok_ = false;
    std::uint32_t thread_id_ = 0U;
    std::thread owner_;
};

// XInput-ordered gamepad access (A, B, X, Y, Back, Guide, Start, LS, RS, LB,
// RB, DPad up/down/left/right matches both XInput and SDL_GameControllerButton).
class SdlGamepads final {
public:
    struct State {
        std::array<bool, 15> buttons{};
        // lx, ly, rx, ry (down positive), left trigger, right trigger.
        std::array<int, 6> axes{};
    };

    // `device` is the Nth attached game controller, mirroring an XInput slot.
    [[nodiscard]] bool read(const int device, State& output) {
        if (device < 0 || device >= static_cast<int>(pads_.size()))
            return false;
        const std::lock_guard lock(mutex_);
        if (!ready_) {
            ready_ = init_sdl_subsystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
            if (!ready_)
                return false;
        }
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_scan_) {
            next_scan_ = now + std::chrono::milliseconds(500);
            rescan();
        }
        SDL_GameController* pad = pads_[static_cast<std::size_t>(device)];
        if (pad == nullptr)
            return false;
        SDL_GameControllerUpdate();
        for (std::size_t index = 0U; index < output.buttons.size(); ++index)
            output.buttons[index] = SDL_GameControllerGetButton(
                pad, static_cast<SDL_GameControllerButton>(index)) != 0U;
        const auto axis = [pad](const SDL_GameControllerAxis which) {
            return static_cast<int>(SDL_GameControllerGetAxis(pad, which));
        };
        output.axes = {axis(SDL_CONTROLLER_AXIS_LEFTX), axis(SDL_CONTROLLER_AXIS_LEFTY),
                       axis(SDL_CONTROLLER_AXIS_RIGHTX), axis(SDL_CONTROLLER_AXIS_RIGHTY),
                       axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT),
                       axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT)};
        return true;
    }

    ~SdlGamepads() {
        for (SDL_GameController*& pad : pads_) {
            if (pad != nullptr)
                SDL_GameControllerClose(pad);
            pad = nullptr;
        }
    }

private:
    void rescan() {
        // Drop unplugged pads, then fill free slots in joystick order.
        for (SDL_GameController*& pad : pads_) {
            if (pad != nullptr && SDL_GameControllerGetAttached(pad) == SDL_FALSE) {
                SDL_GameControllerClose(pad);
                pad = nullptr;
            }
        }
        const int count = SDL_NumJoysticks();
        for (int joystick = 0; joystick < count; ++joystick) {
            if (SDL_IsGameController(joystick) == SDL_FALSE)
                continue;
            const SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(joystick);
            bool open = false;
            for (SDL_GameController* pad : pads_)
                open |= pad != nullptr &&
                        SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad)) == id;
            if (open)
                continue;
            for (SDL_GameController*& slot : pads_) {
                if (slot == nullptr) {
                    slot = SDL_GameControllerOpen(joystick);
                    break;
                }
            }
        }
    }

    std::mutex mutex_;
    bool ready_ = false;
    std::array<SDL_GameController*, 4> pads_{};
    std::chrono::steady_clock::time_point next_scan_{};
};

}  // namespace jfg::platform
