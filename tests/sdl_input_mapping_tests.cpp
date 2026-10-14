// Checks the SDL-to-Windows-virtual-key translation used by the Linux window
// layer. Controller mapping and pc-input files store Windows virtual-key
// codes, so these values are a file-format contract shared with Windows.
#include "jfg/platform/sdl_live_window.hpp"

#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(const SDL_Scancode scancode, const int expected, const std::string_view name) {
    const int actual = jfg::platform::virtual_key_from_scancode(scancode);
    if (actual != expected) {
        ++failures;
        std::cerr << "FAIL: " << name << " mapped to 0x" << std::hex << actual
                  << ", expected 0x" << expected << std::dec << '\n';
    }
}

}  // namespace

int main() {
    expect(SDL_SCANCODE_A, 0x41, "A");
    expect(SDL_SCANCODE_Z, 0x5A, "Z");
    expect(SDL_SCANCODE_1, 0x31, "1");
    expect(SDL_SCANCODE_9, 0x39, "9");
    expect(SDL_SCANCODE_0, 0x30, "0");
    expect(SDL_SCANCODE_F1, 0x70, "F1");
    expect(SDL_SCANCODE_F12, 0x7B, "F12");
    expect(SDL_SCANCODE_RETURN, 0x0D, "Return");
    expect(SDL_SCANCODE_KP_ENTER, 0x0D, "Keypad Enter");
    expect(SDL_SCANCODE_ESCAPE, 0x1B, "Escape");
    expect(SDL_SCANCODE_SPACE, 0x20, "Space");
    expect(SDL_SCANCODE_LEFT, 0x25, "Left");
    expect(SDL_SCANCODE_UP, 0x26, "Up");
    expect(SDL_SCANCODE_RIGHT, 0x27, "Right");
    expect(SDL_SCANCODE_DOWN, 0x28, "Down");
    expect(SDL_SCANCODE_LSHIFT, 0x10, "Left Shift");
    expect(SDL_SCANCODE_RSHIFT, 0x10, "Right Shift");
    expect(SDL_SCANCODE_LCTRL, 0x11, "Left Control");
    expect(SDL_SCANCODE_RALT, 0x12, "Right Alt");
    expect(SDL_SCANCODE_KP_0, 0x60, "Keypad 0");
    expect(SDL_SCANCODE_KP_9, 0x69, "Keypad 9");
    expect(SDL_SCANCODE_GRAVE, 0xC0, "Grave");
    expect(SDL_SCANCODE_UNKNOWN, 0, "unknown key");

    const auto button = [](const std::uint8_t sdl, const int expected, const std::string_view name) {
        if (jfg::platform::virtual_key_from_mouse_button(sdl) != expected) {
            ++failures;
            std::cerr << "FAIL: mouse " << name << '\n';
        }
    };
    button(SDL_BUTTON_LEFT, 0x01, "left");
    button(SDL_BUTTON_RIGHT, 0x02, "right");
    button(SDL_BUTTON_MIDDLE, 0x04, "middle");
    button(SDL_BUTTON_X1, 0x05, "x1");
    button(SDL_BUTTON_X2, 0x06, "x2");

    if (failures != 0)
        return 1;
    std::cout << "sdl input mapping tests passed\n";
    return 0;
}
