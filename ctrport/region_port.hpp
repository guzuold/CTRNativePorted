// SPDX-License-Identifier: GPL-2.0-only
// Moving a patch between the EU (CUSA14876) and US (CUSA13795) eboots of the same version, and
// checking where a patch writes into code. Used by guest_patch_xml.hpp.
// - Functions: the running eboot's own function table (eh_frame_hdr), so that instructions are
//   decoded from a function start (x86_length.hpp).
// - check_write: a write into code must start where an instruction starts, or change part of one
//   instruction only (a condition, an immediate). A line written for another eboot usually starts
//   in the middle of an instruction and runs over the next ones instead.
// - Map: the ranges of tools/region_map.py (region_map_data.hpp), proven from both eboots, from
//   the patch's region to the running one.
// - relocate: the RIP-relative references and rel32 branches inside written instructions are
//   moved so that they reach the same places from the new address.
#pragma once
#include "region_map_data.hpp"
#include "x86_length.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
namespace ctrport::region_port {

inline std::string hex(std::uint64_t v) {
    char b[32];
    std::snprintf(b, sizeof(b), "0x%llx", static_cast<unsigned long long>(v));
    return b;
}

// --- The running eboot's functions --------------------------------------------------------
struct Functions {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> list; // (start, end), sorted
    // the function containing offset, or nullptr
    const std::pair<std::uint64_t, std::uint64_t>* find(std::uint64_t offset) const {
        auto it = std::upper_bound(list.begin(), list.end(), offset,
                                   [](std::uint64_t v, const auto& f) { return v < f.first; });
        if (it == list.begin()) return nullptr;
        --it;
        return offset < it->second ? &*it : nullptr;
    }
};
// The eh_frame_hdr binary search table (version 1, eh_frame_ptr pcrel sdata4, fde_count udata4,
// table datarel sdata4) and the FDE sizes it points to, read inside [0, size).
inline Functions read_functions(const unsigned char* image, std::uint64_t size) {
    Functions out;
    const auto i32 = [&](std::uint64_t at) {
        std::int32_t v;
        std::memcpy(&v, image + at, 4);
        return static_cast<std::int64_t>(v);
    };
    static const unsigned char magic[4] = {0x01, 0x1b, 0x03, 0x3b};
    for (std::uint64_t at = 0; at + 12 <= size; ++at) {
        at = static_cast<std::uint64_t>(std::search(image + at, image + size, magic, magic + 4) - image);
        if (at + 12 > size) break;
        std::uint32_t count;
        std::memcpy(&count, image + at + 8, 4);
        if (count < 1000 || count > 1000000 || at + 12 + std::uint64_t(count) * 8 > size) continue;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> list;
        bool ok = true;
        for (std::uint32_t k = 0; k < count && ok; ++k) {
            const std::int64_t start = static_cast<std::int64_t>(at) + i32(at + 12 + k * 8ull);
            const std::int64_t fde = static_cast<std::int64_t>(at) + i32(at + 16 + k * 8ull);
            ok = start >= 0 && fde >= 0 && static_cast<std::uint64_t>(fde) + 16 <= size &&
                 fde + 8 + i32(static_cast<std::uint64_t>(fde) + 8) == start &&
                 (list.empty() || start >= static_cast<std::int64_t>(list.back().first));
            if (ok) {
                const std::int64_t length = i32(static_cast<std::uint64_t>(fde) + 12);
                ok = length > 0 && static_cast<std::uint64_t>(start + length) <= size;
                list.push_back({static_cast<std::uint64_t>(start), static_cast<std::uint64_t>(start + length)});
            }
        }
        if (ok) {
            out.list = std::move(list);
            return out;
        }
    }
    return out;
}

// The instruction of the running eboot that contains offset, decoded from its function start;
// false when offset is outside the functions or the function does not decode up to it.
inline bool instruction_at(const unsigned char* image, const Functions& functions, std::uint64_t offset,
                           std::uint64_t& start, x86::Insn& insn) {
    const auto* f = functions.find(offset);
    if (!f) return false;
    for (std::uint64_t at = f->first; at < f->second;) {
        insn = x86::decode(image + at, static_cast<std::size_t>(f->second - at));
        if (!insn.length) return false;
        if (offset < at + insn.length) {
            start = at;
            return true;
        }
        at += insn.length;
    }
    return false;
}

// Why a write of size bytes at offset does not fit the code there; empty when it fits, or when
// offset is not inside a function of the running eboot (data, caves) or its function does not
// decode up to it (a jump table inside the function).
inline std::string check_write(const unsigned char* image, const Functions& functions, std::uint64_t offset,
                               std::size_t size) {
    std::uint64_t start;
    x86::Insn insn;
    if (!instruction_at(image, functions, offset, start, insn)) return {};
    if (offset == start || offset + size <= start + insn.length) return {};
    return "the write at " + hex(offset + 0x400000) + " starts in the middle of the instruction at " +
           hex(start + 0x400000) + " and goes past it (written for another version or region of the game?)";
}

// --- The address map ----------------------------------------------------------------------
struct Map {
    struct Entry {
        std::uint64_t lo, hi;
        std::int64_t delta; // to the running eboot
    };
    std::vector<Entry> code, data;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> differs; // in the patch's coordinates
    std::string version;
    static const Entry* find(const std::vector<Entry>& list, std::uint64_t at, std::size_t size) {
        auto it =
            std::upper_bound(list.begin(), list.end(), at, [](std::uint64_t v, const Entry& e) { return v < e.lo; });
        if (it == list.begin()) return nullptr;
        --it;
        return at >= it->lo && size <= it->hi - it->lo && at - it->lo <= it->hi - it->lo - size ? &*it : nullptr;
    }
    bool differ(std::uint64_t at, std::size_t size) const {
        for (const auto& [lo, hi] : differs)
            if (at < hi && lo < at + size) return true;
        return false;
    }
};
inline bool has_map(const std::string& running_title, const std::string& version) {
    if (running_title != "CUSA13795" && running_title != "CUSA14876") return false;
    for (const auto& pair : region_map::pairs)
        if (version == pair.version) return true;
    return false;
}
// The map from the other region's eboot of version (e.g. "01.21") to the running one, whose
// title is running_title; false when none was made.
inline bool map_for(const std::string& running_title, const std::string& version, Map& map) {
    const bool to_us = running_title == "CUSA13795";
    if (!to_us && running_title != "CUSA14876") return false;
    for (const auto& pair : region_map::pairs) {
        if (version != pair.version) continue;
        map = {};
        map.version = version;
        const auto add = [&](const region_map::Range* r, std::size_t n, std::vector<Map::Entry>& out) {
            for (std::size_t i = 0; i < n; ++i) {
                if (!r[i].size) continue;
                const std::int64_t lo = to_us ? r[i].eu : static_cast<std::int64_t>(r[i].eu) + r[i].delta;
                out.push_back({static_cast<std::uint64_t>(lo), static_cast<std::uint64_t>(lo) + r[i].size,
                               to_us ? r[i].delta : -static_cast<std::int64_t>(r[i].delta)});
            }
            std::sort(out.begin(), out.end(), [](const Map::Entry& a, const Map::Entry& b) { return a.lo < b.lo; });
        };
        add(pair.code, pair.code_count, map.code);
        add(pair.data, pair.data_count, map.data);
        for (std::size_t i = 0; i < pair.differs_count; ++i) {
            const auto& s = pair.differs[i];
            if (!s.size) continue;
            std::int64_t lo = s.eu;
            if (!to_us) // the differing EU span in US coordinates, through its data range
                for (std::size_t k = 0; k < pair.data_count; ++k)
                    if (s.eu >= pair.data[k].eu && s.eu < pair.data[k].eu + pair.data[k].size) lo += pair.data[k].delta;
            map.differs.push_back({static_cast<std::uint64_t>(lo), static_cast<std::uint64_t>(lo) + s.size});
        }
        return true;
    }
    return false;
}

// --- Moving written instructions ----------------------------------------------------------
// target(source address) -> running address, or false when it has no counterpart.
using Target = std::function<bool(std::uint64_t, std::uint64_t&)>;

// Rewrites the references of the instructions in bytes, written at to (running eboot) instead of
// from (the patch's eboot). Instructions are decoded from the start of bytes, followed by the
// running eboot's bytes after the write (an instruction may end in the original code). A
// reference field entirely inside bytes is moved; one only partly inside is refused.
inline void relocate(std::vector<unsigned char>& bytes, std::uint64_t from, std::uint64_t to,
                     const unsigned char* image, std::uint64_t image_size, const Target& target) {
    std::vector<unsigned char> view(bytes);
    for (std::uint64_t k = 0; k < 16 && to + bytes.size() + k < image_size; ++k)
        view.push_back(image[to + bytes.size() + k]);
    for (std::size_t at = 0; at < bytes.size();) {
        const auto insn = x86::decode(view.data() + at, view.size() - at);
        if (!insn.length)
            throw std::runtime_error("the written bytes at " + hex(from + at + 0x400000) +
                                     " do not decode as instructions");
        const auto move = [&](int offset, int size, bool full_width) {
            if (!size) return;
            const std::size_t field = at + static_cast<std::size_t>(offset);
            if (field >= bytes.size()) return; // from the original code: already right here
            if (field + static_cast<std::size_t>(size) > bytes.size())
                throw std::runtime_error("the write at " + hex(from + at + 0x400000) + " ends inside a reference");
            std::int64_t value = 0;
            if (size == 4) {
                std::int32_t v;
                std::memcpy(&v, &bytes[field], 4);
                value = v;
            } else
                value = static_cast<std::int8_t>(bytes[field]);
            const std::uint64_t end_from = from + at + insn.length, end_to = to + at + insn.length;
            const std::uint64_t source = static_cast<std::uint64_t>(static_cast<std::int64_t>(end_from) + value);
            std::uint64_t here;
            if (!target(source, here))
                throw std::runtime_error("the instruction at " + hex(from + at + 0x400000) + " reaches " +
                                         hex(source + 0x400000) + ", which has no known counterpart in this eboot");
            const std::int64_t moved = static_cast<std::int64_t>(here) - static_cast<std::int64_t>(end_to);
            if (!full_width) {
                if (moved != value)
                    throw std::runtime_error("the short branch at " + hex(from + at + 0x400000) +
                                             " would change its target");
                return;
            }
            if (moved < INT32_MIN || moved > INT32_MAX)
                throw std::runtime_error("the instruction at " + hex(from + at + 0x400000) +
                                         " cannot reach its target");
            const std::int32_t d = static_cast<std::int32_t>(moved);
            std::memcpy(&bytes[field], &d, 4);
        };
        if (insn.rip) move(insn.disp_offset, insn.disp_size, true);
        if (insn.relative) move(insn.imm_offset, insn.imm_size, insn.imm_size == 4);
        at += static_cast<std::size_t>(insn.length);
    }
}

} // namespace ctrport::region_port
