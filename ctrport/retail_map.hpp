// SPDX-License-Identifier: GPL-2.0-only
// The runtime's settings, native graphics options and menu bridge were written against the EU
// v01.21 eboot (CUSA14876, the reference). The other retail eboots (US CUSA13795 v01.00 and
// v01.21, EU v01.00) contain the same code at other addresses. prepare() looks for every binding
// of retail_anchors.hpp in the running eboot: a binding is found when its window of reference
// code occurs exactly once (bytes that change when code moves are ignored); a data binding is
// read from the code that uses it, and must agree across its windows; a vtable is the one table
// holding the functions found for its slots. A binding that is not found stays unavailable and
// the feature using it is switched off; nothing is guessed. Reference offsets map to themselves.
#pragma once
#include "retail_anchors.hpp"
#include "retail_profiles.hpp"
#include <algorithm>
#include <chrono>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ctrport::retail_map {
inline constexpr char reference_sha256[] = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
inline constexpr std::size_t missing = static_cast<std::size_t>(-1);

struct State {
    const unsigned char* image = nullptr;
    std::size_t extent = 0;
    std::string sha;
    bool reference = false;
    std::vector<std::size_t> found;  // per point: image offset in the running eboot, or missing
};
inline State state;

inline const Point* point(std::size_t reference) {
    for (const auto& p : points) if (p.reference == reference) return &p;
    return nullptr;
}

// Start offsets (in the image) of the window, up to two of them.
inline std::vector<std::size_t> occurrences(const unsigned char* image, const std::vector<std::pair<std::uint64_t, std::uint64_t>>& spans,
                                            const Window& w) {
    // Longest compared run: searched first, then the whole window with its mask.
    std::size_t best = 0, length = 0;
    for (std::size_t i = 0; i < w.size;) {
        if (w.mask[i] != 'x') { ++i; continue; }
        std::size_t j = i;
        while (j < w.size && w.mask[j] == 'x') ++j;
        if (j - i > length) { best = i; length = j - i; }
        i = j;
    }
    std::vector<std::size_t> hits;
    if (length < 4) return hits;
    const auto* key = reinterpret_cast<const unsigned char*>(w.bytes) + best;
    const std::boyer_moore_horspool_searcher searcher(key, key + length);
    for (const auto& [begin, end] : spans) {
        if (end - begin < w.size) continue;
        for (const unsigned char* at = image + begin + best;;) {
            at = std::search(at, image + end - (w.size - best) + length, searcher);
            if (at == image + end - (w.size - best) + length) break;
            const std::size_t s = static_cast<std::size_t>(at - image) - best;
            ++at;
            bool same = true;
            for (std::size_t i = 0; i < w.size && same; ++i)
                same = w.mask[i] != 'x' || image[s + i] == static_cast<unsigned char>(w.bytes[i]);
            if (!same) continue;
            hits.push_back(s);
            if (hits.size() == 2) return hits;
        }
    }
    return hits;
}

inline std::size_t resolve(const unsigned char* image, std::size_t extent, const std::vector<std::pair<std::uint64_t, std::uint64_t>>& spans,
                           const Point& p) {
    std::size_t result = missing;
    for (std::uint32_t n = 0; n < p.count; ++n) {
        const auto& w = windows[p.first + n];
        const auto hits = occurrences(image, spans, w);
        if (hits.size() != 1) continue;
        std::size_t at = hits.front() + w.site;
        if (p.kind == 1) {
            std::int32_t displacement = 0;
            std::memcpy(&displacement, image + at + w.displacement, 4);
            std::size_t reached = at + w.end + static_cast<std::ptrdiff_t>(displacement);
            if (w.indirect) {
                if (reached + 8 > extent) continue;
                std::uint64_t pointer = 0;
                std::memcpy(&pointer, image + reached, 8);
                const auto base = reinterpret_cast<std::uintptr_t>(image);
                if (pointer < base || pointer - base >= extent) continue;
                reached = static_cast<std::size_t>(pointer - base);
            }
            at = reached + w.delta;
        }
        if (at >= extent) continue;
        if (result != missing && result != at) return missing;  // windows disagree
        result = at;
    }
    return result;
}

// The table (8-byte aligned, outside code) whose listed slots hold the listed points.
inline std::size_t resolve_vtable(const unsigned char* image, std::size_t extent, const Point& p) {
    std::vector<std::pair<std::size_t, std::uint64_t>> slots;
    const auto base = reinterpret_cast<std::uintptr_t>(image);
    std::string spec = p.slots;
    for (std::size_t from = 0; from <= spec.size();) {
        auto comma = spec.find(',', from);
        if (comma == std::string::npos) comma = spec.size();
        const auto item = spec.substr(from, comma - from);
        const auto at = item.find('@');
        std::size_t target = missing;
        for (std::size_t i = 0; i < std::size(points); ++i)
            if (item.compare(0, at, points[i].name) == 0 && std::strlen(points[i].name) == at) target = state.found[i];
        if (target == missing) return missing;
        slots.emplace_back(std::stoul(item.substr(at + 1)), base + target);
        from = comma + 1;
    }
    std::size_t result = missing;
    for (std::size_t s = 0; s + 8 <= extent; s += 8) {
        std::uint64_t value = 0;
        std::memcpy(&value, image + s, 8);
        if (value != slots.front().second || s < slots.front().first * 8) continue;
        const std::size_t table = s - slots.front().first * 8;
        bool all = true;
        for (const auto& [slot, expected] : slots) {
            if (table + slot * 8 + 8 > extent) { all = false; break; }
            std::memcpy(&value, image + table + slot * 8, 8);
            all = all && value == expected;
        }
        if (!all) continue;
        if (result != missing) return missing;
        result = table;
    }
    return result;
}

// Loader barrier, after relocation (vtables then hold absolute pointers) and before any binding
// is used. Logs every binding that is not found, and for the reference-free cross-check points
// whether they agree with the offsets recorded for this eboot (retail_profiles.hpp).
inline void prepare(const unsigned char* image, std::size_t extent, const std::string& sha,
                    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& executable_spans) {
    state = State{image, extent, sha, sha == reference_sha256, std::vector<std::size_t>(std::size(points), missing)};
    if (state.reference) {
        for (std::size_t i = 0; i < std::size(points); ++i) state.found[i] = points[i].reference;
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    std::size_t resolved = 0;
    for (int pass = 0; pass < 2; ++pass)  // vtables need the functions their slots hold
        for (std::size_t i = 0; i < std::size(points); ++i) {
            if ((points[i].kind == 2) != (pass == 1)) continue;
            state.found[i] = points[i].kind == 2 ? resolve_vtable(image, extent, points[i]) : resolve(image, extent, executable_spans, points[i]);
            if (state.found[i] != missing) ++resolved;
            else std::fprintf(stderr, "retail_map: %s (EU 1.21 %#x) not found in this eboot; the features using it are off\n",
                              points[i].name, static_cast<unsigned>(points[i].reference));
        }
    std::fprintf(stderr, "retail_map: %zu of %zu EU 1.21 bindings found in eboot %.16s (%lld ms)\n", resolved, std::size(points), sha.c_str(),
                 static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));
    // Offsets recorded for this eboot by other means (retail_profiles.hpp) check the search.
    if (const auto* profile = retail_profile(sha)) {
        const std::pair<const char*, std::uint64_t> recorded[] = {
            {"check.startup_worker", profile->worker}, {"check.fps60_store", profile->fps60}, {"check.scale4k_load", profile->scale4k}};
        for (const auto& [name, offset] : recorded) {
            if (!offset) continue;
            for (std::size_t i = 0; i < std::size(points); ++i)
                if (!std::strcmp(points[i].name, name))
                    std::fprintf(stderr, "retail_map: check %s found %#zx, recorded %#llx: %s\n", name, state.found[i],
                                 static_cast<unsigned long long>(offset), state.found[i] == offset ? "agree" : "DIFFER");
        }
    }
    std::fflush(stderr);
}

// Before prepare() (control programs, the reference eboot) offsets map to themselves.
inline bool identity() { return !state.image || state.reference; }
inline bool bound(const std::string& sha) { return state.image ? sha == state.sha : sha == reference_sha256; }

inline std::size_t find(std::size_t reference) {
    if (identity()) return reference;
    for (std::size_t i = 0; i < std::size(points); ++i)
        if (points[i].reference == reference) return state.found[i];
    return missing;
}
inline bool has(std::size_t reference) { return find(reference) != missing; }

// Image offset of a binding in the running eboot; throws when it was not found.
inline std::size_t at(std::size_t reference) {
    const auto result = find(reference);
    if (result == missing) {
        const auto* p = point(reference);
        throw std::invalid_argument(std::string(p ? p->name : "binding") + ": not found in this eboot");
    }
    return result;
}

// Compares the bytes at p (the binding's place in the running eboot) with bytes written for the
// reference eboot at reference: positions the window ignores (call targets, displacements) are
// skipped outside the reference eboot. exact=true refuses a range containing such positions, for
// callers that copy the instructions elsewhere.
inline bool same(const unsigned char* p, std::size_t reference, const void* expected, std::size_t size, bool exact = false) {
    const auto* bytes = static_cast<const unsigned char*>(expected);
    if (identity()) return std::memcmp(p, bytes, size) == 0;
    const Window* window = nullptr;
    std::size_t start = 0;
    for (const auto& pt : points) {
        if (pt.kind != 0) continue;
        const auto& w = windows[pt.first];
        const std::size_t begin = pt.reference - w.site;
        if (begin <= reference && reference + size <= begin + w.size) { window = &w; start = reference - begin; break; }
    }
    if (!window) return std::memcmp(p, bytes, size) == 0;
    for (std::size_t i = 0; i < size; ++i) {
        if (window->mask[start + i] != 'x') {
            if (exact) return false;
            continue;
        }
        if (p[i] != bytes[i]) return false;
    }
    return true;
}
} // namespace ctrport::retail_map
