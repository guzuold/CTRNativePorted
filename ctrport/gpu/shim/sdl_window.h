// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: the game window used by the shadPS4 presenter. Created by ctr_gpu_init on
// its own host thread, which also pumps window events (see window.cpp).
// Adapted from bbport gpu/shim/sdl_window.h (GPL-2.0).
#pragma once
#include <atomic>
#include "common/types.h"

struct SDL_Window;

namespace Frontend {

enum class WindowSystemType : u8 { Headless, Windows, X11, Wayland, Metal };

struct WindowSystemInfo {
    void* display_connection = nullptr;
    void* render_surface = nullptr;
    float render_surface_scale = 1.0f;
    WindowSystemType type = WindowSystemType::Headless;
};

class WindowSDL {
public:
    WindowSDL(s32 width, s32 height, const char* title);
    ~WindowSDL();
    s32 GetWidth() const {
        return width.load(std::memory_order_relaxed);
    }
    s32 GetHeight() const {
        return height.load(std::memory_order_relaxed);
    }
    SDL_Window* GetSDLWindow() const {
        return window;
    }
    WindowSystemInfo GetWindowInfo() const {
        return window_info;
    }
    bool IsOpen() const {
        return is_open.load(std::memory_order_relaxed);
    }
    /// Processes pending window events. Returns false once the user closed the window.
    bool PollEvents();

private:
    std::atomic<s32> width, height;
    std::atomic<bool> is_open{true};
    SDL_Window* window{};
    WindowSystemInfo window_info{};
};

} // namespace Frontend
