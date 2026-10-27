// SPDX-License-Identifier: GPL-2.0-only
// Native port of tools/input_prompts/prepare_xbox.py, so a release needs no Python.
// Builds runtime/user/mods/xbox-default/<source SHA256>/<archive> from the user's own
// retail archive; the retail file is only read. The archive layout (IGA v13), the IGZ10
// font schema and the atlas fingerprints are the ones prepare_xbox.py already checks.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace Gdiplus {
using std::min;
using std::max;
}
#include <gdiplus.h>
#include "xbox_asset_overlay.hpp"
#include "lzma_raw.hpp"
#pragma comment(lib, "gdiplus.lib")

namespace ctrport::xbox_prepare {
namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

inline uint32_t rd32(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
inline uint16_t rd16(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, 2);
    return v;
}
inline void wr32(uint8_t* p, uint32_t v) {
    std::memcpy(p, &v, 4);
}
inline void fail(const std::string& what) {
    throw std::runtime_error("Xbox icons: " + what);
}

using Lzma = ctrport::lzma_raw::Decoder;

struct Entry {
    uint32_t index, offset, size, compression;
    std::string path;
};

// IGA v13 reader matching tools/input_prompts/ctr_nf_iga.py; reads only what it needs.
class Archive {
public:
    explicit Archive(const fs::path& path, bool compact = false) : file_(path, std::ios::binary) {
        if (!file_) fail("cannot open " + path.string());
        file_.seekg(0, std::ios::end);
        file_size_ = uint64_t(file_.tellg());
        Bytes header = read(0, 56);
        if (rd32(&header[0]) != 0x1A414749u || rd32(&header[4]) != 13) fail("not an IGA v13 archive: " + path.string());
        file_count = rd32(&header[12]);
        sector_size = rd32(&header[16]);
        const uint32_t large_n = rd32(&header[28]), medium_n = rd32(&header[32]), small_n = rd32(&header[36]);
        const uint32_t names_offset = rd32(&header[40]), names_size = rd32(&header[48]);
        const uint64_t tables_end = 56ull + file_count * 20ull + large_n * 4ull + medium_n * 2ull + small_n;
        if (compact &&
            (tables_end > 32 * 1024 * 1024 || names_size > 32 * 1024 * 1024 || uint64_t(file_count) * 4 > names_size))
            fail("archive directory too large");
        directory =
            read(0, size_t(compact ? tables_end : std::max<uint64_t>(tables_end, uint64_t(names_offset) + names_size)));
        const Bytes names = compact ? read(names_offset, names_size) : Bytes{};
        const auto& name_data = compact ? names : directory;
        const size_t names_base = compact ? 0 : names_offset;
        size_t at = 56 + size_t(file_count) * 20;
        for (uint32_t i = 0; i < large_n; ++i, at += 4) large_.push_back(rd32(&directory[at]));
        for (uint32_t i = 0; i < medium_n; ++i, at += 2) medium_.push_back(rd16(&directory[at]));
        small_.assign(directory.begin() + at, directory.begin() + at + small_n);
        for (uint32_t i = 0; i < file_count; ++i) {
            const uint8_t* d = &directory[56 + size_t(file_count) * 4 + size_t(i) * 16];
            const size_t name = names_base + rd32(&name_data[names_base + size_t(i) * 4]);
            const auto end_string = [&](size_t pos) {
                while (pos < name_data.size() && name_data[pos]) ++pos;
                if (pos >= name_data.size()) fail("archive name table");
                return pos;
            };
            const size_t full_end = end_string(name), short_end = end_string(full_end + 1);
            files.push_back({i, rd32(d), rd32(d + 8), rd32(d + 12),
                             std::string(name_data.begin() + full_end + 1, name_data.begin() + short_end)});
        }
    }
    Bytes extract(const Entry& e) {
        if (e.compression == 0xFFFFFFFFu) return read(e.offset, e.size);
        Bytes out(e.size);
        const uint32_t blocks = (e.size + 0x7FFF) / 0x8000, type = e.compression >> 28;
        for (uint32_t b = 0; b < blocks; ++b) {
            const uint32_t index = (e.compression & 0x0FFFFFFF) + b;
            uint32_t entry, mask, shift;
            if (e.size <= 0x7Full * sector_size) {
                if (index >= small_.size()) fail("block table");
                entry = small_[index];
                mask = 0x7F;
                shift = 7;
            } else if (e.size <= 0x7FFFull * sector_size) {
                if (index >= medium_.size()) fail("block table");
                entry = medium_[index];
                mask = 0x7FFF;
                shift = 15;
            } else {
                if (index >= large_.size()) fail("block table");
                entry = large_[index];
                mask = 0x7FFFFFFF;
                shift = 31;
            }
            const uint64_t source = uint64_t(e.offset) + uint64_t(entry & mask) * sector_size;
            const size_t out_at = size_t(b) * 0x8000, out_size = std::min<size_t>(0x8000, e.size - out_at);
            if ((entry >> shift) != 1) {
                Bytes raw = read(source, out_size);
                std::memcpy(&out[out_at], raw.data(), out_size);
                continue;
            }
            if (type != 2) fail("unsupported compression in " + e.path); // fonts use LZMA only
            const Bytes head = read(source, 9);
            const uint32_t packed = rd32(&head[0]);
            const Bytes payload = read(source + 9, packed);
            Lzma(payload.data(), payload.size(), head[4], rd32(&head[5])).decode(&out[out_at], out_size);
        }
        return out;
    }
    uint64_t size() const { return file_size_; }
    uint32_t file_count = 0, sector_size = 0;
    Bytes directory;
    std::vector<Entry> files;

private:
    size_t cstring_end(size_t at) const {
        while (at < directory.size() && directory[at]) ++at;
        if (at >= directory.size()) fail("archive name table");
        return at;
    }
    Bytes read(uint64_t at, size_t n) {
        if (at + n > file_size_) fail("archive read outside file");
        Bytes b(n);
        file_.clear();
        file_.seekg(std::streamoff(at));
        file_.read(reinterpret_cast<char*>(b.data()), std::streamsize(n));
        if (!file_) fail("archive read failed");
        return b;
    }
    std::ifstream file_;
    uint64_t file_size_ = 0;
    std::vector<uint32_t> large_;
    std::vector<uint16_t> medium_;
    std::vector<uint8_t> small_;
};

inline std::string sha256(const uint8_t* data, size_t size) {
    BCRYPT_ALG_HANDLE alg{};
    BCRYPT_HASH_HANDLE hash{};
    std::array<unsigned char, 32> digest{};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) fail("SHA256 provider");
    const bool ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
                    BCryptHashData(hash, const_cast<PUCHAR>(data), ULONG(size), 0) >= 0 &&
                    BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) fail("SHA256");
    std::string r;
    for (auto b : digest) {
        r += "0123456789abcdef"[b >> 4];
        r += "0123456789abcdef"[b & 15];
    }
    return r;
}
inline std::string sha256(const Bytes& b) {
    return sha256(b.data(), b.size());
}

// Fingerprints from ctrport/assets/xbox-default/retail_atlas.json.
constexpr const char* kBaseAtlas = "e360b4a23745a096cbc4f76dc16d3103c6ec6f799c38862edb3df944dd8f50cd";
constexpr const char* kUpdateAtlas[] = {"6c0b9319cd2bd0bfe07c4d7b8633d9aff71dd932a04f3df5766095f4f5b48dee",
                                        "5a509c8fb94dc40796669863a7e8c6941f4332c629df0e08de22cd3daa9a13c9"};

struct Icon {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> bgra;
}; // straight alpha

// Loads a PNG, crops it to its non-transparent box and fits it into w x h (never enlarged).
inline Icon load_icon(const fs::path& png, uint32_t w, uint32_t h) {
    Gdiplus::Bitmap source(png.c_str());
    if (source.GetLastStatus() != Gdiplus::Ok) fail("cannot read " + png.string());
    const UINT sw = source.GetWidth(), sh = source.GetHeight();
    Gdiplus::Rect all(0, 0, INT(sw), INT(sh));
    Gdiplus::BitmapData lock{};
    if (source.LockBits(&all, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &lock) != Gdiplus::Ok)
        fail("cannot decode " + png.string());
    UINT x0 = sw, y0 = sh, x1 = 0, y1 = 0;
    for (UINT y = 0; y < sh; ++y)
        for (UINT x = 0; x < sw; ++x)
            if (static_cast<const uint8_t*>(lock.Scan0)[size_t(y) * lock.Stride + x * 4 + 3]) {
                x0 = std::min(x0, x);
                y0 = std::min(y0, y);
                x1 = std::max(x1, x + 1);
                y1 = std::max(y1, y + 1);
            }
    source.UnlockBits(&lock);
    if (x1 <= x0 || y1 <= y0) fail("empty icon " + png.string());
    const UINT cw = x1 - x0, ch = y1 - y0;
    const double scale = std::min({double(w) / cw, double(h) / ch, 1.0});
    Icon icon;
    icon.width = std::max<uint32_t>(1, uint32_t(std::lround(cw * scale)));
    icon.height = std::max<uint32_t>(1, uint32_t(std::lround(ch * scale)));
    Gdiplus::Bitmap target(INT(icon.width), INT(icon.height), PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&target);
        g.SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        g.Clear(Gdiplus::Color(0, 0, 0, 0));
        Gdiplus::ImageAttributes clamp;
        clamp.SetWrapMode(Gdiplus::WrapModeTileFlipXY);
        g.DrawImage(&source, Gdiplus::Rect(0, 0, INT(icon.width), INT(icon.height)), INT(x0), INT(y0), INT(cw), INT(ch),
                    Gdiplus::UnitPixel, &clamp);
    }
    Gdiplus::Rect out(0, 0, INT(icon.width), INT(icon.height));
    if (target.LockBits(&out, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &lock) != Gdiplus::Ok)
        fail("icon resize");
    icon.bgra.resize(size_t(icon.width) * icon.height * 4);
    for (UINT y = 0; y < icon.height; ++y)
        std::memcpy(&icon.bgra[size_t(y) * icon.width * 4],
                    static_cast<const uint8_t*>(lock.Scan0) + size_t(y) * lock.Stride, icon.width * 4);
    target.UnlockBits(&lock);
    return icon;
}

// Replaces the glyphs of the single 512x263 button atlas in a retail IGZ10 font.
inline Bytes patch_font(Bytes data, const fs::path& assets) {
    const auto u = [&](size_t o) {
        if (o + 4 > data.size()) fail("font read outside file");
        return rd32(&data[o]);
    };
    if (u(0) != 0x49475a01u || u(4) != 10 || u(12) != 8) fail("unsupported font format");
    std::vector<std::array<uint32_t, 4>> chunks;
    for (size_t o = 20; o < 0x224; o += 16) {
        std::array<uint32_t, 4> c{u(o), u(o + 4), u(o + 8), u(o + 12)};
        if (!c[1]) break;
        if (uint64_t(c[1]) + c[2] > data.size()) fail("font chunk outside file");
        chunks.push_back(c);
    }
    if (chunks.empty()) fail("font has no chunks");
    struct Fixup {
        uint32_t n;
        size_t at;
        uint32_t size;
    };
    std::map<std::string, Fixup> fixups;
    size_t o = chunks[0][1];
    for (uint32_t i = 0, count = u(16); i < count; ++i) {
        const uint32_t n = u(o + 4), size = u(o + 8), header = u(o + 12);
        fixups[std::string(reinterpret_cast<const char*>(&data[o]), 4)] = {n, o + header, size - header};
        o += size;
    }
    if (!fixups.count("TMET") || !fixups.count("RVTB")) fail("font fixups missing");
    std::vector<std::string> types;
    o = fixups["TMET"].at;
    for (uint32_t i = 0; i < fixups["TMET"].n; ++i) {
        size_t end = o;
        while (end < data.size() && data[end]) ++end;
        if (end >= data.size()) fail("font type table");
        types.emplace_back(reinterpret_cast<const char*>(&data[o]), end - o);
        o = end + 1;
        if (o < data.size() && data[o] == 0) ++o;
    }
    const auto absolute = [&](uint32_t ptr) -> size_t {
        const size_t c = (ptr >> 27) + 1;
        if (c >= chunks.size()) fail("font pointer chunk");
        return size_t(chunks[c][1]) + (ptr & 0x7ffffff);
    };
    std::vector<std::pair<size_t, std::string>> objects;
    const Fixup rvtb = fixups["RVTB"];
    uint64_t prev = 0, delta = 0;
    int shift = 0;
    for (size_t i = 0; i < rvtb.size && objects.size() < rvtb.n; ++i) {
        const uint8_t byte = data.at(rvtb.at + i);
        for (uint8_t nib : {uint8_t(byte & 15), uint8_t(byte >> 4)}) {
            delta |= uint64_t(nib & 7) << shift;
            shift += 3;
            if (!(nib & 8)) {
                prev += delta * 4;
                delta = 0;
                shift = 0;
                const size_t pos = absolute(uint32_t(prev));
                const uint32_t type = u(pos);
                if (type >= types.size()) fail("font object type");
                objects.emplace_back(pos, types[type]);
                if (objects.size() == rvtb.n) break;
            }
        }
    }
    std::vector<size_t> images, lists;
    for (auto& [pos, type] : objects) {
        if (type == "igImage2" && rd16(&data.at(pos + 24)) == 512 && rd16(&data.at(pos + 26)) == 263)
            images.push_back(pos);
        if (type == "igCharMetricsList" && (u(pos + 16) == 13 || u(pos + 16) == 14)) lists.push_back(pos);
    }
    if (images.size() != 1 || lists.size() != 1) fail("button atlas/layout not recognized");
    const size_t start = absolute(u(images[0] + 64)), size = u(images[0] + 56);
    if (size != 512 * 264 * 4 || start + size > data.size()) fail("unexpected button texture extent");
    const std::string atlas_hash = sha256(&data[start], size);
    const bool is_update = atlas_hash == kUpdateAtlas[0] || atlas_hash == kUpdateAtlas[1];
    if (atlas_hash != kBaseAtlas && !is_update) fail("unknown retail button atlas");
    if (u(lists[0] + 16) != (is_update ? 14u : 13u)) fail("unexpected metric layout");
    // 8x8 Morton-ordered tiles, 64 tiles per row; BGRA8.
    const auto tiled = [](uint32_t x, uint32_t y) {
        uint32_t m = 0;
        for (int b = 0; b < 3; ++b) m |= (((x & 7) >> b) & 1) << (2 * b) | (((y & 7) >> b) & 1) << (2 * b + 1);
        return ((size_t(y / 8) * 64 + x / 8) * 64 + m) * 4;
    };
    std::vector<uint8_t> linear(size);
    for (uint32_t y = 0; y < 264; ++y)
        for (uint32_t x = 0; x < 512; ++x)
            std::memcpy(&linear[(size_t(y) * 512 + x) * 4], &data[start + tiled(x, y)], 4);
    // Glyph indices established by prepare_xbox.py from retail metrics and visual inspection.
    static const std::vector<std::pair<uint32_t, const wchar_t*>> base_map{
        {3, L"LT"},      {4, L"RT"}, {5, L"LB"}, {6, L"RB"},       {7, L"B_Color"},
        {8, L"Y_Color"}, {9, L"L"},  {10, L"R"}, {11, L"X_Color"}, {12, L"A_Color"}};
    static const std::vector<std::pair<uint32_t, const wchar_t*>> update_map{
        {3, L"A_Color"}, {4, L"B_Color"}, {5, L"Y_Color"}, {6, L"X_Color"}, {7, L"LB"},    {8, L"RB"},
        {9, L"LT"},      {10, L"RT"},     {11, L"L"},      {12, L"R"},      {13, L"Share"}};
    const size_t table = absolute(u(lists[0] + 32));
    for (const auto& [index, name] : is_update ? update_map : base_map) {
        const size_t metric = absolute(u(table + size_t(index) * 8));
        float uv[4];
        std::memcpy(uv, &data.at(metric + 52), 16);
        // Python round(): half to even, which nearbyint matches in the default rounding mode.
        const double scale[4] = {512, 263, 512, 263};
        int box[4];
        for (int i = 0; i < 4; ++i) box[i] = int(std::nearbyint(double(uv[i]) * scale[i]));
        const int x = box[0], y = box[1], r = box[2], b = box[3];
        if (!(0 <= x && x < r && r <= 512 && 0 <= y && y < b && b <= 263)) fail("invalid glyph rectangle");
        const Icon icon =
            load_icon(assets / (std::wstring(L"T_X_") + name + L".png"), uint32_t(r - x), uint32_t(b - y));
        for (int yy = y; yy < b; ++yy) std::memset(&linear[(size_t(yy) * 512 + x) * 4], 0, size_t(r - x) * 4);
        const int ox = x + (r - x - int(icon.width)) / 2, oy = y + (b - y - int(icon.height)) / 2;
        for (uint32_t yy = 0; yy < icon.height; ++yy)
            std::memcpy(&linear[(size_t(oy + yy) * 512 + ox) * 4], &icon.bgra[size_t(yy) * icon.width * 4],
                        icon.width * 4);
    }
    for (uint32_t y = 0; y < 264; ++y)
        for (uint32_t x = 0; x < 512; ++x)
            std::memcpy(&data[start + tiled(x, y)], &linear[(size_t(y) * 512 + x) * 4], 4);
    return data;
}

inline std::set<std::string> font_names(const std::wstring& archive) {
    if (archive == L"permanent.pak") return {"fonts/geekspeak_outline.igz", "fonts/zoinks_outline.igz"};
    if (archive == L"languagestartup.pak") return {"fonts/geekspeak.igz"};
    if (archive == L"update.pak")
        return {"fonts/geekspeak.igz", "fonts/geekspeak_outline.igz", "fonts/zoinks_outline.igz"};
    fail("only permanent.pak, languagestartup.pak and update.pak are supported");
    return {};
}
inline std::string lower(std::string s) {
    for (auto& c : s) c = char(std::tolower(uint8_t(c)));
    return s;
}

// The location the runtime (xbox_asset_overlay) looks for, and whether it is complete.
inline fs::path target_for(const fs::path& user, const std::string& source_hash, const fs::path& source) {
    return user / "mods/xbox-default" / source_hash / source.filename();
}
inline bool prepared(const fs::path& pak) {
    std::error_code ec;
    std::ifstream checksum(pak.wstring() + L".sha256");
    std::string expected;
    checksum >> expected;
    return fs::is_regular_file(pak, ec) && expected.size() == 64;
}

// Builds the overlay archive: a copy of the retail archive with the patched fonts appended
// uncompressed and only their descriptors redirected; then rereads and checks the result.
inline fs::path prepare(const fs::path& source, const fs::path& user, const fs::path& assets) {
    const std::string source_hash = prompt_file_hash(source);
    const fs::path pak = target_for(user, source_hash, source);
    if (prepared(pak)) return pak;
    const auto names = font_names(source.filename().wstring());
    Archive archive(source);
    std::map<uint32_t, Bytes> replacements;
    for (const auto& e : archive.files)
        if (names.count(lower(e.path))) replacements[e.index] = patch_font(archive.extract(e), assets);
    if (replacements.size() != names.size()) fail("expected retail button fonts missing");

    fs::create_directories(pak.parent_path());
    const fs::path temp = pak.wstring() + L".partial";
    if (!CopyFileW(source.c_str(), temp.c_str(), FALSE)) fail("cannot copy " + source.string());
    SetFileAttributesW(temp.c_str(), FILE_ATTRIBUTE_NORMAL);
    {
        std::fstream out(temp, std::ios::binary | std::ios::in | std::ios::out);
        if (!out) fail("cannot open " + temp.string());
        uint64_t end = archive.size();
        for (const auto& [index, bytes] : replacements) {
            const uint64_t offset = (end + archive.sector_size - 1) / archive.sector_size * archive.sector_size;
            if (offset + bytes.size() > 0xFFFFFFFFull) fail("archive too large for overlay");
            const Bytes pad(size_t(offset - end), 0);
            out.seekp(std::streamoff(end));
            out.write(reinterpret_cast<const char*>(pad.data()), std::streamsize(pad.size()));
            out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
            end = offset + bytes.size();
            uint8_t descriptor[16];
            std::memcpy(descriptor, &archive.directory[56 + size_t(archive.file_count) * 4 + size_t(index) * 16], 16);
            wr32(descriptor, uint32_t(offset));
            wr32(descriptor + 12, 0xFFFFFFFFu);
            out.seekp(std::streamoff(56 + uint64_t(archive.file_count) * 4 + uint64_t(index) * 16));
            out.write(reinterpret_cast<const char*>(descriptor), 16);
        }
        out.flush();
        if (!out) fail("cannot write " + temp.string());
    }
    {
        Archive check(temp);
        if (check.files.size() != archive.files.size()) fail("overlay check: entry count");
        for (size_t i = 0; i < archive.files.size(); ++i) {
            const auto& a = archive.files[i];
            const auto& b = check.files[i];
            if (auto it = replacements.find(a.index); it != replacements.end()) {
                if (check.extract(b) != it->second) fail("overlay check: replacement differs");
            } else if (a.offset != b.offset || a.size != b.size || a.compression != b.compression || a.path != b.path) {
                fail("overlay check: unrelated entry changed");
            }
        }
    }
    if (prompt_file_hash(source) != source_hash) fail("retail archive changed during preparation");
    const std::string output_hash = prompt_file_hash(temp);
    std::error_code ec;
    fs::rename(temp, pak, ec);
    if (ec) fail("cannot finalize " + pak.string());
    std::ofstream(pak.wstring() + L".sha256") << output_hash << "\n";
    std::ofstream manifest(pak.parent_path() / "manifest.json");
    manifest
        << "{\n  \"source_sha256\": \"" << source_hash << "\",\n  \"output_sha256\": \"" << output_hash
        << "\",\n  \"source_unchanged\": true,\n  \"tool\": \"ctrport/xbox_prepare.hpp\",\n  \"replaced_entries\": "
        << replacements.size() << "\n}\n";
    return pak;
}

// Launcher entry point: skips rehashing a large retail archive whose path, size and
// write time match an earlier preparation. The runtime still verifies both hashes.
inline std::string utf8(const std::wstring& w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(size_t(std::max(n, 0)), '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.data(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}
inline void prepare_cached(const fs::path& source, const fs::path& user, const fs::path& assets) {
    const fs::path cache = user / "mods/xbox-default/sources.cache";
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExW(source.c_str(), GetFileExInfoStandard, &info)) fail("cannot read " + source.string());
    const std::string key =
        std::to_string((uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow) + "\t" +
        std::to_string((uint64_t(info.ftLastWriteTime.dwHighDateTime) << 32) | info.ftLastWriteTime.dwLowDateTime) +
        "\t" + utf8(fs::absolute(source).wstring());
    std::vector<std::string> lines;
    {
        std::ifstream in(cache, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            if (line.size() < 66 || line[64] != '\t') continue;
            if (line.substr(65) != key) {
                lines.push_back(line);
                continue;
            }
            if (prepared(target_for(user, line.substr(0, 64), source))) return;
        }
    }
    const fs::path pak = prepare(source, user, assets);
    lines.push_back(pak.parent_path().filename().string() + "\t" + key);
    std::ofstream out(cache, std::ios::binary | std::ios::trunc);
    for (const auto& line : lines) out << line << "\n";
}
} // namespace ctrport::xbox_prepare
