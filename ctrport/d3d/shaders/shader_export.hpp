// SPDX-License-Identifier: MIT
// C++ port of FrankHUN88453/CTR-NitroFueled-PC 96b4aa5474b1d42b0d9f383349dc091ea9a2d385
// tools/igz.py, tools/export_shaders.py and tools/import_runtime_shaders.py (MIT,
// ctrport/d3d/LICENSE.FrankHUN88453): the shader buffers of the user's shaders_ps4.pak (and patch
// archives) with their Gnm shader structs, plus the shaders ctr_d3d met at run time.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "gcn_hlsl.hpp"

namespace gcn {

using Bytes = std::vector<uint8_t>;

[[noreturn]] inline void export_fail(const std::string& what) { throw std::runtime_error(what); }
inline uint32_t rd32(const Bytes& b, uint64_t at) {
    if (at + 4 > b.size()) export_fail("read outside data");
    uint32_t v;
    memcpy(&v, b.data() + at, 4);
    return v;
}
inline uint64_t rd64(const Bytes& b, uint64_t at) {
    if (at + 8 > b.size()) export_fail("read outside data");
    uint64_t v;
    memcpy(&v, b.data() + at, 8);
    return v;
}
inline uint8_t rd8(const Bytes& b, uint64_t at) {
    if (at >= b.size()) export_fail("read outside data");
    return b[at];
}

// Alchemy IGZ version 10 (PS4): sections, then the fixup chunks; only what the shader export needs.
// Offsets inside the data are encoded as (data section << 27) | offset.
class IgzFile {
public:
    explicit IgzFile(Bytes blob) : b(std::move(blob)) {
        if (rd32(b, 0) != 0x49475A01) export_fail("not an IGZ file");
        const uint32_t nfix = rd32(b, 16);
        for (uint64_t o = 0x14; o + 16 <= 0x800; o += 16) {
            const uint32_t off = rd32(b, o + 4), length = rd32(b, o + 8);
            if (off == 0 && length == 0) break;
            sections.push_back({off, length});
        }
        if (sections.empty()) export_fail("IGZ without sections");
        uint64_t p = sections[0].first;
        const uint64_t fend = p + sections[0].second;
        for (uint32_t k = 0; k < nfix && p < fend; k++) {
            const std::string tag(reinterpret_cast<const char*>(b.data() + p), 4);
            const uint32_t count = rd32(b, p + 4), length = rd32(b, p + 8), start = rd32(b, p + 12);
            if (p + length > b.size() || start > length) export_fail("IGZ fixup outside file");
            const uint8_t* data = b.data() + p + start;
            const size_t size = length - start;
            if (tag == "TMET") types = cstrings(data, size, count);
            else if (tag == "RVTB") vtables = offsets(data, size, count);
            p += length;
        }
    }
    uint64_t file_offset(uint64_t enc) const {
        const uint64_t s = 1 + (enc >> 27);
        if (s >= sections.size()) export_fail("IGZ offset in a missing section");
        return sections[s].first + (enc & 0x7FFFFFF);
    }
    uint64_t u64(uint64_t enc) const { return rd64(b, file_offset(enc)); }
    // (object offset, type name) of every object with a vtable
    std::vector<std::pair<uint64_t, std::string>> objects() const {
        std::vector<std::pair<uint64_t, std::string>> out;
        for (uint32_t off : vtables) {
            const uint64_t idx = u64(off);
            out.push_back({off, idx < types.size() ? types[idx] : "#" + std::to_string(idx)});
        }
        return out;
    }
    Bytes b;

private:
    // packed offset lists: nibbles low first, 3 data bits + continuation bit; offset += v * 4
    static std::vector<uint32_t> offsets(const uint8_t* d, size_t n, uint32_t count) {
        std::vector<uint32_t> out;
        uint32_t prev = 0;
        size_t i = 0;
        for (uint32_t k = 0; k < count; k++) {
            uint32_t v = 0, shift = 0;
            for (;;) {
                if (i / 2 >= n) export_fail("IGZ offset list truncated");
                const uint8_t nib = (i & 1) ? d[i / 2] >> 4 : d[i / 2] & 0xF;
                i++;
                v |= uint32_t(nib & 7) << shift;
                shift += 3;
                if (!(nib & 8)) break;
            }
            prev += v * 4;
            out.push_back(prev);
        }
        return out;
    }
    static std::vector<std::string> cstrings(const uint8_t* d, size_t n, uint32_t count) {
        std::vector<std::string> out;
        size_t p = 0;
        for (uint32_t k = 0; k < count; k++) {
            const void* z = p < n ? memchr(d + p, 0, n - p) : nullptr;
            if (!z) export_fail("IGZ string list truncated");
            const size_t e = static_cast<const uint8_t*>(z) - d;
            out.emplace_back(reinterpret_cast<const char*>(d + p), e - p);
            p = e + 1;
            if (p & 1) p++;  // strings are 2-byte aligned
        }
        return out;
    }
    std::vector<std::pair<uint32_t, uint32_t>> sections;  // file offset, length
    std::vector<std::string> types;
    std::vector<uint32_t> vtables;
};

// Gnm shader structs (VsShader, PsShader, EsShader, GsShader with its copy VsShader)
inline std::vector<Slot> slots_at(const Bytes& b, uint64_t& o, int n) {
    std::vector<Slot> out;
    for (int k = 0; k < n; k++, o += 4) out.push_back({rd8(b, o), rd8(b, o + 1), rd8(b, o + 2), rd8(b, o + 3)});
    return out;
}
inline GnmMeta parse_gnm(const std::string& kind, const Bytes& b) {
    GnmMeta m;
    const uint32_t common = rd32(b, 0);
    const int nslots = common >> 24;
    m.shader_size = common & 0x7FFFFF;
    auto regs = [&](std::initializer_list<const char*> names) {
        uint64_t o = 8;
        for (const char* n : names) m.regs[n] = rd32(b, o), o += 4;
    };
    if (kind == "vs") {
        regs({"pgm_lo", "pgm_hi", "rsrc1", "rsrc2", "vs_out_config", "pos_format", "cl_vs_out_cntl"});
        const int n_in = rd8(b, 36), n_exp = rd8(b, 37);
        uint64_t o = 40;
        m.slots = slots_at(b, o, nslots);
        for (int k = 0; k < n_in; k++, o += 4) {
            Input in;
            in.sem = rd8(b, o), in.vgpr = rd8(b, o + 1), in.size = rd8(b, o + 2);
            m.inputs.push_back(in);
        }
        for (int k = 0; k < n_exp; k++, o += 2) m.exports.push_back({rd8(b, o), rd8(b, o + 1) & 0x1F});
    } else if (kind == "ps") {
        regs({"pgm_lo", "pgm_hi", "rsrc1", "rsrc2", "z_format", "col_format", "input_ena", "input_addr", "in_control",
              "baryc_cntl", "db_shader_control", "cb_shader_mask"});
        const int n_in = rd8(b, 56);
        uint64_t o = 60;
        m.slots = slots_at(b, o, nslots);
        for (int k = 0; k < n_in; k++, o += 2) {
            const uint32_t v = rd8(b, o) | (rd8(b, o + 1) << 8);
            Input in;
            in.sem = v & 0xFF, in.def = (v >> 8) & 3, in.flat = (v >> 10) & 1, in.linear = (v >> 11) & 1, in.custom = (v >> 12) & 1;
            m.inputs.push_back(in);
        }
    } else if (kind == "es") {
        regs({"pgm_lo", "pgm_hi", "rsrc1", "rsrc2"});
        const int n_in = rd8(b, 0x18);
        uint64_t o = 0x20;
        m.slots = slots_at(b, o, nslots);
        for (int k = 0; k < n_in; k++, o += 4) {
            Input in;
            in.sem = rd8(b, o), in.vgpr = rd8(b, o + 1), in.size = rd8(b, o + 2);
            m.inputs.push_back(in);
        }
    } else if (kind == "gs") {
        regs({"pgm_lo", "pgm_hi", "rsrc1", "rsrc2"});
        uint64_t o = 0x38;
        m.slots = slots_at(b, o, nslots);
        if (o > b.size()) export_fail("GS struct truncated");
        m.copy = std::make_shared<GnmMeta>(parse_gnm("vs", Bytes(b.begin() + o, b.end())));
    }
    return m;
}

struct ShaderEntry {
    std::string name, effect, kind;
    uint32_t hash = 0;
    Bytes code;
    GnmMeta gnm;
};

inline std::string kind_name(int32_t stype) {
    switch (stype) {
    case 0: return "ps";
    case 1: return "vs";
    case 2: return "gs";
    case 3: return "es";  // export half of a VS->GS pipeline
    default: return "t" + std::to_string(stype);
    }
}

// every igShaderBuffer of one IGZ file: <effect>_<hash>_<kind>
inline void export_igz(const IgzFile& z, const std::string& effect, std::vector<ShaderEntry>& out) {
    for (const auto& [off, t] : z.objects()) {
        if (t != "igShaderBuffer") continue;
        const uint64_t at = z.file_offset(off + 0x10);
        ShaderEntry e;
        e.effect = effect;
        e.kind = kind_name(int32_t(rd32(z.b, at)));
        e.hash = rd32(z.b, at + 4);
        const uint64_t hdr = z.u64(off + 0x18);
        const uint64_t size = z.u64(off + 0x20) & 0xFFFFFFF, ptr = z.u64(off + 0x28);
        const uint64_t code = z.file_offset(ptr);
        if (code + size > z.b.size()) export_fail("shader code outside file");
        e.code.assign(z.b.begin() + code, z.b.begin() + code + size);
        const uint64_t shp = z.file_offset(z.u64(hdr + 0x20)), rop = z.file_offset(z.u64(hdr + 0x28));
        // a GS struct embeds its copy shader struct, which runs past the next pointer
        const uint64_t gend = e.kind == "gs" ? shp + 0x100 : rop;
        const uint64_t gs = std::min<uint64_t>(shp, z.b.size()), ge = std::clamp<uint64_t>(gend, gs, z.b.size());
        char hx[16];
        snprintf(hx, sizeof(hx), "%08x", e.hash);
        e.name = effect + "_" + hx + "_" + e.kind;
        e.gnm = parse_gnm(e.kind, Bytes(z.b.begin() + gs, z.b.begin() + ge));
        out.push_back(std::move(e));
    }
}

// shaders dumped by ctr_d3d (missing_shaders/Rt_<fnv>_<kind>.bin + .gnm), e.g. the Bink video
// shaders the eboot embeds; skipped when the same program is already in the archives
inline int import_runtime(const std::filesystem::path& dir, std::vector<ShaderEntry>& index, std::vector<std::string>& log) {
    namespace fs = std::filesystem;
    std::set<uint64_t> known;
    for (const auto& e : index)
        known.insert(fnv1a64(e.code.data(), std::min<size_t>(e.code.size(), e.gnm.shader_size)));
    std::vector<fs::path> files;
    std::error_code ec;
    for (const auto& it : fs::directory_iterator(dir, ec)) {
        const std::string n = it.path().filename().string();
        if (n.rfind("Rt_", 0) == 0 && it.path().extension() == ".bin") files.push_back(it.path());
    }
    std::sort(files.begin(), files.end());
    auto slurp = [](const fs::path& p) {
        std::ifstream f(p, std::ios::binary);
        return Bytes(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    };
    int added = 0;
    for (const auto& p : files) {
        const std::string base = p.stem().string();
        const auto parts = split(base, '_');
        if (parts.size() != 3) continue;
        ShaderEntry e;
        e.name = base, e.effect = "Runtime", e.kind = parts[2];
        e.code = slurp(p);
        if (known.count(fnv1a64(e.code.data(), e.code.size()))) continue;  // translated from the pak meanwhile
        fs::path gnm = p;
        gnm.replace_extension(".gnm");
        if (!fs::exists(gnm)) {
            log.push_back(base + ": no Gnm struct dump, skipped");
            continue;
        }
        e.gnm = parse_gnm(e.kind, slurp(gnm));
        if (e.gnm.shader_size != e.code.size()) {
            log.push_back(base + ": size mismatch, skipped");
            continue;
        }
        e.hash = uint32_t(std::stoul(parts[1].substr(0, 8), nullptr, 16));
        known.insert(fnv1a64(e.code.data(), e.code.size()));
        index.push_back(std::move(e));
        added++;
    }
    return added;
}

}  // namespace gcn
