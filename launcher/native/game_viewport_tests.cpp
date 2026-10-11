#include "game_viewport.hpp"
#include <cstdlib>
#include <iostream>
static void check(bool value) { if (!value) std::abort(); }
int main() {
    using jfg::frontend::game_viewport;
    auto wide = game_viewport(1920, 1080);
    check(wide.x == 240 && wide.y == 0 && wide.width == 1440 && wide.height == 1080);
    auto classic = game_viewport(640, 480);
    check(classic.x == 0 && classic.y == 0 && classic.width == 640 && classic.height == 480);
    auto tall = game_viewport(600, 900);
    check(tall.x == 0 && tall.y == 225 && tall.width == 600 && tall.height == 450);
    auto ultrawide = game_viewport(3440, 1440);
    check(ultrawide.x == 760 && ultrawide.width == 1920 && ultrawide.height == 1440);
    for (int w = 1; w < 2048; w += 17) for (int h = 1; h < 1600; h += 19) {
        auto v = game_viewport(w, h);
        check(v.x >= 0 && v.y >= 0 && v.width > 0 && v.height > 0);
        check(v.x + v.width <= w && v.y + v.height <= h);
        check(std::abs(w - v.width - 2*v.x) <= 1 && std::abs(h - v.height - 2*v.y) <= 1);
        check(std::abs(3*v.width - 4*v.height) <= 4);
    }
    auto minimized = game_viewport(0, 0);
    check(minimized.width == 1 && minimized.height == 1);
    std::cout << "Game viewport checks passed (windowed, fullscreen, ultrawide, portrait, odd sizes).\n";
}
