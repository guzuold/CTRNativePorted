// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <algorithm>
#include <cmath>
namespace nitro {
constexpr float width = 1280, height = 720, header = 65, footer = 664;
constexpr float zoom = 1.4f;
constexpr int samples = 2; // 2x per axis, resolved once for every custom-painted surface.
constexpr float duration_ms = 280;
constexpr float nav_left[5] = {286, 391, 495, 607, 724};
constexpr float nav_width[5] = {100, 100, 100, 114, 150};
inline float nav_center(float selected) {
    const int left = std::clamp(int(selected), 0, 4), right = std::min(left + 1, 4);
    const float t = selected - left;
    return (nav_left[left] + nav_width[left] * .5f) * (1 - t) + (nav_left[right] + nav_width[right] * .5f) * t;
}
struct Crop {
    float x, y, w, h;
};
inline Crop crop(float iw, float ih, float dw, float dh) {
    if (iw <= 0 || ih <= 0 || dw <= 0 || dh <= 0) return {};
    const float scale = std::max(dw / iw, dh / ih) * zoom;
    const float w = dw / scale, h = dh / scale;
    return {(iw - w) * .5f, ih - h, w, h};
}
inline float ease(float p) {
    p = std::clamp(p, 0.f, 1.f);
    return p * p * (3 - 2 * p);
}
inline float page_x(float x) {
    return 40 + (x - 296) * 1.73f;
}
inline float page_y(float y) {
    return y + 40;
}
inline float page_w(float w) {
    return w * 1.73f;
}
}
