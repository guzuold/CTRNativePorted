// SPDX-License-Identifier: GPL-2.0-only
// Read-only IGZ10 bitmap fonts from the user's retail archives. No bundled game assets.
#pragma once
#include "xbox_prepare.hpp"
#include <memory>

namespace ctrport::launcher_font {
using namespace Gdiplus;
using Bytes = xbox_prepare::Bytes;
struct Glyph {
    RectF source;
    float advance;
};
struct Font {
    std::vector<uint8_t> pixels;
    std::unique_ptr<Bitmap> atlas;
    std::map<wchar_t, Glyph> glyphs;
    float height = 0;
    bool draw(Graphics& gr, const std::wstring& text, float size, const RectF& box, ARGB color,
              StringAlignment align) const {
        if (text.empty()) return true;
        // Unsupported Unicode stays readable through the launcher's system-font path.
        for (wchar_t ch : text)
            if ((ch >= 0x590 && ch <= 0x8ff) || (ch >= 0xfb1d && ch <= 0xfeff) ||
                (ch != L'\n' && ch != L'\r' && !glyphs.count(ch)))
                return false;
        float scale = size * 1.2f / height;
        const auto width = [&](const std::wstring& s) {
            float n = 0;
            for (auto ch : s) n += glyphs.at(ch).advance * scale;
            return n;
        };
        std::vector<std::wstring> lines;
        std::wstring line;
        const bool wrap = text.find(L'\n') != std::wstring::npos;
        // a single line too wide (longer words in other languages) is drawn smaller, down to 65%
        if (!wrap) {
            const float w = width(text);
            if (w > box.Width) scale *= std::max(0.65f, box.Width / w);
        }
        for (wchar_t ch : text) {
            if (ch == L'\r') continue;
            if (ch == L'\n') {
                lines.push_back(line);
                line.clear();
                continue;
            }
            line += ch;
            if (wrap && line.size() > 1 && width(line) > box.Width) {
                const size_t space = line.find_last_of(L' ');
                if (space != std::wstring::npos && space > 0) {
                    lines.push_back(line.substr(0, space));
                    line.erase(0, space + 1);
                } else {
                    lines.push_back(line.substr(0, line.size() - 1));
                    line = ch;
                }
            }
        }
        lines.push_back(line);
        const float line_height = height * scale;
        const size_t max_lines = size_t(std::max(1.0f, std::floor(box.Height / line_height)));
        const bool truncated = lines.size() > max_lines;
        if (truncated) lines.resize(max_lines);
        for (size_t i = 0; i < lines.size(); ++i) {
            auto& s = lines[i];
            if (width(s) > box.Width || (truncated && i + 1 == lines.size())) {
                if (!glyphs.count(L'.')) return false;
                const float dots = width(L"...");
                while (!s.empty() && width(s) + dots > box.Width) s.pop_back();
                s += L"...";
            }
        }
        Color tint(color);
        ColorMatrix matrix = {{{tint.GetR() / 255.0f, 0, 0, 0, 0},
                               {0, tint.GetG() / 255.0f, 0, 0, 0},
                               {0, 0, tint.GetB() / 255.0f, 0, 0},
                               {0, 0, 0, tint.GetA() / 255.0f, 0},
                               {0, 0, 0, 0, 1}}};
        ImageAttributes attr;
        attr.SetColorMatrix(&matrix);
        const auto state = gr.Save();
        gr.SetClip(box, CombineModeIntersect);
        gr.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        gr.SetPixelOffsetMode(PixelOffsetModeHalf);
        float y = box.Y + (box.Height - line_height * float(lines.size())) / 2;
        for (const auto& s : lines) {
            float x = box.X;
            if (align == StringAlignmentCenter)
                x += (box.Width - width(s)) / 2;
            else if (align == StringAlignmentFar)
                x += box.Width - width(s);
            for (wchar_t ch : s) {
                const auto& g = glyphs.at(ch);
                if (ch != L' ')
                    gr.DrawImage(atlas.get(), RectF(x, y, g.source.Width * scale, g.source.Height * scale), g.source.X,
                                 g.source.Y, g.source.Width, g.source.Height, UnitPixel, &attr);
                x += g.advance * scale;
            }
            y += line_height;
        }
        gr.Restore(state);
        return true;
    }
};
inline std::unique_ptr<Font> decode(const Bytes& d) {
    const auto bad = [] { throw std::runtime_error("Unsupported or damaged retail bitmap font"); };
    const auto range = [&](size_t p, size_t n) {
        if (p > d.size() || n > d.size() - p) bad();
    };
    const auto u = [&](size_t p) {
        range(p, 4);
        return xbox_prepare::rd32(d.data() + p);
    };
    const auto f = [&](size_t p) {
        range(p, 4);
        float v;
        std::memcpy(&v, d.data() + p, 4);
        if (!std::isfinite(v)) bad();
        return v;
    };
    if (d.size() > 32 * 1024 * 1024 || u(0) != 0x49475a01 || u(4) != 10 || u(12) != 8) bad();
    std::vector<std::pair<size_t, size_t>> chunks;
    for (size_t p = 20; p < 0x224; p += 16) {
        const size_t at = u(p + 4), n = u(p + 8);
        if (!at) break;
        range(at, n);
        chunks.emplace_back(at, n);
    }
    if (chunks.size() < 3) bad();
    const auto ptr = [&](uint32_t v, size_t n) {
        const size_t c = (v >> 27) + 1, off = v & 0x7ffffff;
        if (c >= chunks.size() || off > chunks[c].second || n > chunks[c].second - off) bad();
        return chunks[c].first + off;
    };
    struct Fixup {
        size_t at, size;
        uint32_t count;
    };
    std::map<std::string, Fixup> fixups;
    size_t at = chunks[0].first, end = at + chunks[0].second;
    for (uint32_t i = 0; i < u(16); ++i) {
        range(at, 16);
        const size_t n = u(at + 8), head = u(at + 12);
        if (n < 16 || head < 16 || head > n || at > end || n > end - at) bad();
        fixups[std::string(reinterpret_cast<const char*>(d.data() + at), 4)] = {at + head, n - head, u(at + 4)};
        at += n;
    }
    if (!fixups.count("TMET") || !fixups.count("RVTB")) bad();
    const auto tm = fixups.at("TMET"), rv = fixups.at("RVTB");
    std::vector<std::string> types;
    at = tm.at;
    for (uint32_t i = 0; i < tm.count; ++i) {
        const size_t begin = at;
        while (at < tm.at + tm.size && d[at]) ++at;
        if (at == tm.at + tm.size) bad();
        types.emplace_back(reinterpret_cast<const char*>(d.data() + begin), at - begin);
        ++at;
        if (at < tm.at + tm.size && !d[at]) ++at;
    }
    std::map<size_t, std::string> objects;
    uint64_t prev = 0, delta = 0;
    unsigned shift = 0;
    for (size_t i = 0; i < rv.size && objects.size() < rv.count; ++i) {
        for (uint8_t nib : {uint8_t(d[rv.at + i] & 15), uint8_t(d[rv.at + i] >> 4)}) {
            if (shift > 30) bad();
            delta |= uint64_t(nib & 7) << shift;
            shift += 3;
            if (!(nib & 8)) {
                prev += delta * 4;
                delta = 0;
                shift = 0;
                if (prev > UINT32_MAX) bad();
                const size_t p = ptr(uint32_t(prev), 4);
                if (u(p) >= types.size()) bad();
                objects.emplace(p, types[u(p)]);
                if (objects.size() == rv.count) break;
            }
        }
    }
    if (objects.size() != rv.count) bad();
    size_t image = 0, font = 0;
    for (const auto& [p, t] : objects) {
        if (t == "igImage2") {
            if (image) bad();
            image = p;
        }
        if (t == "igBitmapFont") {
            if (font) bad();
            font = p;
        }
    }
    if (!image || !font) bad();
    range(image, 72);
    range(font, 0x60);
    const uint32_t w = xbox_prepare::rd16(d.data() + image + 24), h = xbox_prepare::rd16(d.data() + image + 26);
    if (!w || !h || w > 4096 || h > 4096 || w % 8) bad();
    const size_t bytes = size_t(w) * ((h + 7) & ~7u) * 4;
    if (u(image + 56) != bytes) bad();
    const size_t pixels = ptr(u(image + 64), bytes);
    auto result = std::make_unique<Font>();
    result->height = f(font + 24);
    if (result->height < 1 || result->height > 512) bad();
    const size_t list = ptr(u(font + 72), 40);
    if (!objects.count(list) || objects.at(list) != "igCharMetricsList") bad();
    const uint32_t count = u(list + 16);
    if (!count || count > 65536) bad();
    const size_t table = ptr(u(list + 32), size_t(count) * 8);
    for (uint32_t i = 0; i < count; ++i) {
        const size_t p = ptr(u(table + size_t(i) * 8), 72);
        if (!objects.count(p) || objects.at(p) != "igCharMetrics") bad();
        const uint32_t code = u(p + 12);
        if (code > 65535) bad();
        const float x = std::round(f(p + 52) * w), y = std::round(f(p + 56) * h), right = std::round(f(p + 60) * w),
                    bottom = std::round(f(p + 64) * h), advance = f(p + 24);
        if (x < 0 || y < 0 || right <= x || bottom <= y || right > w || bottom > h || advance < 0 || advance > w) bad();
        result->glyphs.emplace(wchar_t(code), Glyph{RectF(x, y, right - x, bottom - y), advance});
    }
    // These retail text atlases use the same 8x8 Morton-tiled BGRA8 layout as button fonts.
    result->pixels.resize(size_t(w) * h * 4);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t morton = 0;
            for (unsigned b = 0; b < 3; ++b) morton |= ((x >> b) & 1) << (2 * b) | ((y >> b) & 1) << (2 * b + 1);
            const size_t source = pixels + (((y / 8) * (w / 8) + x / 8) * 64 + morton) * 4;
            std::memcpy(result->pixels.data() + (size_t(y) * w + x) * 4, d.data() + source, 4);
        }
    result->atlas = std::make_unique<Bitmap>(INT(w), INT(h), INT(w * 4), PixelFormat32bppARGB, result->pixels.data());
    if (result->atlas->GetLastStatus() != Ok) bad();
    return result;
}
struct Set {
    std::unique_ptr<Font> title, body;
    std::string diagnostics;
};
inline Set load(const std::vector<std::filesystem::path>& sources) {
    Set result;
    for (const auto& path : sources) {
        if (result.title && result.body) break;
        try {
            xbox_prepare::Archive archive(path, true);
            for (const auto& e : archive.files) {
                const auto name = xbox_prepare::lower(e.path);
                auto* target = name == "fonts/zoinks.igz"              ? &result.title
                               : name == "fonts/proximanova-title.igz" ? &result.body
                                                                       : nullptr;
                if (!target || *target) continue;
                try {
                    if (e.size > 32 * 1024 * 1024) throw std::runtime_error("Font too large");
                    *target = decode(archive.extract(e));
                    result.diagnostics += path.string() + " : " + e.path + " loaded (" +
                                          std::to_string((*target)->glyphs.size()) + " glyphs)\n";
                } catch (const std::exception& ex) {
                    result.diagnostics += path.string() + " : " + e.path + " : " + ex.what() + "\n";
                }
            }
        } catch (const std::exception& ex) {
            result.diagnostics += path.string() + " : " + ex.what() + "\n";
        }
    }
    if (!result.title) result.diagnostics += "Title: system font fallback\n";
    if (!result.body) result.diagnostics += "Body: system font fallback\n";
    return result;
}
} // namespace ctrport::launcher_font
