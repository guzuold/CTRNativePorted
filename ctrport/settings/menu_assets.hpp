// SPDX-License-Identifier: GPL-2.0-only
// Native PC Settings menu resources, built at run time from the user's own retail 1.21
// archives: the original Options menu gets a "PC Settings" row that opens native hub and
// selector pages cloned from the retail menu templates (no class serializer, no host UI).
// C++17 port of tools/settings/assets (igz10.py, generate_menu_assets.py, paginated_menu.py,
// retail121_pages.py, update_overlay.py, iga_overlay.py); for the same inputs it produces
// the same bytes, checked by tools/settings/menu_assets_control.cpp. Every template is
// pinned by SHA256, so an unknown game version or a mod that changed them is refused.
// Nothing is written to the game folders.
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "../lzma_raw.hpp"
#include "menu_catalog.hpp"
#pragma comment(lib, "bcrypt.lib")

namespace ctrport::settings::menu_assets {
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
using std::uint32_t; using std::uint64_t; using std::int32_t; using std::size_t;

[[noreturn]] inline void fail(const std::string& what) { throw std::runtime_error("PC Settings menu: " + what); }

inline uint32_t u32(const Bytes& d, uint64_t at) {
    if (at + 4 > d.size()) fail("IGZ word outside file/chunk");
    uint32_t v; std::memcpy(&v, d.data() + at, 4); return v;
}
inline void put32(Bytes& d, uint64_t at, uint64_t value) {
    if (at + 4 > d.size() || value > 0xffffffffull) fail("IGZ word write outside file/chunk");
    const uint32_t v = uint32_t(value); std::memcpy(d.data() + at, &v, 4);
}
inline double getf(const Bytes& d, uint64_t at) { const uint32_t bits = u32(d, at); float v; std::memcpy(&v, &bits, 4); return v; }
inline void putf(Bytes& d, uint64_t at, double value) { const float v = float(value); uint32_t bits; std::memcpy(&bits, &v, 4); put32(d, at, bits); }

inline std::string sha256(const std::uint8_t* data, size_t size) {
    BCRYPT_ALG_HANDLE alg{}; BCRYPT_HASH_HANDLE hash{}; std::array<unsigned char, 32> digest{};
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) fail("SHA256 provider");
    const bool ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
                    BCryptHashData(hash, const_cast<PUCHAR>(data), ULONG(size), 0) >= 0 &&
                    BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok) fail("SHA256");
    std::string r; for (auto b : digest) { r += "0123456789abcdef"[b >> 4]; r += "0123456789abcdef"[b & 15]; }
    return r;
}
inline std::string sha256(const Bytes& b) { return sha256(b.data(), b.size()); }
inline void pinned(const Bytes& blob, const char* expected, const std::string& what) {
    if (sha256(blob) != expected) fail("unrecognized retail template " + what);
}

// FNV-1a over the lowercase ASCII names used by these retail resources.
inline uint32_t name_hash(const std::string& value) {
    uint32_t result = 2166136261u;
    for (unsigned char c : value) {
        if (c >= 0x80) fail("non-ASCII resource name");
        const unsigned char l = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        result = (result ^ l) * 16777619u;
    }
    return result;
}

// ---- IGZ10 fixup reader/editor (igz10.py) -------------------------------------------------
inline bool is_encoded(const std::string& t) { for (const char* k : {"RVTB", "RSTT", "ROFS", "RPID", "REXT", "RHND", "RNEX"}) if (t == k) return true; return false; }
inline bool is_strings(const std::string& t) { return t == "TSTR" || t == "TMET" || t == "TDEP"; }
inline bool is_known(const std::string& t) {
    if (is_encoded(t) || is_strings(t)) return true;
    for (const char* k : {"MTSZ", "ROOT", "ONAM", "NSPC", "EXID", "EXNM"}) if (t == k) return true;
    return false;
}

inline std::vector<uint32_t> decode_offsets(const std::uint8_t* data, size_t size, uint32_t count) {
    std::vector<uint32_t> result; uint64_t value = 0; int shift = 0;
    for (size_t i = 0; i < size; ++i)
        for (const unsigned nibble : {unsigned(data[i] & 15), unsigned(data[i] >> 4)}) {
            value |= uint64_t(nibble & 7) << shift; shift += 3;
            if (shift > 33) fail("IGZ relocation delta overflow");
            if (!(nibble & 8)) {
                const uint64_t next = (result.empty() ? 0 : result.back()) + value * 4;
                if (next > 0xffffffffull || (!result.empty() && next <= result.back())) fail("IGZ relocation offsets not strictly increasing");
                result.push_back(uint32_t(next));
                if (result.size() == count) return result;
                value = 0; shift = 0;
            }
        }
    if (count) fail("IGZ truncated relocation table");
    return {};
}
inline Bytes encode_offsets(const std::vector<uint32_t>& values) {
    std::vector<std::uint8_t> nibbles; uint32_t previous = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        if ((i && values[i] <= values[i - 1]) || (values[i] & 3)) fail("IGZ offsets must be unique, sorted and word aligned");
        uint32_t delta = (values[i] - previous) / 4; previous = values[i];
        do { const std::uint8_t n = delta & 7; delta >>= 3; nibbles.push_back(std::uint8_t(n | (delta ? 8 : 0))); } while (delta);
    }
    while (nibbles.size() % 8) nibbles.push_back(0);
    Bytes out;
    for (size_t i = 0; i < nibbles.size(); i += 2) out.push_back(std::uint8_t(nibbles[i] | (nibbles[i + 1] << 4)));
    return out;
}
inline std::vector<std::string> decode_strings(const std::uint8_t* data, size_t size, uint32_t count) {
    std::vector<std::string> values; size_t at = 0;
    for (uint32_t i = 0; i < count; ++i) {
        const void* end = at < size ? std::memchr(data + at, 0, size - at) : nullptr;
        if (!end) fail("IGZ truncated string table");
        const size_t stop = size_t(static_cast<const std::uint8_t*>(end) - data);
        values.emplace_back(reinterpret_cast<const char*>(data + at), stop - at);
        at = stop + 1;
        if (at < size && data[at] == 0) ++at;
    }
    return values;
}
inline Bytes encode_strings(const std::vector<std::string>& values) {
    Bytes data;
    for (const auto& v : values) {
        if (v.find('\0') != std::string::npos) fail("IGZ embedded null in string");
        data.insert(data.end(), v.begin(), v.end()); data.push_back(0);
        if (data.size() & 1) data.push_back(0);
    }
    while (data.size() & 3) data.push_back(0);
    return data;
}

struct Section {
    std::string tag; uint32_t count = 0, header_size = 0; Bytes raw;
    std::vector<uint32_t> offsets; std::vector<std::string> strings; bool changed = false;
    Bytes serialize() const {
        if (!changed) return raw;
        Bytes payload; uint32_t n;
        if (is_encoded(tag)) { payload = encode_offsets(offsets); n = uint32_t(offsets.size()); }
        else if (is_strings(tag)) { payload = encode_strings(strings); n = uint32_t(strings.size() / (tag == "TDEP" ? 2 : 1)); }
        else fail("IGZ mutation of unsupported section " + tag);
        Bytes out(raw.begin(), raw.begin() + header_size);
        put32(out, 4, n); put32(out, 8, header_size + payload.size()); put32(out, 12, header_size);
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }
};

struct Object { size_t index; uint32_t start, end, size; std::string type; };

// Ordered name -> object map with Python dict semantics (igNameList order matters).
struct Names {
    std::vector<std::pair<std::string, uint32_t>> items;
    uint32_t at(const std::string& k) const {
        for (const auto& i : items) if (i.first == k) return i.second;
        fail("missing named object " + k);
    }
    void set(const std::string& k, uint32_t v) {
        for (auto& i : items) if (i.first == k) { i.second = v; return; }
        items.emplace_back(k, v);
    }
    uint32_t pop(const std::string& k) {
        for (auto it = items.begin(); it != items.end(); ++it)
            if (it->first == k) { const uint32_t v = it->second; items.erase(it); return v; }
        fail("missing named object " + k);
    }
};

class Igz {
public:
    explicit Igz(const Bytes& source) {
        if (source.size() < 2048 || u32(source, 0) != 0x49475A01u || u32(source, 4) != 10) fail("expected IGZ10");
        if (u32(source, 12) != 8) fail("expected retail PS4 IGZ platform 8");
        for (size_t at = 16; at < 2048; at += 16) {
            std::array<uint32_t, 4> r{u32(source, at), u32(source, at + 4), u32(source, at + 8), u32(source, at + 12)};
            if (r[2] == 0) break;
            if (uint64_t(r[2]) + r[3] > source.size()) fail("IGZ chunk outside file");
            chunks_.push_back(r);
        }
        if (chunks_.size() != 2) fail("editor accepts the verified two-chunk GUI contract only");
        header_.assign(source.begin(), source.begin() + chunks_[0][2]);
        const uint64_t start = chunks_[0][2], size = chunks_[0][3];
        uint64_t at = start;
        while (at < start + size) {
            if (u32(source, at) == 0) break;
            const std::string tag(reinterpret_cast<const char*>(source.data() + at), 4);
            if (!is_known(tag) || find(tag)) fail("IGZ unknown/duplicate fixup section " + tag);
            Section s; s.tag = tag; s.count = u32(source, at + 4);
            const uint32_t extent = u32(source, at + 8); s.header_size = u32(source, at + 12);
            if (s.header_size < 16 || extent < s.header_size || at + extent > start + size) fail("IGZ malformed fixup extent");
            s.raw.assign(source.begin() + at, source.begin() + at + extent);
            const std::uint8_t* payload = s.raw.data() + s.header_size; const size_t payload_size = extent - s.header_size;
            if (is_encoded(tag)) { if (s.count) s.offsets = decode_offsets(payload, payload_size, s.count); }
            else if (is_strings(tag)) s.strings = decode_strings(payload, payload_size, s.count * (tag == "TDEP" ? 2 : 1));
            sections_.push_back(std::move(s));
            at += extent;
        }
        fixup_tail_.assign(source.begin() + at, source.begin() + start + size);
        data.assign(source.begin() + chunks_[1][2], source.begin() + chunks_[1][2] + chunks_[1][3]);
        trailing_.assign(source.begin() + chunks_[1][2] + chunks_[1][3], source.end());
        for (const char* tag : {"TMET", "TSTR", "RVTB", "ROFS"}) if (!find(tag)) fail(std::string("IGZ missing required fixup ") + tag);
        validate();
    }
    Bytes data;

    Section* find(const std::string& tag) { for (auto& s : sections_) if (s.tag == tag) return &s; return nullptr; }
    Section& section(const std::string& tag) { if (auto* s = find(tag)) return *s; fail("IGZ missing section " + tag); }
    std::vector<uint32_t> offsets(const std::string& tag) { auto* s = find(tag); return s ? s->offsets : std::vector<uint32_t>{}; }
    bool has_offset(const std::string& tag, uint32_t at) { auto* s = find(tag); return s && std::binary_search(s->offsets.begin(), s->offsets.end(), at); }
    void relocations(const std::string& tag, std::vector<uint32_t> values) {
        auto& s = section(tag);
        std::sort(values.begin(), values.end()); values.erase(std::unique(values.begin(), values.end()), values.end());
        s.offsets = std::move(values); s.changed = true;
    }
    void add_relocations(const std::string& tag, const std::vector<uint32_t>& added) {
        auto values = offsets(tag); values.insert(values.end(), added.begin(), added.end()); relocations(tag, std::move(values));
    }
    std::vector<std::string>& strings() { return section("TSTR").strings; }
    void strings_changed() { section("TSTR").changed = true; }
    uint32_t add_string(const std::string& value) {
        auto& s = section("TSTR");
        const auto it = std::find(s.strings.begin(), s.strings.end(), value);
        if (it != s.strings.end()) return uint32_t(it - s.strings.begin());
        s.strings.push_back(value); s.changed = true;
        return uint32_t(s.strings.size() - 1);
    }
    void set_string(uint32_t at, const std::string& value) { put32(data, at, add_string(value)); add_relocations("RSTT", {at}); }
    uint32_t append(const Bytes& bytes, size_t alignment = 8) {
        data.resize(data.size() + (alignment - data.size() % alignment) % alignment, 0);
        const uint32_t at = uint32_t(data.size());
        data.insert(data.end(), bytes.begin(), bytes.end());
        return at;
    }
    // Clones complete objects and their inline buffers using relocation maps.
    uint32_t clone_range(uint32_t start, uint32_t end) {
        if (!has_offset("RVTB", start) || (end != data.size() && !has_offset("RVTB", end))) fail("clone boundaries must be complete object boundaries");
        std::vector<std::pair<std::string, std::vector<uint32_t>>> tables;
        for (const auto& s : sections_) if (is_encoded(s.tag)) tables.emplace_back(s.tag, s.offsets);
        const Bytes slice(data.begin() + start, data.begin() + end);
        const uint32_t delta = append(slice) - start;
        for (auto& [tag, locations] : tables) {
            std::vector<uint32_t> added;
            for (uint32_t p : locations) if (start <= p && p < end) added.push_back(p + delta);
            if (tag == "ROFS")
                for (uint32_t p : added) { const uint32_t v = u32(data, p); if (start <= v && v < end) put32(data, p, uint64_t(v) + delta); }
            if (!added.empty()) { locations.insert(locations.end(), added.begin(), added.end()); relocations(tag, locations); }
        }
        return delta;
    }
    // Retargets an existing verified igDataList with a new eight-byte buffer.
    uint32_t replace_list(uint32_t start, const std::vector<uint32_t>& values) { return replace_list_impl(start, values, {}, false); }
    uint32_t replace_list(uint32_t start, const std::vector<std::string>& values) { return replace_list_impl(start, {}, values, true); }
    void replace_names(const Names& named) {
        const auto objs = objects();
        const uint32_t roots = objs.at(0).start, names = first("igNameList");
        std::vector<uint32_t> targets; for (const auto& i : named.items) targets.push_back(i.second);
        replace_list(roots, targets);
        Bytes payload(named.items.size() * 16, 0);
        for (size_t i = 0; i < named.items.size(); ++i) {
            put32(payload, i * 16, add_string(named.items[i].first));
            put32(payload, i * 16 + 8, name_hash(named.items[i].first));
        }
        const uint32_t at = append(payload);
        for (uint32_t field : {12u, 16u}) put32(data, names + field, named.items.size());
        put32(data, names + 24, payload.size()); put32(data, names + 32, at);
        std::vector<uint32_t> added; for (size_t i = 0; i < named.items.size(); ++i) added.push_back(uint32_t(at + i * 16));
        add_relocations("RSTT", added);
    }
    uint32_t add_external(const std::string& name, const std::string& space, bool handle) {
        auto& s = section("EXNM");
        Bytes payload(s.raw.begin() + s.header_size, s.raw.begin() + s.header_size + size_t(s.count) * 8);
        uint32_t index = 0;
        for (size_t i = 0; i < payload.size(); i += 8) index += ((u32(payload, i + 4) & 0x80000000u) != 0) == handle;
        const uint32_t name_index = add_string(name);
        const uint32_t space_index = add_string(space) | (handle ? 0x80000000u : 0);
        auto& e = section("EXNM");  // add_string never reallocates sections_, but keep it explicit
        Bytes entry(8); put32(entry, 0, name_index); put32(entry, 4, space_index);
        payload.insert(payload.end(), entry.begin(), entry.end());
        Bytes head(e.raw.begin(), e.raw.begin() + e.header_size);
        ++e.count; put32(head, 4, e.count); put32(head, 8, head.size() + payload.size());
        head.insert(head.end(), payload.begin(), payload.end()); e.raw = std::move(head);
        return index | (handle ? 0x80000000u : 0);
    }
    std::vector<std::pair<std::string, std::string>> externals(bool handle) {
        auto& s = section("EXNM"); const auto& strings = section("TSTR").strings;
        std::vector<std::pair<std::string, std::string>> result;
        for (uint32_t i = 0; i < s.count; ++i) {
            const uint32_t name = u32(s.raw, s.header_size + i * 8ull), space = u32(s.raw, s.header_size + i * 8ull + 4);
            if (((space & 0x80000000u) != 0) == handle) {
                if (name >= strings.size() || (space & 0x7fffffffu) >= strings.size()) fail("IGZ external name outside TSTR");
                result.emplace_back(strings[name], strings[space & 0x7fffffffu]);
            }
        }
        return result;
    }
    std::vector<Object> objects() {
        const auto& starts = section("RVTB").offsets; const auto& types = section("TMET").strings;
        std::vector<Object> result;
        for (size_t i = 0; i < starts.size(); ++i) {
            const uint32_t start = starts[i];
            if (start >> 27) fail("IGZ non-first-chunk object not supported");
            const uint32_t end = i + 1 < starts.size() ? starts[i + 1] : uint32_t(data.size());
            const uint32_t type = u32(data, start);
            if (type >= types.size()) fail("IGZ invalid object type index");
            result.push_back({i, start, end, end - start, types[type]});
        }
        return result;
    }
    uint32_t first(const std::string& type) {
        for (const auto& o : objects()) if (o.type == type) return o.start;
        fail("missing object of type " + type);
    }
    bool object_at(uint32_t offset, Object& out) {
        for (const auto& o : objects()) if (o.start <= offset && offset < o.end) { out = o; return true; }
        return false;
    }
    uint32_t pointer(uint32_t offset) const { return u32(data, offset); }
    std::string string(uint32_t offset) {
        if (!has_offset("RSTT", offset)) fail("IGZ field lacks string relocation");
        const uint32_t index = pointer(offset); const auto& s = section("TSTR").strings;
        if (index >= s.size()) fail("IGZ string index outside TSTR");
        return s[index];
    }
    Names named_objects() {
        const auto objs = objects();
        const uint32_t roots = objs.at(0).start, names = first("igNameList");
        const uint32_t count = u32(data, roots + 12);
        if (count != u32(data, names + 12)) fail("IGZ root/name list counts differ");
        const uint32_t rdata = pointer(roots + 32), ndata = pointer(names + 32);
        Names result;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t target = pointer(rdata + i * 8); Object o;
            if (!object_at(target, o) || o.start != target) fail("IGZ named root does not identify an object");
            const std::string name = string(ndata + i * 16);
            for (const auto& item : result.items) if (item.first == name) fail("IGZ duplicate root name");
            result.items.emplace_back(name, target);
        }
        return result;
    }
    void validate() {
        const auto objs = objects();
        if (objs.empty() || objs[0].start != 0) fail("IGZ root object missing");
        const size_t strings = section("TSTR").strings.size();
        for (const auto& s : sections_) {
            if (!is_encoded(s.tag)) continue;
            for (uint32_t at : s.offsets) {
                if ((at >> 27) || uint64_t(at) + 4 > data.size()) fail("IGZ relocation location outside data");
                if (s.tag == "ROFS") { const uint32_t t = pointer(at); if ((t >> 27) || t >= data.size()) fail("IGZ pointer target outside data"); }
                else if (s.tag == "RSTT" && pointer(at) >= strings) fail("IGZ invalid string relocation target");
            }
        }
    }
    Bytes serialize() {
        validate();
        Bytes fixups;
        for (const auto& s : sections_) { const Bytes b = s.serialize(); fixups.insert(fixups.end(), b.begin(), b.end()); }
        fixups.insert(fixups.end(), fixup_tail_.begin(), fixup_tail_.end());
        Bytes out = header_;
        auto first_chunk = chunks_[0], second = chunks_[1];
        first_chunk[3] = uint32_t(fixups.size()); second[2] = first_chunk[2] + first_chunk[3]; second[3] = uint32_t(data.size());
        for (int i = 0; i < 4; ++i) { put32(out, 16 + i * 4, first_chunk[i]); put32(out, 32 + i * 4, second[i]); }
        out.insert(out.end(), fixups.begin(), fixups.end());
        out.insert(out.end(), data.begin(), data.end());
        out.insert(out.end(), trailing_.begin(), trailing_.end());
        return out;
    }
    // Serialized class size table entry for a type (MTSZ).
    uint32_t class_size(const std::string& type) {
        auto& sizes = section("MTSZ"); const auto& types = section("TMET").strings;
        const auto it = std::find(types.begin(), types.end(), type);
        if (it == types.end()) fail("IGZ type missing " + type);
        if (sizes.count != types.size()) fail("retail serialized class size mismatch");
        return u32(sizes.raw, sizes.header_size + uint64_t(it - types.begin()) * 4);
    }

private:
    uint32_t replace_list_impl(uint32_t start, const std::vector<uint32_t>& values, const std::vector<std::string>& texts, bool strings) {
        const size_t n = strings ? texts.size() : values.size();
        Bytes payload(n * 8, 0);
        for (size_t i = 0; i < n; ++i) put32(payload, i * 8, strings ? add_string(texts[i]) : values[i]);
        const uint32_t at = append(payload);
        put32(data, start + 12, n); put32(data, start + 16, n);
        put32(data, start + 24, payload.size()); put32(data, start + 32, at);
        std::vector<uint32_t> added; for (size_t i = 0; i < n; ++i) added.push_back(uint32_t(at + i * 8));
        add_relocations(strings ? "RSTT" : "ROFS", added);
        return at;
    }
    std::vector<std::array<uint32_t, 4>> chunks_;
    Bytes header_, fixup_tail_, trailing_;
    std::vector<Section> sections_;
};

inline void require(Igz& f, uint32_t at, const std::string& kind, uint32_t size = 0) {
    Object o;
    if (!f.object_at(at, o) || o.start != at || o.type != kind || (size && o.size != size)) fail("retail layout contract mismatch at " + std::to_string(at));
    if (f.class_size(kind) > o.size) fail("retail serialized class size mismatch");
}

// ---- IGA13 archive reader (tools/input_prompts/ctr_nf_iga.py) ------------------------------
struct IgaEntry { uint32_t index, id; uint64_t offset; int32_t ordinal; uint32_t size, compression; std::string full_path, path; };

class Iga {
public:
    // whole: keep the complete file in `data` (needed to rebuild a small archive).
    Iga(const fs::path& path, bool whole) : file_(path, std::ios::binary) {
        if (!file_) fail("cannot open " + path.u8string());
        file_.seekg(0, std::ios::end); size_ = uint64_t(file_.tellg());
        if (whole) data = read(0, size_t(size_));
        const Bytes h = whole ? Bytes(data.begin(), data.begin() + (std::min<uint64_t>)(56, size_)) : read(0, 56);
        if (h.size() < 56 || u32(h, 0) != 0x1A414749u || u32(h, 4) != 13) fail("not an IGA v13 archive: " + path.u8string());
        file_count = u32(h, 12); sector_size = u32(h, 16);
        large_count = u32(h, 28); medium_count = u32(h, 32); small_count = u32(h, 36);
        names_offset = u32(h, 40) | (uint64_t(u32(h, 44)) << 32); names_size = u32(h, 48);
        if (names_offset < 56 || names_offset + names_size > size_) fail("IGA name table outside archive");
        if (56 + uint64_t(file_count) * 20 > names_offset) fail("IGA descriptor table overlaps name table");
        const uint64_t tables = 56 + uint64_t(file_count) * 20, tables_end = tables + large_count * 4ull + medium_count * 2ull + small_count;
        if (tables_end > names_offset || tables_end > (64u << 20) || names_size > (64u << 20)) fail("IGA directory too large");
        directory = whole ? Bytes(data.begin(), data.begin() + tables_end) : read(0, size_t(tables_end));
        const Bytes names = read(names_offset, names_size);
        uint64_t at = tables;
        for (uint32_t i = 0; i < large_count; ++i, at += 4) large_.push_back(u32(directory, at));
        for (uint32_t i = 0; i < medium_count; ++i, at += 2) medium_.push_back(uint16_t(directory[at] | (directory[at + 1] << 8)));
        small_.assign(directory.begin() + at, directory.begin() + at + small_count);
        const auto cstring = [&](size_t pos, size_t& next) {
            const size_t start = pos;
            while (pos < names.size() && names[pos]) ++pos;
            if (pos >= names.size()) fail("IGA name table");
            next = pos + 1;
            return std::string(reinterpret_cast<const char*>(names.data() + start), pos - start);
        };
        for (uint32_t i = 0; i < file_count; ++i) {
            const uint64_t d = 56 + uint64_t(file_count) * 4 + uint64_t(i) * 16;
            const uint32_t rel = u32(names, uint64_t(i) * 4);
            if (rel < uint64_t(file_count) * 4 || rel >= names_size) fail("IGA name offset outside name table");
            size_t next = 0;
            std::string full = cstring(rel, next); std::string shortp = cstring(next, next);
            const uint32_t ordinal = u32(directory, d + 4);
            files.push_back({i, u32(directory, 56 + uint64_t(i) * 4), u32(directory, d) | (uint64_t(ordinal & 0xFF) << 32),
                             int32_t(ordinal), u32(directory, d + 8), u32(directory, d + 12), std::move(full), std::move(shortp)});
        }
    }
    const IgaEntry& entry(const std::string& path) const {
        for (const auto& e : files) if (e.path == path) return e;
        fail("archive entry missing: " + path);
    }
    Bytes extract(const std::string& path) { return extract(entry(path)); }
    Bytes extract(const IgaEntry& e) {
        if (e.offset >= size_) fail("IGA payload offset outside archive");
        if (e.compression == 0xFFFFFFFFu) return read(e.offset, e.size);
        Bytes out(e.size);
        const uint32_t blocks = uint32_t((uint64_t(e.size) + 0x7FFF) / 0x8000), type = e.compression >> 28;
        for (uint32_t b = 0; b < blocks; ++b) {
            const uint32_t index = (e.compression & 0x0FFFFFFF) + b;
            uint32_t value, mask, shift;
            if (e.size <= 0x7Full * sector_size) { if (index >= small_.size()) fail("block table"); value = small_[index]; mask = 0x7F; shift = 7; }
            else if (e.size <= 0x7FFFull * sector_size) { if (index >= medium_.size()) fail("block table"); value = medium_[index]; mask = 0x7FFF; shift = 15; }
            else { if (index >= large_.size()) fail("block table"); value = large_[index]; mask = 0x7FFFFFFF; shift = 31; }
            const uint64_t relative = uint64_t(value & mask) * sector_size, source = e.offset + relative;
            const size_t out_at = size_t(b) * 0x8000, out_size = (std::min<size_t>)(0x8000, e.size - out_at);
            if ((value >> shift) != 1) {
                if (relative + out_size > e.size) fail("truncated IGA block in " + e.path);
                const Bytes raw = read(source, out_size); std::memcpy(&out[out_at], raw.data(), out_size); continue;
            }
            if (type != 2) fail("unsupported compression in " + e.path);
            const Bytes head = read(source, 9);
            const uint32_t packed = u32(head, 0);
            if (relative + 9 + packed > e.size) fail("truncated IGA block in " + e.path);
            const Bytes payload = read(source + 9, packed);
            lzma_raw::Decoder(payload.data(), payload.size(), head[4], u32(head, 5)).decode(&out[out_at], out_size);
        }
        return out;
    }
    uint64_t size() const { return size_; }
    uint32_t file_count = 0, sector_size = 0, large_count = 0, medium_count = 0, small_count = 0, names_size = 0;
    uint64_t names_offset = 0;
    Bytes data, directory;
    std::vector<IgaEntry> files;

private:
    Bytes read(uint64_t at, size_t n) {
        if (at + n > size_) fail("archive read outside file");
        Bytes b(n);
        file_.clear(); file_.seekg(std::streamoff(at));
        if (n) file_.read(reinterpret_cast<char*>(b.data()), std::streamsize(n));
        if (!file_) fail("archive read failed");
        return b;
    }
    std::ifstream file_;
    uint64_t size_ = 0;
    std::vector<uint32_t> large_; std::vector<uint16_t> medium_; std::vector<std::uint8_t> small_;
};

// Builds a separate IGA13 archive; every original entry payload is preserved (iga_overlay.py).
inline Bytes build_archive(const Iga& archive, const std::vector<std::pair<std::string, Bytes>>& replacements) {
    struct Record { uint32_t id; uint64_t offset; int32_t ordinal; uint32_t size, compression; std::string full_path, path; };
    std::vector<Record> records;
    for (const auto& r : archive.files) {
        if (name_hash(r.path) != r.id) fail("IGA path hash contract mismatch");
        records.push_back({r.id, r.offset, r.ordinal, r.size, r.compression, r.full_path, r.path});
    }
    const auto max_spread = [](const std::vector<Record>& rs, uint32_t divider) {
        uint64_t m = 0;
        for (size_t i = 0; i < rs.size(); ++i) { const int64_t d = int64_t(rs[i].id / divider) - int64_t(i); m = (std::max<uint64_t>)(m, uint64_t(d < 0 ? -d : d)); }
        return m;
    };
    {
        std::vector<Record> by_index = records;  // archive.files order is index order
        const uint32_t divider = u32(archive.data, 20);
        if (divider != uint32_t((1ull << 32) / records.size()) || u32(archive.data, 24) != max_spread(by_index, divider))
            fail("IGA interpolation-search contract mismatch");
    }
    Bytes result = archive.data;
    int32_t ordinal = records.front().ordinal;
    for (const auto& r : records) ordinal = (std::max)(ordinal, r.ordinal);
    ordinal += 256;
    static const std::regex vsc(R"(vsc/UI/(CTRPort_PC_Settings(?:_[A-Za-z0-9]+)*)/\1_Root\.igz)");
    const uint32_t sector = archive.sector_size;
    for (const auto& [path, blob] : replacements) {
        const bool ui = path.rfind("ui/", 0) == 0 || path.rfind("UI/Domains/", 0) == 0 || path.rfind("packages/generated/UI/Domains/", 0) == 0;
        if ((!ui && !std::regex_match(path, vsc)) || ("/" + path + "/").find("/../") != std::string::npos) fail("asset override path outside UI namespace");
        result.resize(result.size() + (sector - result.size() % sector) % sector, 0);
        const uint64_t at = result.size(); result.insert(result.end(), blob.begin(), blob.end());
        auto it = std::find_if(records.begin(), records.end(), [&](const Record& r) { return r.path == path; });
        if (it == records.end()) {
            records.push_back({name_hash(path), 0, ordinal, 0, 0, "temporary/octane/data/ps4/output/" + path, path});
            ordinal += 256; it = records.end() - 1;
        }
        it->offset = at; it->size = uint32_t(blob.size()); it->compression = 0xffffffffu;
    }
    std::stable_sort(records.begin(), records.end(), [](const Record& a, const Record& b) { return a.id < b.id; });
    for (size_t i = 1; i < records.size(); ++i) if (records[i].id == records[i - 1].id) fail("IGA path hash collision");
    const uint64_t old_tables = 56 + uint64_t(archive.file_count) * 20;
    const uint64_t table_size = archive.large_count * 4ull + archive.medium_count * 2ull + archive.small_count;
    const uint64_t index_end = 56 + records.size() * 20 + table_size;
    uint64_t first_payload = UINT64_MAX;
    for (const auto& r : archive.files) first_payload = (std::min)(first_payload, r.offset);
    if (index_end > first_payload) {
        // Block tables hold file-relative offsets: grow the directory by whole sectors and
        // shift every descriptor; compressed payloads and their block tables stay exact.
        const uint64_t growth = (index_end - first_payload + sector - 1) / sector * sector;
        result.insert(result.begin() + first_payload, size_t(growth), 0);
        for (auto& r : records) r.offset += growth;
    }
    const Bytes tables(archive.data.begin() + old_tables, archive.data.begin() + old_tables + table_size);
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        put32(result, 56 + i * 4, r.id);
        const uint64_t d = 56 + records.size() * 4 + i * 16;
        put32(result, d, r.offset); put32(result, d + 4, uint32_t(r.ordinal)); put32(result, d + 8, r.size); put32(result, d + 12, r.compression);
    }
    std::copy(tables.begin(), tables.end(), result.begin() + 56 + records.size() * 20);
    Bytes names(records.size() * 4, 0);
    for (size_t i = 0; i < records.size(); ++i) {
        put32(names, i * 4, names.size());
        names.insert(names.end(), records[i].full_path.begin(), records[i].full_path.end()); names.push_back(0);
        names.insert(names.end(), records[i].path.begin(), records[i].path.end()); names.push_back(0);
    }
    result.resize(result.size() + (sector - result.size() % sector) % sector, 0);
    put32(result, 40, result.size()); put32(result, 48, names.size());
    result.insert(result.end(), names.begin(), names.end());
    const uint32_t divider = uint32_t((1ull << 32) / records.size());
    put32(result, 8, index_end - 56); put32(result, 12, records.size()); put32(result, 20, divider);
    put32(result, 24, max_spread(records, divider));
    return result;
}

// ---- menu resources (generate_menu_assets.py, paginated_menu.py, retail121_pages.py) ----
constexpr const char* kMenu = "Octane_Options_Menu";
constexpr const char* kPage = "Octane_Options_Menu_Page";
constexpr const char* kCustom = "CTRPort_PC_Settings";
constexpr size_t kLocales = menu_catalog::locales.size();
using Labels = std::array<std::string, kLocales>;
using catalog_control = menu_catalog::Control;

inline const std::string& label(size_t locale, size_t index) { return menu_catalog::labels()[locale][index]; }
inline std::vector<std::string> display_values(const catalog_control& c) {
    std::vector<std::string> out;
    for (const auto& v : c.values) out.push_back(c.key == "upscaler" && (v == "fsr3" || v == "dlss") ? v + " (auto)" : v);
    return out;
}
inline std::vector<uint32_t> list_values(Igz& f, uint32_t at) {
    const uint32_t buf = u32(f.data, at + 32); std::vector<uint32_t> out;
    for (uint32_t i = 0; i < u32(f.data, at + 12); ++i) out.push_back(u32(f.data, buf + i * 8ull));
    return out;
}
inline uint32_t clone_object(Igz& f, const std::string& type) {
    for (const auto& o : f.objects()) if (o.type == type) return o.start + f.clone_range(o.start, o.end);
    fail("missing object of type " + type);
}
inline uint32_t add_localization(Igz& f, const std::string& text, const std::string& target, const std::string& space, const std::string& field) {
    const uint32_t at = clone_object(f, "igLocalizedStringData");
    f.set_string(at + 16, text);
    put32(f.data, at + 24, f.add_external(target, space, true));
    put32(f.data, at + 32, f.add_external(field, "metafield", false));
    return at;
}
inline uint32_t add_localized_list(Igz& f, const std::vector<std::string>& values, const std::string& target, const std::string& space) {
    const uint32_t at = clone_object(f, "igLocalizedList");
    f.replace_list(at, values);
    put32(f.data, at + 40, f.add_external(target, space, true));
    put32(f.data, at + 48, f.add_external("CGuiBehaviorOctaneGlobalOptionSelector::_optionsList", "metafield", false));
    return at;
}
// Exact namespace/resource strings; unrelated retail graph imports are kept. igNameList
// hashes follow the renamed strings for retail handle lookup.
inline void rename_project(Igz& f, const std::string& old, const std::string& now) {
    const std::string root = old + "_Root";
    for (auto& v : f.strings()) {
        if (v == old) v = now;
        else if (v == root) v = now + "_Root";
        else if (v == "Scripts.Graph." + root) v = "Scripts.Graph." + now + "_Root";
        else if (v.rfind(root, 0) == 0) v = now + "_Root" + v.substr(root.size());
    }
    f.strings_changed();
    const auto& types = f.section("TMET").strings;
    const bool vsc = std::find(types.begin(), types.end(), "igVscMetaObject") != types.end();
    for (const auto& o : f.objects()) {
        if (o.type != "igNameList" || !vsc) continue;
        const uint32_t buf = u32(f.data, o.start + 32);
        for (uint32_t i = 0; i < u32(f.data, o.start + 12); ++i) put32(f.data, buf + i * 16ull + 8, name_hash(f.string(buf + i * 16)));
    }
}

// Base archive Options menu: an extra "PC Settings" row cloned from the Gameplay button.
inline Bytes make_menu(const Bytes& source) {
    Igz f(source); Names n = f.named_objects();
    require(f, 0xe50, "igGuiPlaceable", 288); require(f, 0x20a0, "igGuiBehaviorList", 64);
    require(f, 0x2320, "igGuiVscBehavior", 48); require(f, 0x2350, "igGuiVscBehavior", 128);
    const uint32_t p = 0xe50 + f.clone_range(0xe50, 0xf70), d = f.clone_range(0x20a0, 0x2710);
    const uint32_t behavior = 0x20a0 + d, nav = 0x20e0 + d, open_menu = 0x2350 + d;
    put32(f.data, p + 0xe8, behavior); put32(f.data, p + 0xf0, 0x23d0 + d);
    for (uint32_t at : f.offsets("ROFS")) if (0x20a0 + d <= at && at < 0x2710 + d && u32(f.data, at) == 0xe50) put32(f.data, at, p);
    const uint32_t table = u32(f.data, n.at("Null_Asset_Buttons") + 0x48);
    auto children = list_values(f, table); children.push_back(p);
    f.replace_list(table, children);
    const uint32_t language = n.at("Comp__Menu_ButtonWithIcon005"), following = u32(f.data, language + 0x58);
    put32(f.data, language + 0x58, p); put32(f.data, p + 0x50, language); put32(f.data, p + 0x58, following); put32(f.data, following + 0x50, p);
    put32(f.data, nav + 0x110, language); put32(f.data, nav + 0x118, 0xe50);
    put32(f.data, 0x20e0 + 0x110, p); put32(f.data, 0x11e8 + 0x118, p);
    const double y = getf(f.data, language + 0x64), previous = getf(f.data, 0xf70 + 0x64);
    putf(f.data, p + 0x64, y + (y - previous));
    put32(f.data, open_menu + 0x30, f.add_external(kCustom, kCustom, true));
    n.set("CTRPort_PC_Settings_Button", p); n.set("CTRPort_PC_Settings_Navigation", nav); n.set("CTRPort_PC_Settings_Behaviors", behavior);
    n.set("CTRPort_PC_Settings_SelectPage", 0x2320 + d); n.set("CTRPort_PC_Settings_OpenPage", open_menu);
    f.replace_names(n); f.validate();
    return f.serialize();
}
// Options menu localization: the title of the added row.
inline Bytes make_language(const Bytes& source, size_t locale) {
    Igz f(source);
    const uint32_t data_list = f.first("igLocalizedStringDataList");
    auto entries = list_values(f, data_list);
    entries.push_back(add_localization(f, label(locale, 0), "CTRPort_PC_Settings_Navigation", kMenu, "CGuiBehaviorNavigationItem::_itemText"));
    f.replace_list(data_list, entries);
    return f.serialize();
}

struct Row { std::string name; uint32_t placeable; Labels labels; const catalog_control* control = nullptr; };
struct Info { std::string project; bool page; std::vector<Row> rows; };
struct Child { std::string project; Labels labels; };
struct Node { std::string project; bool page; std::vector<const catalog_control*> controls; std::vector<Child> children; Labels labels; };

constexpr const char* kMenuSha = "b06237f0c7f54998fe4cebd3844b62c63761fb74d2f1723ce5160fc82561503c";
constexpr const char* kPageSha = "38b232d6488db220d497c807313319da9140c36a1cc7c3a25cf078ff57a1cbaf";

// Selector page (1..7 rows) cloned from the 1.21 Options page's global option selector.
inline Bytes make_page121(const Bytes& source, const std::vector<const catalog_control*>& controls, const std::string& project, Info& info) {
    if (controls.empty() || controls.size() > 7) fail("a native selector page requires 1..7 controls");
    pinned(source, kPageSha, "options page");
    Igz f(source); Names n = f.named_objects();
    require(f, 0x2910, "igGuiPlaceable", 288); require(f, 0x34b8, "igGuiBehaviorList", 56);
    require(f, 0x3760, "CGuiBehaviorOctaneGlobalOptionSelector", 464); require(f, 0x39d0, "igStringRefList", 64);
    if (f.class_size("CGuiBehaviorOctaneGlobalOptionSelector") != 464) fail("unknown serialized selector MTSZ");
    if (u32(f.data, 0x3760 + 0x1c8) != 9 || u32(f.data, 0x3760 + 0x1c0) != 0x39d0) fail("retail selector field contract mismatch");
    // The page keeps the retail root graph, animations, backdrop and Back behavior.
    for (auto& s : f.strings()) { if (s == kPage) s = project; else if (s == std::string(kPage) + "_Root") s = project + "_Root"; }
    f.strings_changed();
    n.set(project, n.pop(kPage));
    struct PageRow { std::string name; uint32_t placeable, selector; const catalog_control* control; };
    std::vector<PageRow> rows;
    for (size_t i = 0; i < controls.size(); ++i) {
        const auto& c = *controls[i];
        const uint32_t p = 0x2910 + f.clone_range(0x2910, 0x2a30), d = f.clone_range(0x34b8, 0x3a10);
        const uint32_t nav = 0x34f0 + d, selector = 0x3760 + d, options = 0x39d0 + d;
        // The global selector handles directional focus itself.
        put32(f.data, p + 0xe8, 0x34b8 + d); put32(f.data, p + 0x40, 0x1b10);
        putf(f.data, p + 0x60, 20.0 + double(i) * 10.0); putf(f.data, p + 0x64, -340.0 + double(i) * 100.0);
        for (uint32_t field : {0x84u, 0x88u}) putf(f.data, p + field, 0.9);
        put32(f.data, selector + 0x1c8, c.id); put32(f.data, selector + 0x1cc, 0);
        f.replace_list(options, display_values(c));
        const std::string row = "CTRPort_PC_Setting_" + c.key;
        n.set(row, p); n.set(row + "_Navigation", nav); n.set(row + "_Selector", selector);
        n.set(row + "_Options", options); n.set(row + "_Behaviors", 0x34b8 + d);
        rows.push_back({row, p, selector, &c});
    }
    // Retail's flattened traversal: predecessor -> rows -> parent.
    const uint32_t group = 0x1b10; uint32_t following = u32(f.data, group + 0x50);
    std::vector<uint32_t> seen;
    while (u32(f.data, following + 0x40) == group) {
        if (std::find(seen.begin(), seen.end(), following) != seen.end()) fail("cyclic original page traversal");
        seen.push_back(following); following = u32(f.data, following + 0x50);
    }
    const size_t count = rows.size();
    for (size_t i = 0; i < count; ++i) {
        const uint32_t p = rows[i].placeable, nav = rows[i].selector;
        put32(f.data, p + 0x50, i ? rows[i - 1].placeable : following);
        put32(f.data, p + 0x58, i + 1 < count ? rows[i + 1].placeable : group);
        put32(f.data, nav + 0x110, rows[(i + count - 1) % count].placeable);
        put32(f.data, nav + 0x118, rows[(i + 1) % count].placeable);
    }
    std::vector<uint32_t> links;
    for (const auto& r : rows) for (uint32_t field : {0x110u, 0x118u}) links.push_back(r.selector + field);
    f.add_relocations("ROFS", links);
    const uint32_t table = u32(f.data, group + 0x48);
    std::vector<uint32_t> placeables; for (const auto& r : rows) placeables.push_back(r.placeable);
    f.replace_list(table, placeables);
    put32(f.data, group + 0x50, rows.back().placeable); put32(f.data, following + 0x58, rows.front().placeable);
    // The root graph focuses its named first gameplay control: bind it to the first row.
    const std::string original = "Comp__SettingSelector004";
    n.set("CTRPort_Original_Detached_FirstGameplayRow", n.at(original));
    const uint32_t root = n.at("Scripts_Graph_Octane_Options_Menu_Page_Root");
    for (uint32_t at : f.offsets("RHND")) {
        if (at < root || at >= root + 296) continue;
        const uint32_t v = u32(f.data, at);
        if (!(v & 0x80000000u)) continue;
        const auto handles = f.externals(true);
        if ((v & 0x7fffffffu) >= handles.size()) fail("IGZ handle outside externals");
        if (handles[v & 0x7fffffffu] == std::make_pair(original, project)) put32(f.data, at, f.add_external(rows.front().name, project, true));
    }
    f.replace_names(n); f.validate();
    info = {project, true, {}};
    for (const auto& r : rows) info.rows.push_back({r.name, r.placeable, r.control->labels, r.control});
    return f.serialize();
}

// Hub page (1..6 rows) cloned from the 1.21 Options menu; each row opens a child project.
inline Bytes make_hub121(const Bytes& source, const std::string& project, const std::vector<Child>& children, Info& info) {
    pinned(source, kMenuSha, "options menu");
    if (children.empty() || children.size() > 6) fail("native hub requires 1..6 children");
    Igz f(source); Names n = f.named_objects(); rename_project(f, kMenu, project);
    Igz original(source);
    const auto original_rofs = original.offsets("ROFS");
    info = {project, false, {}};
    std::vector<uint32_t> navs;
    for (size_t i = 0; i < children.size(); ++i) {
        const std::array<std::pair<uint32_t, uint32_t>, 3> spans{{{0xeb0, 0xfd0}, {0xfd0, 0x1250}, {0x3288, 0x36a8}}};
        std::array<uint32_t, 3> deltas{};
        for (size_t s = 0; s < spans.size(); ++s) deltas[s] = f.clone_range(spans[s].first, spans[s].second);
        const auto remap = [&](uint32_t v) {
            for (size_t s = 0; s < spans.size(); ++s) if (spans[s].first <= v && v < spans[s].second) return v + deltas[s];
            return v;
        };
        // clone_range relocates references inside each block; cross-block references
        // are rebound from the original bytes.
        for (size_t s = 0; s < spans.size(); ++s)
            for (uint32_t at : original_rofs)
                if (spans[s].first <= at && at < spans[s].second) put32(f.data, uint64_t(at) + deltas[s], remap(u32(original.data, at)));
        const uint32_t p = remap(0xeb0), nav = remap(0x1010), opening = remap(0x32e8);
        put32(f.data, opening + 0x30, f.add_external(children[i].project, children[i].project, true));
        putf(f.data, p + 0x64, -210.0 + double(i) * 105.0);
        const std::string name = "CTRPort_Category_" + std::to_string(i + 1);
        n.set(name, p); n.set(name + "_Navigation", nav);
        info.rows.push_back({name, p, children[i].labels, nullptr});
        navs.push_back(nav);
    }
    const uint32_t group = n.at("Null_Asset_Buttons"), table = u32(f.data, group + 0x48);
    const uint32_t following = u32(original.data, n.at("Comp__Menu_ButtonWithIcon006") + 0x58);
    std::vector<uint32_t> placeables; for (const auto& r : info.rows) placeables.push_back(r.placeable);
    f.replace_list(table, placeables);
    put32(f.data, group + 0x58, placeables.front()); put32(f.data, following + 0x50, placeables.back());
    const size_t count = placeables.size();
    for (size_t i = 0; i < count; ++i) {
        const uint32_t p = placeables[i], nav = navs[i];
        put32(f.data, p + 0x50, i ? placeables[i - 1] : group);
        put32(f.data, p + 0x58, i + 1 < count ? placeables[i + 1] : following);
        put32(f.data, nav + 0x118, placeables[(i + 1) % count]);
        put32(f.data, nav + 0x110, placeables[(i + count - 1) % count]);
    }
    n.set("CTRPort_Original_Detached_FirstMenuRow", n.at("Comp__Menu_ButtonWithIcon"));
    put32(f.data, n.at("Root_placeable_behaviors_0") + 0x50, f.add_external(info.rows.front().name, project, true));
    n.set(project, n.pop(kMenu));
    f.replace_names(n); f.validate();
    return f.serialize();
}

// Per-locale strings of a hub or page: row titles, option labels.
inline Bytes make_locales(const Bytes& source, size_t locale, const Info& info) {
    Igz f(source); const std::string& project = info.project;
    rename_project(f, info.page ? kPage : kMenu, project);
    const uint32_t data_list = f.first("igLocalizedStringDataList");
    auto entries = list_values(f, data_list);
    uint32_t list_list = 0; std::vector<uint32_t> lists;
    if (info.page) { list_list = f.first("igLocalizedListList"); lists = list_values(f, list_list); }
    for (const auto& row : info.rows) {
        std::string title = row.labels[locale];
        if (info.page) {
            if (row.control->restart) title += " (" + label(locale, 8) + ")";
            entries.push_back(add_localization(f, title, row.name + "_Selector", project, "CGuiBehaviorBaseSettingSelector::_optionName"));
            const auto& localized = row.control->value_labels[locale];
            lists.push_back(add_localized_list(f, localized.empty() ? display_values(*row.control) : localized, row.name + "_Selector", project));
        }
        entries.push_back(add_localization(f, title, row.name + "_Navigation", project, "CGuiBehaviorNavigationItem::_itemText"));
    }
    f.replace_list(data_list, entries);
    if (info.page) f.replace_list(list_list, lists);
    return f.serialize();
}

// UI domain: registers every added project.
inline Bytes make_domain(const Bytes& source, const char* sha, uint32_t list_size, const std::vector<std::string>& projects) {
    pinned(source, sha, "UI domain");
    Igz f(source);
    require(f, 56, "CJuiceDomain", 40); require(f, 96, "igStringRefList", list_size);
    if (u32(f.data, 56 + 16) != 96) fail("domain project list layout changed");
    std::vector<std::string> values;
    for (uint32_t i = 0; i < u32(f.data, 96 + 12); ++i) values.push_back(f.string(u32(f.data, 96 + 32) + i * 8));
    for (const auto& p : projects) values.push_back("UI/" + p + ".igz");
    f.replace_list(96, values); f.validate();
    return f.serialize();
}
// UI preload package: graph first, localized fields next, project before its domain.
inline Bytes make_package(const Bytes& source, const char* sha, uint32_t info_size, bool base, const std::vector<std::string>& projects) {
    pinned(source, sha, "UI preload package");
    Igz f(source);
    require(f, 48, "igStreamingChunkInfo", info_size);
    const uint32_t count = u32(f.data, 48 + 40), buf = u32(f.data, 48 + 56);
    if ((base && count != 251) || u32(f.data, 48 + 48) != count * 16) fail("streaming chunk pair-vector layout changed");
    std::vector<std::pair<std::string, std::string>> pairs;
    for (uint32_t i = 0; i < count; ++i) pairs.emplace_back(f.string(buf + i * 16), f.string(buf + i * 16 + 8));
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, std::string>>>> additions(3);
    additions[0].first = "vsc"; additions[1].first = "lang_file"; additions[2].first = "gui_project";
    for (const auto& p : projects) additions[0].second.emplace_back("vsc", "vsc/UI/" + p + "/" + p + "_Root.igz");
    for (const auto& p : projects) for (const char* l : menu_catalog::locales) additions[1].second.emplace_back("lang_file", "ui/" + p + "_" + l + ".lng");
    for (const auto& p : projects) additions[2].second.emplace_back("gui_project", "ui/" + p + ".igz");
    std::vector<bool> done(3, false);
    std::vector<std::pair<std::string, std::string>> result;
    const auto take = [&](const std::string& kind) {
        for (size_t k = 0; k < 3; ++k)
            if (additions[k].first == kind) {
                if (done[k]) fail("preload group added twice");
                done[k] = true; result.insert(result.end(), additions[k].second.begin(), additions[k].second.end()); return;
            }
    };
    const auto pending = [&](const std::string& kind) { for (size_t k = 0; k < 3; ++k) if (additions[k].first == kind) return !done[k]; return false; };
    for (size_t i = 0; i < pairs.size(); ++i) {
        std::string path = pairs[i].second;
        if (!base) std::transform(path.begin(), path.end(), path.begin(), [](char c) { return char(c >= 'A' && c <= 'Z' ? c + 32 : c); });
        if (path == "ui/octane_options_menu.igz") take("gui_project");
        result.push_back(pairs[i]);
        if (pairs[i].first != "gui_project" && pending(pairs[i].first) && (i + 1 == pairs.size() || pairs[i + 1].first != pairs[i].first)) take(pairs[i].first);
    }
    if (std::find(done.begin(), done.end(), false) != done.end()) fail("missing original preload resource type group");
    Bytes payload(result.size() * 16, 0);
    for (size_t i = 0; i < result.size(); ++i) { put32(payload, i * 16, f.add_string(result[i].first)); put32(payload, i * 16 + 8, f.add_string(result[i].second)); }
    const uint32_t at = f.append(payload);
    put32(f.data, 48 + 40, result.size()); put32(f.data, 48 + 48, payload.size()); put32(f.data, 48 + 56, at);
    std::vector<uint32_t> added; for (size_t i = 0; i < result.size() * 2; ++i) added.push_back(uint32_t(at + i * 8));
    f.add_relocations("RSTT", added); f.validate();
    return f.serialize();
}
// 1.21 Options menu: the update splits gameplay navigation from its VSC/animation block,
// so both are cloned and their cross-block relocations rebound explicitly.
inline Bytes make_menu121(const Bytes& source) {
    pinned(source, kMenuSha, "options menu");
    Igz f(source); Names n = f.named_objects();
    require(f, 0xeb0, "igGuiPlaceable", 288); require(f, 0xfd0, "igGuiBehaviorList", 64);
    require(f, 0x32b8, "igGuiVscBehavior", 48); require(f, 0x32e8, "igGuiVscBehavior", 128);
    const std::array<std::pair<uint32_t, uint32_t>, 3> spans{{{0xeb0, 0xfd0}, {0xfd0, 0x1250}, {0x3288, 0x36a8}}};
    std::array<uint32_t, 3> deltas{};
    for (size_t s = 0; s < spans.size(); ++s) deltas[s] = f.clone_range(spans[s].first, spans[s].second);
    const auto remap = [&](uint32_t v) {
        for (size_t s = 0; s < spans.size(); ++s) if (spans[s].first <= v && v < spans[s].second) return v + deltas[s];
        return v;
    };
    Igz original(source);
    for (size_t s = 0; s < spans.size(); ++s)
        for (uint32_t at : original.offsets("ROFS"))
            if (spans[s].first <= at && at < spans[s].second) put32(f.data, uint64_t(at) + deltas[s], remap(u32(original.data, at)));
    const uint32_t p = remap(0xeb0), nav = remap(0x1010), opening = remap(0x32e8);
    const uint32_t table = u32(f.data, n.at("Null_Asset_Buttons") + 0x48);
    auto children = list_values(f, table); children.push_back(p);
    f.replace_list(table, children);
    const uint32_t language = n.at("Comp__Menu_ButtonWithIcon006"), following = u32(f.data, language + 0x58);
    put32(f.data, language + 0x58, p); put32(f.data, p + 0x50, language); put32(f.data, p + 0x58, following); put32(f.data, following + 0x50, p);
    put32(f.data, nav + 0x110, language); put32(f.data, nav + 0x118, 0xeb0);
    put32(f.data, 0x1010 + 0x110, p); put32(f.data, 0x2c28 + 0x118, p);
    // PS4 hides its platform-language row; use the sixth visible row location.
    putf(f.data, p + 0x64, 315.0);
    put32(f.data, opening + 0x30, f.add_external(kCustom, kCustom, true));
    n.set("CTRPort_PC_Settings_Button", p); n.set("CTRPort_PC_Settings_Navigation", nav); n.set("CTRPort_PC_Settings_Behaviors", remap(0xfd0));
    n.set("CTRPort_PC_Settings_SelectPage", remap(0x32b8)); n.set("CTRPort_PC_Settings_OpenPage", opening);
    f.replace_names(n); f.validate();
    return f.serialize();
}

// Balanced six-way native hubs; leaf pages hold at most seven selectors (plan_pages).
inline std::vector<Node> plan_pages() {
    std::vector<Node> nodes;
    const auto hub = [&](const std::string& project, std::vector<Child> children, const Labels& labels) {
        int level = 0;
        while (children.size() > 6) {
            ++level; std::vector<Child> grouped;
            for (size_t i = 0; i < children.size(); i += 6) {
                const std::string name = project + "_Level" + std::to_string(level) + "_Group" + std::to_string(i / 6 + 1);
                Labels text; for (size_t l = 0; l < kLocales; ++l) text[l] = labels[l] + " " + std::to_string(i / 6 + 1);
                nodes.push_back({name, false, {}, std::vector<Child>(children.begin() + i, children.begin() + (std::min)(children.size(), i + 6)), text});
                grouped.push_back({name, text});
            }
            children = grouped;
        }
        nodes.push_back({project, false, {}, children, labels});
    };
    std::vector<Child> categories;
    for (const auto& category : menu_catalog::categories()) {
        std::vector<const catalog_control*> members;
        for (const auto& c : menu_catalog::controls()) if (c.category == category.key) members.push_back(&c);
        if (members.empty()) continue;
        const std::string base = std::string(kCustom) + "_" + category.key;
        std::vector<Child> pages;
        for (size_t offset = 0; offset < members.size(); offset += 7) {
            const std::string project = base + "_Page" + std::to_string(offset / 7 + 1);
            Labels title;
            for (size_t l = 0; l < kLocales; ++l) title[l] = category.labels[l] + (members.size() > 7 ? " " + std::to_string(offset / 7 + 1) : "");
            nodes.push_back({project, true, std::vector<const catalog_control*>(members.begin() + offset, members.begin() + (std::min)(members.size(), offset + 7)), {}, title});
            pages.push_back({project, title});
        }
        if (pages.size() == 1) categories.push_back(pages.front());
        else { hub(base, pages, category.labels); categories.push_back({base, category.labels}); }
    }
    Labels title; for (size_t l = 0; l < kLocales; ++l) title[l] = label(l, 0);
    hub(kCustom, categories, title);
    return nodes;
}

constexpr const char* kBaseDomainSha = "42a11060ee5d0d097d143c71e50aefb618f56fac6c8e8939a2b7f9fe023fa257";
constexpr const char* kBasePackageSha = "c76651ed98e11813ebdab2165c9e8f8c2fddfaccdcdd9c0002596493316c53aa";
constexpr const char* kDomainSha = "bf7396da373c73a4793ac673f15bda83f22a2bce7a5eb9c66bff9c3c38272ef9";
constexpr const char* kPackageSha = "59ccc91b2078239d6d601f2df138d92f793db062fb8cb3f9bee9594d8883b246";
constexpr const char* kDomainPath = "UI/Domains/juicedomain_permanent.igz";
constexpr const char* kPackagePath = "packages/generated/UI/Domains/juicedomain_permanent_pkg.igz";

struct Generated {
    std::vector<std::pair<std::string, Bytes>> files;          // resources added to the base archive
    Bytes juicedomain;                                          // archives/juicedomain_permanent.pak
    std::vector<std::pair<std::string, Bytes>> update, update2; // entry replacements per update layer
    std::vector<std::string> projects;
};

// Inputs: the effective archives/juicedomain_permanent.pak, archives/update.pak and
// archives/update2.pak of the user's retail 1.21 installation.
inline Generated generate(const fs::path& juicedomain, const fs::path& update, const fs::path& update2) {
    Generated out;
    Iga base(juicedomain, true), latest(update2, false);
    std::array<Bytes, 2> templates{latest.extract(std::string("ui/") + kMenu + ".igz"), latest.extract(std::string("ui/") + kPage + ".igz")};
    pinned(templates[0], kMenuSha, "options menu"); pinned(templates[1], kPageSha, "options page");
    std::array<Bytes, 2> graphs{latest.extract(std::string("vsc/UI/") + kMenu + "/" + kMenu + "_Root.igz"),
                                latest.extract(std::string("vsc/UI/") + kPage + "/" + kPage + "_Root.igz")};
    pinned(graphs[0], "62fda0c91e6d6749672981779f166d7607bfb06e8a92aed4b3822c3013127524", "menu lifecycle graph");
    pinned(graphs[1], "8c1b64661ede8041ac607f547125aaed821f90770885540775b962b6e404a745", "page lifecycle graph");
    std::array<std::array<Bytes, kLocales>, 2> locales;
    for (size_t k = 0; k < 2; ++k)
        for (size_t l = 0; l < kLocales; ++l) locales[k][l] = latest.extract(std::string("ui/") + (k ? kPage : kMenu) + "_" + menu_catalog::locales[l] + ".lng");

    const Bytes base_menu = base.extract(std::string("ui/") + kMenu + ".igz");
    pinned(base_menu, "b346aa5b94312d7df03ee907fbed99346e7eaf8f7d53669ef401b8ab43df4073", "base options menu");
    out.files.emplace_back(std::string("ui/") + kMenu + ".igz", make_menu(base_menu));
    for (size_t l = 0; l < kLocales; ++l) {
        const std::string path = std::string("ui/") + kMenu + "_" + menu_catalog::locales[l] + ".lng";
        out.files.emplace_back(path, make_language(base.extract(path), l));
    }
    for (const auto& node : plan_pages()) {
        Info info;
        const Bytes page = node.page ? make_page121(templates[1], node.controls, node.project, info) : make_hub121(templates[0], node.project, node.children, info);
        out.files.emplace_back("ui/" + node.project + ".igz", page);
        Igz graph(graphs[node.page ? 1 : 0]); rename_project(graph, node.page ? kPage : kMenu, node.project);
        out.files.emplace_back("vsc/UI/" + node.project + "/" + node.project + "_Root.igz", graph.serialize());
        for (size_t l = 0; l < kLocales; ++l)
            out.files.emplace_back("ui/" + node.project + "_" + menu_catalog::locales[l] + ".lng", make_locales(locales[node.page ? 1 : 0][l], l, info));
        out.projects.push_back(node.project);
    }
    out.files.emplace_back(kDomainPath, make_domain(base.extract(kDomainPath), kBaseDomainSha, 128, out.projects));
    out.files.emplace_back(kPackagePath, make_package(base.extract(kPackagePath), kBasePackageSha, 4080, true, out.projects));
    out.juicedomain = build_archive(base, out.files);

    // The update layers override the base archive's menu, domain and preload package.
    Iga first(update, false);
    out.update.emplace_back(kDomainPath, make_domain(first.extract(kDomainPath), kDomainSha, 152, out.projects));
    out.update2.emplace_back(std::string("ui/") + kMenu + ".igz", make_menu121(templates[0]));
    for (size_t l = 0; l < kLocales; ++l)
        out.update2.emplace_back(std::string("ui/") + kMenu + "_" + menu_catalog::locales[l] + ".lng", make_language(locales[0][l], l));
    out.update2.emplace_back(kPackagePath, make_package(latest.extract(kPackagePath), kPackageSha, 4880, false, out.projects));
    return out;
}
}  // namespace ctrport::settings::menu_assets
