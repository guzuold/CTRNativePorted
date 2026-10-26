// SPDX-License-Identifier: GPL-2.0-only
// PlayStation controllers through SDL's HIDAPI drivers (DualShock 4, DualSense), without DS4Windows
// or Steam Input, for the Direct3D 11 renderer: the same reading, merge and exports as the GPU
// library's (ctrport/gpu/shim/controller_exit.cpp), polled on a thread of its own because this
// renderer has no SDL window. Xbox controllers stay on the runtime's XInput.
#define CTR_GPU_BUILD
#include <windows.h>
#include <Xinput.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include "../gpu/ctr_gpu.h"

static_assert(sizeof(CtrGpuGamepad) == sizeof(XINPUT_GAMEPAD), "CtrGpuGamepad keeps the XINPUT_GAMEPAD layout");

namespace {
std::mutex pad_lock;
std::vector<SDL_Gamepad*> pads;
CtrGpuGamepad pad_snapshot{};
int pad_count = 0;
std::vector<CtrGpuGamepadEntry> pad_entries;  // one per connected controller, for split screen

// Every SDL gamepad except Xbox ones, which the runtime reads through XInput: compatible
// DualShock 4 pads often report another vendor ID and an SDL type other than PS4.
bool playstation(SDL_Gamepad* pad) {
    const auto type = SDL_GetGamepadType(pad);
    return type != SDL_GAMEPAD_TYPE_XBOX360 && type != SDL_GAMEPAD_TYPE_XBOXONE;
}
CtrGpuGamepad read_pad(SDL_Gamepad* pad) {
    static const std::pair<SDL_GamepadButton, WORD> map[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, XINPUT_GAMEPAD_A}, {SDL_GAMEPAD_BUTTON_EAST, XINPUT_GAMEPAD_B},
        {SDL_GAMEPAD_BUTTON_WEST, XINPUT_GAMEPAD_X}, {SDL_GAMEPAD_BUTTON_NORTH, XINPUT_GAMEPAD_Y},
        {SDL_GAMEPAD_BUTTON_START, XINPUT_GAMEPAD_START}, {SDL_GAMEPAD_BUTTON_BACK, XINPUT_GAMEPAD_BACK},
        // the runtime maps XINPUT_GAMEPAD_BACK to the PS4 touch pad button
        {SDL_GAMEPAD_BUTTON_TOUCHPAD, XINPUT_GAMEPAD_BACK},
        {SDL_GAMEPAD_BUTTON_LEFT_STICK, XINPUT_GAMEPAD_LEFT_THUMB}, {SDL_GAMEPAD_BUTTON_RIGHT_STICK, XINPUT_GAMEPAD_RIGHT_THUMB},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, XINPUT_GAMEPAD_LEFT_SHOULDER}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, XINPUT_GAMEPAD_DPAD_UP}, {SDL_GAMEPAD_BUTTON_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_DOWN},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_LEFT}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, XINPUT_GAMEPAD_DPAD_RIGHT}};
    CtrGpuGamepad out{};
    for (const auto& [button, bit] : map)
        if (SDL_GetGamepadButton(pad, button)) out.buttons |= bit;
    // SDL sticks are -32768..32767 with Y down, triggers 0..32767; XInput has Y up, triggers 0..255
    const auto up = [](Sint16 y) { return static_cast<int16_t>(y == -32768 ? 32767 : -y); };
    const auto trigger = [](Sint16 v) { return static_cast<uint8_t>(std::clamp<int>(v, 0, 32767) * 255 / 32767); };
    out.left_x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    out.left_y = up(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
    out.right_x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
    out.right_y = up(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY));
    out.left_trigger = trigger(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    out.right_trigger = trigger(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    return out;
}
// same merge as the runtime's XInput slots: buttons OR-ed, larger trigger, larger stick deflection
void merge(CtrGpuGamepad& into, const CtrGpuGamepad& pad) {
    const auto stick = [](int16_t& x, int16_t& y, int16_t nx, int16_t ny) {
        if (static_cast<long long>(nx) * nx + static_cast<long long>(ny) * ny > static_cast<long long>(x) * x + static_cast<long long>(y) * y) {
            x = nx;
            y = ny;
        }
    };
    into.buttons |= pad.buttons;
    into.left_trigger = std::max(into.left_trigger, pad.left_trigger);
    into.right_trigger = std::max(into.right_trigger, pad.right_trigger);
    stick(into.left_x, into.left_y, pad.left_x, pad.left_y);
    stick(into.right_x, into.right_y, pad.right_x, pad.right_y);
}
void gamepad_event(const SDL_Event& event) {
    std::lock_guard<std::mutex> guard(pad_lock);
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        SDL_Gamepad* pad = SDL_OpenGamepad(event.gdevice.which);
        if (!pad) {
            std::fprintf(stderr, "ctrport: SDL gamepad open failed: %s\n", SDL_GetError());
            return;
        }
        const char* name = SDL_GetGamepadName(pad);
        const char* type = SDL_GetGamepadStringForType(SDL_GetGamepadType(pad));
        std::fprintf(stderr, "ctrport: SDL gamepad seen: %s type=%s vendor=%04x product=%04x\n", name ? name : "?",
                     type ? type : "unknown", SDL_GetGamepadVendor(pad), SDL_GetGamepadProduct(pad));
        // other controllers stay on XInput; an already open pad only had its reference count raised
        if (!playstation(pad) || std::find(pads.begin(), pads.end(), pad) != pads.end()) {
            SDL_CloseGamepad(pad);
            return;
        }
        pads.push_back(pad);
        std::fprintf(stderr, "ctrport: PlayStation controller connected: %s\n", name ? name : "?");
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        for (auto it = pads.begin(); it != pads.end(); ++it)
            if (SDL_GetGamepadID(*it) == event.gdevice.which) {
                std::fputs("ctrport: PlayStation controller disconnected\n", stderr);
                SDL_CloseGamepad(*it);
                pads.erase(it);
                break;
            }
    }
}
void update_gamepads() {
    std::lock_guard<std::mutex> guard(pad_lock);
    CtrGpuGamepad merged{};
    int count = 0;
    pad_entries.clear();
    for (auto* pad : pads)
        if (SDL_GamepadConnected(pad)) {
            const CtrGpuGamepad state = read_pad(pad);
            merge(merged, state);
            ++count;
            pad_entries.push_back({static_cast<uint32_t>(SDL_GetGamepadID(pad)), state});
        }
    pad_snapshot = merged;
    pad_count = count;
}
}  // namespace

// ctr_gpu_init: the SDL gamepad subsystem and its polling thread (every 4 ms).
void StartPads() {
    static std::once_flag once;
    std::call_once(once, [] {
        std::thread([] {
            SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
            if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
                std::fprintf(stderr, "ctrport: SDL gamepad subsystem unavailable: %s\n", SDL_GetError());
                return;
            }
            for (;;) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) gamepad_event(event);
                update_gamepads();
                SDL_Delay(4);
            }
        }).detach();
    });
}

extern "C" CTR_GPU_API int ctr_gpu_gamepad_state(CtrGpuGamepad* out) {
    std::lock_guard<std::mutex> guard(pad_lock);
    if (out) *out = pad_count ? pad_snapshot : CtrGpuGamepad{};
    return pad_count;
}
extern "C" CTR_GPU_API void ctr_gpu_gamepad_rumble(uint16_t low_frequency, uint16_t high_frequency) {
    std::lock_guard<std::mutex> guard(pad_lock);
    // the game sets a level and keeps it until it sets another; SDL needs a duration
    for (auto* pad : pads) SDL_RumbleGamepad(pad, low_frequency, high_frequency, (low_frequency || high_frequency) ? 10000 : 0);
}
extern "C" CTR_GPU_API int ctr_gpu_gamepad_list(CtrGpuGamepadEntry* out, int capacity) {
    std::lock_guard<std::mutex> guard(pad_lock);
    int written = 0;
    for (const auto& entry : pad_entries) {
        if (!out || written >= capacity) break;
        out[written++] = entry;
    }
    return written;
}
extern "C" CTR_GPU_API void ctr_gpu_gamepad_rumble_one(uint32_t id, uint16_t low_frequency, uint16_t high_frequency) {
    std::lock_guard<std::mutex> guard(pad_lock);
    for (auto* pad : pads)
        if (SDL_GetGamepadID(pad) == id) SDL_RumbleGamepad(pad, low_frequency, high_frequency, (low_frequency || high_frequency) ? 10000 : 0);
}
extern "C" CTR_GPU_API int ctr_gpu_gamepad_path(uint32_t id, char* out, int capacity) {
    std::lock_guard<std::mutex> guard(pad_lock);
    if (!out || capacity <= 0) return 0;
    out[0] = 0;
    for (auto* pad : pads)
        if (SDL_GetGamepadID(pad) == id) {
            const char* path = SDL_GetGamepadPath(pad);
            if (!path) return 0;
            const int length = static_cast<int>(std::min<std::size_t>(std::strlen(path), static_cast<std::size_t>(capacity - 1)));
            std::memcpy(out, path, static_cast<std::size_t>(length));
            out[length] = 0;
            return length;
        }
    return 0;
}
extern "C" CTR_GPU_API void ctr_gpu_gamepad_light(uint32_t id, uint8_t red, uint8_t green, uint8_t blue) {
    std::lock_guard<std::mutex> guard(pad_lock);
    for (auto* pad : pads)
        if (SDL_GetGamepadID(pad) == id) SDL_SetGamepadLED(pad, red, green, blue);
}
