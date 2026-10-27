// SPDX-License-Identifier: GPL-2.0-only
// Resident FreeType owner; no PS4 font handles or outputs fabricated.
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <mutex>
namespace {
struct Backend {
    std::mutex mutex;
    FT_Library library = nullptr;
    ~Backend() {
        if (library) FT_Done_FreeType(library);
    }
};
Backend backend;
}
extern "C" int ctr_font_backend_initialize() {
    std::lock_guard<std::mutex> lock(backend.mutex);
    if (backend.library) return 0;
    FT_Library value = nullptr;
    const auto result = FT_Init_FreeType(&value);
    if (result) return result;
    backend.library = value;
    return 0;
}
extern "C" bool ctr_font_backend_ready() {
    std::lock_guard<std::mutex> lock(backend.mutex);
    return backend.library != nullptr;
}

// OpenType capability is backed by actual registered FreeType drivers.
extern "C" bool ctr_font_backend_opentype_ready() {
    std::lock_guard<std::mutex> lock(backend.mutex);
    return backend.library && FT_Get_Module(backend.library, "sfnt") && FT_Get_Module(backend.library, "truetype") &&
           FT_Get_Module(backend.library, "cff");
}
