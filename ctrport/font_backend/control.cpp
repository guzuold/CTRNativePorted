// SPDX-License-Identifier: GPL-2.0-only
// Host FreeType integration control. No PS4 service or guest handle is fabricated.
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <cstdint>
extern "C" int ctr_font_backend_initialize();
extern "C" bool ctr_font_backend_ready();
extern "C" bool ctr_font_backend_opentype_ready();
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    if (ctr_font_backend_opentype_ready() || ctr_font_backend_ready() || ctr_font_backend_initialize() != 0 ||
        !ctr_font_backend_ready() || ctr_font_backend_initialize() != 0)
        return 9;
    if (!ctr_font_backend_opentype_ready()) return 10;
    FT_Library library = nullptr;
    if (FT_Init_FreeType(&library)) return 3;
    FT_Face face = nullptr;
    if (FT_New_Face(library, argv[1], 0, &face)) {
        FT_Done_FreeType(library);
        return 4;
    }
    int result = 0;
    if (FT_Set_Pixel_Sizes(face, 0, 32) || FT_Load_Char(face, 'A', FT_LOAD_RENDER))
        result = 5;
    else {
        const auto& bitmap = face->glyph->bitmap;
        std::uint64_t sum = 0;
        if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.pitch < 0)
            result = 6;
        else
            for (unsigned y = 0; y < bitmap.rows; ++y)
                for (unsigned x = 0; x < bitmap.width; ++x)
                    sum += bitmap.buffer[y * static_cast<unsigned>(bitmap.pitch) + x];
        if (!bitmap.width || !bitmap.rows || !sum || face->glyph->advance.x <= 0) result = 7;
        if (!result)
            std::cout << "{\"scope\":\"host_freetype_glyph_control\",\"glyph\":65,\"pixel_height\":32,\"width\":"
                      << bitmap.width << ",\"rows\":" << bitmap.rows << ",\"coverage_sum\":" << sum
                      << ",\"advance_26_6\":" << face->glyph->advance.x << ",\"guest_binding\":false}\n";
    }
    // Confirm backend rejects an invalid face index without producing a usable handle.
    FT_Face invalid = nullptr;
    if (FT_New_Face(library, argv[1], 0x7fffffff, &invalid) == 0) {
        FT_Done_Face(invalid);
        result = 8;
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    return result;
}
