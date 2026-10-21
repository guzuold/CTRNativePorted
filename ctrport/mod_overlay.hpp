// SPDX-License-Identifier: GPL-2.0-only
// Loose-file mods, in the layout and mods.json format of SacredFire / GetLostIn
// CTR-NitroFueled-PC (scripts/mods.py): one folder per mod under runtime/mods, holding the
// game's own folders (archives/, icons/, ...), optionally inside app0/, CUSA14876/ or
// CUSA13795/, or inside one extra wrapper folder. mods.json is
// {"order": ["first", "second"], "disabled": ["first"]}: listed names keep their order, new
// folders are appended alphabetically, and a later enabled mod wins over an earlier one.
// The runtime never writes the game or mod files: /app0 reads look up the mod index first,
// then the patch folder, then the base game (app0.hpp). Shared by the launcher (C++20) and
// ctrport.exe (C++17).
#pragma once
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace ctrport::mods {
namespace fs = std::filesystem;

inline std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}
inline std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) throw std::runtime_error("invalid UTF-8 text");
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(), size);
    return out;
}
inline std::wstring fold(std::wstring text) {
    for (auto& c : text) c = static_cast<wchar_t>(std::towlower(c));
    return text;
}

// --- mods.json / patches.json: an object whose members are arrays of strings ---------------
using StringLists = std::map<std::string, std::vector<std::wstring>>;
inline StringLists parse_string_lists(const std::string& text, const char* what) {
    std::size_t at = 0;
    const auto fail = [&](const char* why) { throw std::runtime_error(std::string(what) + ": " + why); };
    const auto skip = [&] { while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at; };
    const auto expect = [&](char c) { skip(); if (at >= text.size() || text[at] != c) fail("invalid JSON"); ++at; };
    const auto string = [&]() -> std::string {
        expect('"');
        std::string out;
        while (true) {
            if (at >= text.size()) fail("unterminated string");
            const char c = text[at++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (at >= text.size()) fail("invalid escape");
            const char e = text[at++];
            if (e == 'u') {
                if (at + 4 > text.size()) fail("invalid escape");
                unsigned code = std::stoul(text.substr(at, 4), nullptr, 16); at += 4;
                if (code >= 0xd800 && code < 0xdc00 && at + 6 <= text.size() && text[at] == '\\' && text[at + 1] == 'u') {
                    const unsigned low = std::stoul(text.substr(at + 2, 4), nullptr, 16);
                    if (low >= 0xdc00 && low < 0xe000) { code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00); at += 6; }
                }
                if (code < 0x80) out += static_cast<char>(code);
                else if (code < 0x800) { out += static_cast<char>(0xc0 | (code >> 6)); out += static_cast<char>(0x80 | (code & 0x3f)); }
                else if (code < 0x10000) { out += static_cast<char>(0xe0 | (code >> 12)); out += static_cast<char>(0x80 | ((code >> 6) & 0x3f)); out += static_cast<char>(0x80 | (code & 0x3f)); }
                else { out += static_cast<char>(0xf0 | (code >> 18)); out += static_cast<char>(0x80 | ((code >> 12) & 0x3f)); out += static_cast<char>(0x80 | ((code >> 6) & 0x3f)); out += static_cast<char>(0x80 | (code & 0x3f)); }
            } else {
                const char* from = "\"\\/bfnrt"; const char* to = "\"\\/\b\f\n\r\t";
                const char* found = std::strchr(from, e);
                if (!found || !e) fail("invalid escape");
                out += to[found - from];
            }
        }
    };
    StringLists lists;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xef) at = 3;  // UTF-8 BOM
    expect('{');
    skip();
    if (at < text.size() && text[at] == '}') { ++at; return lists; }
    while (true) {
        const auto key = string();
        expect(':');
        expect('[');
        auto& list = lists[key];
        skip();
        if (at < text.size() && text[at] == ']') ++at;
        else while (true) {
            skip();
            if (at >= text.size() || text[at] != '"') fail("lists must contain only strings");
            list.push_back(wide(string()));
            skip();
            if (at < text.size() && text[at] == ',') { ++at; continue; }
            expect(']');
            break;
        }
        skip();
        if (at < text.size() && text[at] == ',') { ++at; continue; }
        expect('}');
        break;
    }
    skip();
    if (at != text.size()) fail("trailing data");
    return lists;
}
inline std::string json_quote(const std::wstring& value) {
    std::string out = "\"";
    for (const unsigned char c : utf8(value)) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 0x20) { char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
        else out += static_cast<char>(c);
    }
    return out + "\"";
}
inline std::string format_string_lists(const std::vector<std::pair<std::string, std::vector<std::wstring>>>& lists) {
    std::string out = "{";
    for (std::size_t i = 0; i < lists.size(); ++i) {
        out += (i ? ",\n  " : "\n  ") + json_quote(wide(lists[i].first)) + ": [";
        for (std::size_t j = 0; j < lists[i].second.size(); ++j) out += (j ? ", " : "") + json_quote(lists[i].second[j]);
        out += "]";
    }
    return out + "\n}\n";
}
inline std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + utf8(path.wstring()));
    std::stringstream buffer; buffer << in.rdbuf();
    return buffer.str();
}
// Writes through a temporary file so a crash never leaves half a configuration.
inline void write_text(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    const fs::path temporary = path.wstring() + L".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << text;
        if (!out) throw std::runtime_error("cannot write " + utf8(temporary.wstring()));
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot replace " + utf8(path.wstring()));
}

struct Config { std::vector<std::wstring> order, disabled; };
inline Config read_config(const fs::path& path) {
    Config config;
    std::error_code ec;
    if (!fs::exists(path, ec)) return config;
    const auto lists = parse_string_lists(read_text(path), "mods.json");
    if (const auto it = lists.find("order"); it != lists.end()) config.order = it->second;
    if (const auto it = lists.find("disabled"); it != lists.end()) config.disabled = it->second;
    return config;
}
inline void write_config(const fs::path& path, const Config& config) {
    write_text(path, format_string_lists({{"order", config.order}, {"disabled", config.disabled}}));
}

struct Mod { std::wstring name; fs::path folder; bool enabled = true; };
// Folders under mods_dir in load order (the last enabled one wins).
inline std::vector<Mod> list_mods(const fs::path& mods_dir, const Config& config) {
    std::vector<Mod> found;
    std::error_code ec;
    if (!fs::is_directory(mods_dir, ec)) return found;
    for (const auto& entry : fs::directory_iterator(mods_dir)) {
        const auto name = entry.path().filename().wstring();
        if (name.empty() || name[0] == L'.' || !entry.is_directory()) continue;
        found.push_back({name, entry.path(), true});
    }
    std::sort(found.begin(), found.end(), [](const Mod& a, const Mod& b) { return fold(a.name) < fold(b.name); });
    std::vector<Mod> ordered;
    for (const auto& name : config.order) {
        const auto it = std::find_if(found.begin(), found.end(), [&](const Mod& mod) { return mod.name == name; });
        if (it != found.end()) { ordered.push_back(*it); found.erase(it); }
    }
    ordered.insert(ordered.end(), found.begin(), found.end());
    for (auto& mod : ordered)
        mod.enabled = std::find(config.disabled.begin(), config.disabled.end(), mod.name) == config.disabled.end();
    return ordered;
}
inline Config config_of(const std::vector<Mod>& mods) {
    Config config;
    for (const auto& mod : mods) {
        config.order.push_back(mod.name);
        if (!mod.enabled) config.disabled.push_back(mod.name);
    }
    return config;
}

inline bool is_reparse(const fs::path& path) {
    const DWORD flags = GetFileAttributesW(path.c_str());
    return flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_REPARSE_POINT);
}
// Top-level folders of the CTR Nitro-Fueled application folder (base and update).
inline bool game_folder(const std::wstring& name) {
    const auto f = fold(name);
    return f == L"archives" || f == L"dlog" || f == L"ghosts" || f == L"icons";
}
inline bool layout_folder(const std::wstring& name) {
    const auto f = fold(name);
    return f == L"app0" || f == L"cusa14876" || f == L"cusa13795";
}
inline bool readme_file(const fs::path& path) {
    const auto ext = fold(path.extension().wstring());
    return ext == L".txt" || ext == L".md" || ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".ini" || ext == L".json";
}
// The folder of a mod that corresponds to the game's /app0.
inline fs::path content_root(const fs::path& mod) {
    fs::path root = mod;
    for (int depth = 0; depth < 2; ++depth) {
        std::vector<fs::path> folders;
        bool other_files = false;
        for (const auto& entry : fs::directory_iterator(root)) {
            if (entry.is_directory()) folders.push_back(entry.path());
            else if (!readme_file(entry.path())) other_files = true;
        }
        const auto layout = std::find_if(folders.begin(), folders.end(),
                                         [](const fs::path& p) { return layout_folder(p.filename().wstring()); });
        if (layout != folders.end()) {
            root = *layout;
            // CUSA14876/app0/... is accepted as well.
            for (const auto& entry : fs::directory_iterator(root))
                if (entry.is_directory() && fold(entry.path().filename().wstring()) == L"app0") return entry.path();
            return root;
        }
        // One wrapper folder, e.g. a ZIP extracted into a folder named after it.
        if (depth == 0 && folders.size() == 1 && !other_files && !game_folder(folders[0].filename().wstring())) {
            root = folders[0];
            continue;
        }
        return root;
    }
    return root;
}

struct Index {
    // folded relative path with '/' separators -> mod file
    std::map<std::wstring, fs::path> files;
    std::map<std::wstring, std::wstring> owner;
    std::vector<std::pair<std::wstring, std::size_t>> layers;  // mod name, files
    std::vector<std::string> log;
    std::size_t replaced = 0, added = 0;
};
inline std::wstring key_of(const fs::path& relative) {
    std::wstring key = relative.generic_wstring();
    return fold(key);
}
// Builds the index of enabled mods, later mods overriding earlier ones. game_roots are the
// patch and base folders, used only to report replaced and added files.
inline Index build_index(const std::vector<Mod>& mods, const std::vector<fs::path>& game_roots) {
    Index index;
    for (const auto& mod : mods) {
        if (!mod.enabled) continue;
        if (is_reparse(mod.folder)) throw std::runtime_error("mod " + utf8(mod.name) + ": links are not supported");
        const auto root = content_root(mod.folder);
        std::size_t count = 0;
        for (auto it = fs::recursive_directory_iterator(root); it != fs::recursive_directory_iterator(); ++it) {
            const auto relative = it->path().lexically_relative(root);
            if (is_reparse(it->path()))
                throw std::runtime_error("mod " + utf8(mod.name) + ": links are not supported (" + utf8(relative.generic_wstring()) + ")");
            if (it.depth() == 0) {
                const auto top = fold(relative.wstring());
                if (top == L"eboot.bin" || top == L"sce_module" || top == L"sce_sys")
                    throw std::runtime_error("mod " + utf8(mod.name) + ": replacing " + utf8(relative.wstring()) +
                                             " is not supported; code changes belong in patch XML files");
            }
            if (!it->is_regular_file()) continue;
            if (it.depth() == 0 && readme_file(relative) && !game_folder(relative.wstring())) {
                // Loose readme next to the game folders: documentation, not game data, unless the game has it.
                bool game_file = false;
                for (const auto& game : game_roots) { std::error_code ec; if (!game.empty() && fs::exists(game / relative, ec)) game_file = true; }
                if (!game_file) continue;
            }
            const auto key = key_of(relative);
            if (const auto previous = index.owner.find(key); previous != index.owner.end())
                index.log.push_back("Mods: " + utf8(relative.generic_wstring()) + ": " + utf8(previous->second) + " -> " + utf8(mod.name));
            index.files[key] = it->path();
            index.owner[key] = mod.name;
            ++count;
        }
        index.layers.push_back({mod.name, count});
        index.log.push_back("Mods: " + utf8(mod.name) + ": " + std::to_string(count) + " files");
    }
    for (const auto& item : index.files) {
        bool exists = false;
        for (const auto& game : game_roots) { std::error_code ec; if (!game.empty() && fs::exists(game / item.first, ec)) exists = true; }
        ++(exists ? index.replaced : index.added);
    }
    index.log.push_back("Mods: " + std::to_string(index.replaced) + " game files replaced, " + std::to_string(index.added) + " added");
    return index;
}

// Runtime side: configured once at startup from CTR_MODS_DIR and CTR_MODS_CONFIG.
class Overlay {
    std::mutex mutex_;
    bool active_ = false;
    Index index_;
public:
    bool active() { std::lock_guard<std::mutex> lock(mutex_); return active_; }
    void configure(const fs::path& mods_dir, const fs::path& config, const std::vector<fs::path>& game_roots) {
        auto index = build_index(list_mods(mods_dir, read_config(config)), game_roots);
        std::lock_guard<std::mutex> lock(mutex_);
        index_ = std::move(index);
        active_ = true;
        for (const auto& line : index_.log) std::fprintf(stderr, "%s\n", line.c_str());
        std::fflush(stderr);
    }
    void configure_from_environment(const std::vector<fs::path>& game_roots) {
        wchar_t dir[32768]{}, config[32768]{};
        const DWORD dir_len = GetEnvironmentVariableW(L"CTR_MODS_DIR", dir, 32768);
        if (!dir_len || dir_len >= 32768) return;
        const DWORD config_len = GetEnvironmentVariableW(L"CTR_MODS_CONFIG", config, 32768);
        const fs::path config_path = config_len && config_len < 32768 ? fs::path(config) : fs::path(dir) / L"mods.json";
        configure(dir, config_path, game_roots);
    }
    // Mod file for a relative /app0 path, or an empty path.
    fs::path lookup(const std::string& relative) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || index_.files.empty()) return {};
        const auto it = index_.files.find(fold(wide(relative)));
        return it == index_.files.end() ? fs::path{} : it->second;
    }
    std::size_t files() { std::lock_guard<std::mutex> lock(mutex_); return index_.files.size(); }
};
inline Overlay overlay;
}
