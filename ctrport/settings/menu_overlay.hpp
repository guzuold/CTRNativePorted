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

struct View {
    fs::path host;                 // the file this view belongs to
    std::uint64_t source_size = 0; // bytes served from the file (0: whole replacement)
    std::vector<std::pair<std::uint64_t, std::array<std::uint8_t, 16>>> patches;
    Bytes tail;                    // served after source_size
    std::uint64_t size() const { return source_size + tail.size(); }
};

constexpr const char* kArchives[] = {"archives/juicedomain_permanent.pak", "archives/update.pak", "archives/update2.pak"};

inline std::string lower(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c = char(c + 32);
    return s;
}

// Entry replacements appended after the end of an IGA archive (update_overlay.py).
inline std::shared_ptr<View> append_entries(const fs::path& host, const std::vector<std::pair<std::string, Bytes>>& replacements) {
    menu_assets::Iga archive(host, false);
    auto view = std::make_shared<View>();
    view->host = host; view->source_size = archive.size();
    for (const auto& [path, blob] : replacements) {
        const auto& entry = archive.entry(path);
        const std::uint64_t end = view->size();
        view->tail.resize(size_t(view->tail.size() + (archive.sector_size - end % archive.sector_size) % archive.sector_size), 0);
        const std::uint64_t offset = view->size();
        if (offset >= (1ull << 40)) menu_assets::fail("IGA offset exceeds 40-bit field");
        view->tail.insert(view->tail.end(), blob.begin(), blob.end());
        Bytes descriptor(16);
        menu_assets::put32(descriptor, 0, offset & 0xffffffffu);
        menu_assets::put32(descriptor, 4, (std::uint32_t(entry.ordinal) & 0xffffff00u) | std::uint32_t(offset >> 32));
        menu_assets::put32(descriptor, 8, blob.size());
        menu_assets::put32(descriptor, 12, 0xffffffffu);
        std::array<std::uint8_t, 16> bytes{}; std::copy(descriptor.begin(), descriptor.end(), bytes.begin());
        view->patches.emplace_back(56 + std::uint64_t(archive.file_count) * 4 + std::uint64_t(entry.index) * 16, bytes);
    }
    return view;
}

class Overlay {
public:
    using Resolve = std::function<fs::path(const std::string&)>;
    // The view for an /app0 path, or null when the file is served unchanged.
    std::shared_ptr<const View> find(const std::string& relative, const fs::path& host, const Resolve& resolve) {
        const std::string key = lower(relative);
        bool involved = false;
        for (const char* a : kArchives) involved |= key == a;
        if (!involved) return nullptr;
        std::call_once(once_, [&] { build(resolve); });
        const auto it = views_.find(key);
        if (it == views_.end() || it->second->host != host) return nullptr;
        return it->second;
    }

private:
    void build(const Resolve& resolve) {
        char value[4]{};
        if (GetEnvironmentVariableA("CTR_PC_MENU", value, sizeof(value)) == 1 && value[0] == '0') {
            std::fprintf(stderr, "ctrport: PC Settings menu off (CTR_PC_MENU=0)\n"); std::fflush(stderr); return;
        }
        try {
            const auto start = std::chrono::steady_clock::now();
            std::array<fs::path, 3> hosts;
            for (size_t i = 0; i < hosts.size(); ++i) hosts[i] = resolve(kArchives[i]);
            const auto generated = menu_assets::generate(hosts[0], hosts[1], hosts[2]);
            auto whole = std::make_shared<View>();
            whole->host = hosts[0]; whole->tail = generated.juicedomain;
            std::map<std::string, std::shared_ptr<const View>> views;
            views[kArchives[0]] = whole;
            views[kArchives[1]] = append_entries(hosts[1], generated.update);
            views[kArchives[2]] = append_entries(hosts[2], generated.update2);
            views_ = std::move(views);
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            std::fprintf(stderr, "ctrport: PC Settings menu ready: %zu pages, %zu resources, %lld ms\n",
                         generated.projects.size(), generated.files.size(), static_cast<long long>(ms));
        } catch (const std::exception& e) {
            views_.clear();
            std::fprintf(stderr, "ctrport: PC Settings menu unavailable, game files served unchanged: %s\n", e.what());
        }
        std::fflush(stderr);
    }
    std::once_flag once_;
    std::map<std::string, std::shared_ptr<const View>> views_;
};
inline Overlay overlay;

// Reads [offset, offset+count) of the virtual file; read_source reads the original file.
template <class ReadSource>
inline std::uint64_t read(const View& view, std::uint64_t offset, std::uint8_t* target, std::uint64_t count, ReadSource&& read_source) {
    std::uint64_t done = 0;
    if (offset < view.source_size) {
        const std::uint64_t wanted = (std::min<std::uint64_t>)(count, view.source_size - offset);
        done = read_source(offset, target, wanted);
        for (const auto& [at, bytes] : view.patches) {
            const std::uint64_t lo = (std::max<std::uint64_t>)(at, offset), hi = (std::min<std::uint64_t>)(at + 16, offset + done);
            if (lo < hi) std::memcpy(target + (lo - offset), bytes.data() + (lo - at), size_t(hi - lo));
        }
        if (done < wanted) return done;
    }
    const std::uint64_t at = offset + done;
    if (done < count && at >= view.source_size && at - view.source_size < view.tail.size()) {
        const std::uint64_t from = at - view.source_size, n = (std::min<std::uint64_t>)(count - done, view.tail.size() - from);
        std::memcpy(target + done, view.tail.data() + from, size_t(n));
        done += n;
    }
    return done;
}
}  // namespace ctrport::settings::menu_overlay
