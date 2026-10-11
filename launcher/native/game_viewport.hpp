#pragma once
#include <algorithm>
#include <cstdint>
namespace jfg::frontend {
struct GameViewport { int x, y, width, height; };
// Keep a stable television-shaped canvas. A cinematic's narrower VI scanout
// belongs inside this canvas; resizing the launcher must not zoom it away.
inline GameViewport game_viewport(int width, int height) noexcept {
    width = (std::max)(1, width); height = (std::max)(1, height);
    int w = width, h = height;
    if (std::int64_t(width) * 3 > std::int64_t(height) * 4)
        w = static_cast<int>(std::int64_t(height) * 4 / 3);
    else h = static_cast<int>(std::int64_t(width) * 3 / 4);
    w = (std::max)(1, w); h = (std::max)(1, h);
    return {(width - w) / 2, (height - h) / 2, w, h};
}
}
