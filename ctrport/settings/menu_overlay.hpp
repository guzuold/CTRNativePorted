// SPDX-License-Identifier: GPL-2.0-only
// The native PC Settings menu as virtual archives (C++17). On the first /app0 open of one
// of the three archives involved, menu_assets.hpp builds the menu resources in memory from
// the archives the game would read (mods, patch, base, Xbox icons included). Then:
// - archives/juicedomain_permanent.pak is served as the rebuilt archive (about 7 MB);
// - archives/update.pak and archives/update2.pak are served as the original file with the
//   replaced entries appended after its end and only their 16-byte descriptors changed,
//   the same bytes tools/settings/assets/update_overlay.py writes into a full copy.
// The game and mod files are only read. CTR_PC_MENU=0 turns it off. If a template is not
// the pinned retail 1.21 one (another version, a mod), the menu stays off and the game
// reads the files unchanged.
// The same views carry the asset replacements of enabled octr-mod mods (octr_mods.hpp), set
// once at startup: on update.pak and update2.pak the menu entries are appended after them.
#pragma once
#include "menu_assets.hpp"
#include <chrono>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <mutex>

namespace ctrport::settings::menu_overlay {
namespace fs = std::filesystem;
using menu_assets::Bytes;

// A piece of the bytes served after the file: in memory, or `size` bytes of a mod file.
struct Segment {
    Bytes bytes;
    fs::path file;
    std::uint64_t size = 0;
};
struct View {
    fs::path host;                 // the file this view belongs to
    std::uint64_t source_size = 0; // bytes served from the file (0: whole replacement)
    std::vector<std::pair<std::uint64_t, std::array<std::uint8_t, 16>>> patches; // later ones win
    std::vector<Segment> tail;                                                   // served after source_size, in order
    std::uint64_t tail_size = 0;
    std::uint64_t size() const { return source_size + tail_size; }
    void append(Bytes bytes) {
        const auto n = bytes.size();
        tail.push_back({std::move(bytes), {}, n});
        tail_size += n;
    }
    void append_file(const fs::path& file, std::uint64_t n) {
        tail.push_back({{}, file, n});
        tail_size += n;
    }
};

constexpr const char* kArchives[] = {"archives/juicedomain_permanent.pak", "archives/update.pak",
                                     "archives/update2.pak"};

inline std::string lower(std::string s) {
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return s;
}

// One entry replacement appended after the end of an IGA archive (update_overlay.py): the
// payload goes after the sector-aligned end, stored uncompressed, and only the entry's
// 16-byte descriptor changes. With `file` the payload is served from that file instead.
inline void append_entry(View& view, const menu_assets::Iga& archive, const menu_assets::IgaEntry& entry, Bytes blob,
                         const fs::path& file = {}, std::uint64_t file_size = 0) {
    const std::uint64_t end = view.size();
    if (const auto pad = (archive.sector_size - end % archive.sector_size) % archive.sector_size)
        view.append(Bytes(size_t(pad), 0));
    const std::uint64_t offset = view.size(), size = file.empty() ? blob.size() : file_size;
    if (offset >= (1ull << 40)) menu_assets::fail("IGA offset exceeds 40-bit field");
    if (size > 0xffffffffull) menu_assets::fail("IGA entry larger than 4 GB");
    if (file.empty())
        view.append(std::move(blob));
    else
        view.append_file(file, size);
    Bytes descriptor(16);
    menu_assets::put32(descriptor, 0, offset & 0xffffffffu);
    menu_assets::put32(descriptor, 4, (std::uint32_t(entry.ordinal) & 0xffffff00u) | std::uint32_t(offset >> 32));
    menu_assets::put32(descriptor, 8, std::uint32_t(size));
    menu_assets::put32(descriptor, 12, 0xffffffffu);
    std::array<std::uint8_t, 16> bytes{};
    std::copy(descriptor.begin(), descriptor.end(), bytes.begin());
    view.patches.emplace_back(56 + std::uint64_t(archive.file_count) * 4 + std::uint64_t(entry.index) * 16, bytes);
}
// Entry replacements by path; `base` (the octr-mod view of the same file) is continued.
inline std::shared_ptr<View> append_entries(const fs::path& host,
                                            const std::vector<std::pair<std::string, Bytes>>& replacements,
                                            const std::shared_ptr<const View>& base = nullptr) {
    menu_assets::Iga archive(host, false);
    auto view = base ? std::make_shared<View>(*base) : std::make_shared<View>();
    view->host = host;
    view->source_size = archive.size();
    for (const auto& [path, blob] : replacements) append_entry(*view, archive, archive.entry(path), blob);
    return view;
}

class Overlay {
public:
    using Resolve = std::function<fs::path(const std::string&)>;
    // The octr-mod views (octr_mods.hpp), keyed by lower-case /app0 path; set before any open.
    void set_mod_views(std::map<std::string, std::shared_ptr<const View>> views) { mod_views_ = std::move(views); }
    // Off for an eboot without the menu bridge (entry_boundary.hpp); before any open.
    void disable_menu() { menu_disabled_ = true; }
    // The view for an /app0 path, or null when the file is served unchanged.
    std::shared_ptr<const View> find(const std::string& relative, const fs::path& host, const Resolve& resolve) {
        const std::string key = lower(relative);
        bool involved = false;
        for (const char* a : kArchives) involved |= key == a;
        std::shared_ptr<const View> view;
        if (involved) {
            std::call_once(once_, [&] { build(resolve); });
            if (const auto it = views_.find(key); it != views_.end()) view = it->second;
        }
        if (!view)
            if (const auto it = mod_views_.find(key); it != mod_views_.end()) view = it->second;
        return view && view->host == host ? view : nullptr;
    }

private:
    void build(const Resolve& resolve) {
        if (menu_disabled_) {
            std::fprintf(stderr, "ctrport: PC Settings menu off: not available for this eboot\n");
            std::fflush(stderr);
            return;
        }
        char value[4]{};
        if (GetEnvironmentVariableA("CTR_PC_MENU", value, sizeof(value)) == 1 && value[0] == '0') {
            std::fprintf(stderr, "ctrport: PC Settings menu off (CTR_PC_MENU=0)\n");
            std::fflush(stderr);
            return;
        }
        try {
            const auto start = std::chrono::steady_clock::now();
            std::array<fs::path, 3> hosts;
            for (size_t i = 0; i < hosts.size(); ++i) hosts[i] = resolve(kArchives[i]);
            if (mod_views_.count(kArchives[0]))
                menu_assets::fail("a mod changes assets inside juicedomain_permanent.pak");
            const auto generated = menu_assets::generate(hosts[0], hosts[1], hosts[2]);
            auto whole = std::make_shared<View>();
            whole->host = hosts[0];
            whole->append(generated.juicedomain);
            const auto mod_view = [&](const char* key) -> std::shared_ptr<const View> {
                const auto it = mod_views_.find(key);
                return it == mod_views_.end() ? nullptr : it->second;
            };
            std::map<std::string, std::shared_ptr<const View>> views;
            views[kArchives[0]] = whole;
            views[kArchives[1]] = append_entries(hosts[1], generated.update, mod_view(kArchives[1]));
            views[kArchives[2]] = append_entries(hosts[2], generated.update2, mod_view(kArchives[2]));
            views_ = std::move(views);
            const auto ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            std::fprintf(stderr, "ctrport: PC Settings menu ready: %zu pages, %zu resources, %lld ms\n",
                         generated.projects.size(), generated.files.size(), static_cast<long long>(ms));
        } catch (const std::exception& e) {
            views_.clear();
            std::fprintf(stderr, "ctrport: PC Settings menu unavailable, game files served unchanged: %s\n", e.what());
        }
        std::fflush(stderr);
    }
    std::once_flag once_;
    std::map<std::string, std::shared_ptr<const View>> views_, mod_views_;
    bool menu_disabled_ = false;
};
inline Overlay overlay;

// Reads [offset, offset+count) of the virtual file; read_source reads the original file.
template <class ReadSource>
inline std::uint64_t read(const View& view, std::uint64_t offset, std::uint8_t* target, std::uint64_t count,
                          ReadSource&& read_source) {
    std::uint64_t done = 0;
    if (offset < view.source_size) {
        const std::uint64_t wanted = (std::min<std::uint64_t>)(count, view.source_size - offset);
        done = read_source(offset, target, wanted);
        for (const auto& [at, bytes] : view.patches) {
            const std::uint64_t lo = (std::max<std::uint64_t>)(at, offset),
                                hi = (std::min<std::uint64_t>)(at + 16, offset + done);
            if (lo < hi) std::memcpy(target + (lo - offset), bytes.data() + (lo - at), size_t(hi - lo));
        }
        if (done < wanted) return done;
    }
    std::uint64_t segment_at = view.source_size;
    for (const auto& segment : view.tail) {
        if (done >= count) break;
        const std::uint64_t at = offset + done;
        if (at >= segment_at && at < segment_at + segment.size) {
            const std::uint64_t from = at - segment_at,
                                n = (std::min<std::uint64_t>)(count - done, segment.size - from);
            if (segment.file.empty()) {
                std::memcpy(target + done, segment.bytes.data() + from, size_t(n));
            } else {
                std::ifstream in(segment.file, std::ios::binary);
                in.seekg(std::streamoff(from));
                if (!in.read(reinterpret_cast<char*>(target + done), std::streamsize(n)))
                    throw std::runtime_error("mod file unreadable: " + segment.file.u8string());
            }
            done += n;
        }
        segment_at += segment.size;
    }
    return done;
}
} // namespace ctrport::settings::menu_overlay
