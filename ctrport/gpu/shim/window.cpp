#include "pacing_controls/window_policy.hpp"
// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: SDL3 window for the Vulkan swapchain (Win32 surface).
// Adapted from bbport gpu/shim/window.cpp (GPL-2.0), without the text-entry and overlay parts.
#include <cstdlib>
#include <SDL3/SDL.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "sdl_window.h"

namespace Frontend {
bool PollControllerExit(SDL_Window* window);
void InitGamepads();
void GamepadEvent(const SDL_Event& event);
void UpdateGamepads();

WindowSDL::WindowSDL(s32 width_, s32 height_, const char* title) : width{width_}, height{height_} {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        UNREACHABLE_MSG("Failed to initialize SDL video: {}", SDL_GetError());
    }
    // ctrport: PlayStation controllers through SDL (controller_exit.cpp); XInput pads are read
    // by the runtime directly.
    InitGamepads();
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width_);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height_);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
    const char* fullscreen = std::getenv("CTR_GPU_FULLSCREEN");
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN,
                           !std::strcmp(ctrport::settings::pacing::setting("CTR_SETTINGS_WINDOW_MODE"), "original") && fullscreen && fullscreen[0] == '1');
    window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    ASSERT_MSG(window, "Failed to create window: {}", SDL_GetError());
    try { ctrport::settings::pacing::apply_window(window,width_,height_); }
    catch (...) { SDL_DestroyWindow(window); window=nullptr; throw; }

    const SDL_PropertiesID wp = SDL_GetWindowProperties(window);
    window_info.type = WindowSystemType::Windows;
    window_info.render_surface =
        SDL_GetPointerProperty(wp, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    width = w;
    height = h;
    LOG_INFO(Frontend, "Window {}x{}", w, h);
}

WindowSDL::~WindowSDL() {
    SDL_DestroyWindow(window);
}

bool WindowSDL::PollEvents() {
    if(PollControllerExit(window))is_open=false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED: {
            int w = 0, h = 0;
            SDL_GetWindowSizeInPixels(window, &w, &h);
            width = w;
            height = h;
            break;
        }
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            is_open = false;
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
        case SDL_EVENT_GAMEPAD_REMOVED:
            GamepadEvent(event);
            break;
        default:
            break;
        }
    }
    UpdateGamepads();
    return is_open;
}

} // namespace Frontend
