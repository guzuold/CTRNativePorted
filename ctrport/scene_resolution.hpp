#include "settings/store.hpp"
#include <cstdio>
// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <windows.h>
namespace ctrport {
// Zero preserves the legacy standard/Neo/native-4K switches. Explicit presets
// use the verified Neo scale site, independently of the host swapchain.
inline int scene_height_selected() {
    static const int height = [] {
        if (const auto* document = settings::runtime_snapshot()) {
            const int selected = settings::scene_height(*document);
            std::printf("{\"scope\":\"settings_scene_requested\",\"schema\":1,\"height\":%d,\"application\":\"restart\"}\n", selected);
            return selected;
        }
        char value[32]{};
        SetLastError(ERROR_SUCCESS);
        const auto count = GetEnvironmentVariableA("CTR_SCENE_HEIGHT", value, sizeof(value));
        if (!count && GetLastError() == ERROR_ENVVAR_NOT_FOUND) return 0;
        if (!count || count >= sizeof(value)) throw std::runtime_error("Invalid CTR_SCENE_HEIGHT");
        char* end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        if (!*value || *end) throw std::runtime_error("Invalid CTR_SCENE_HEIGHT");
        for (int preset : {720, 900, 1080, 1440, 1800, 2160})
            if (parsed == preset) return preset;
        throw std::runtime_error("CTR_SCENE_HEIGHT must be 720, 900, 1080, 1440, 1800 or 2160");
    }();
    return height;
}
}
