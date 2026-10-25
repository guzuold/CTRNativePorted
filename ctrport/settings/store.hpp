// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Shared launcher/runtime settings. The only authoritative document remains
// runtime/user/launcher_config.json. Unknown fields survive field-level updates.
#include "../online_server/json.hpp"
#include "control_schema.hpp"
#include "presets.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

namespace ctrport::settings {
using ctr_online::Json;
namespace fs = std::filesystem;
constexpr unsigned schema_version = 1;
constexpr std::size_t max_document_bytes = 256 * 1024;

struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE v) : value(v) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

[[noreturn]] inline void win_error(const char* operation) {
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
}

inline bool one_of(const std::string& value, std::initializer_list<const char*> domain) {
    return std::any_of(domain.begin(), domain.end(), [&](const char* p) { return value == p; });
}

inline void validate(const Json& doc, bool complete=true) {
    if (!doc.is_object()) throw std::invalid_argument("settings: expected JSON object");
    control_schema::validate(doc);
    if (const auto* v = doc.find("settings_schema")) {
        if (!v->is_integer() || v->as_u64() != schema_version)
            throw std::invalid_argument("settings: unsupported schema; document preserved");
    }
    for (const char* key : {"base_dir", "patch_dir", "resolution", "custom_w", "custom_h",
                            "scene_height", "upscaler", "antialiasing", "game_language", "online_id",
                            "online_server", "online_friend_code", "public_server", "online_public_ip",
                            "trophy_key", "player1_pad", "player2_pad", "player3_pad", "player4_pad"}) {
        const auto* v = doc.find(key);
        if (v && !v->is_string()) throw std::invalid_argument(std::string("settings: expected string: ") + key);
    }
    for (const char* key : {"use_patch", "fps_60", "ps4_pro", "native_4k", "unlock_all", "fullscreen",
                            "online", "online_host", "wumpa_x100", "xbox_prompts", "split_screen", "mods_enabled"}) {
        const auto* v = doc.find(key);
        if (v && !v->is_bool()) throw std::invalid_argument(std::string("settings: expected boolean: ") + key);
    }
    if (const auto* v = doc.find("scene_height"))
        if (!one_of(v->text, {"720", "900", "1080", "1440", "1800", "2160"}))
            throw std::invalid_argument("settings: invalid internal resolution");
    if (const auto* v = doc.find("upscaler"))
        if (!one_of(v->text, {"off", "fsr1", "fsr3", "dlss"}))
            throw std::invalid_argument("settings: invalid upscaler");
    if (const auto* refresh=doc.find("display_refresh")) {
        const auto* mode=doc.find("window_mode");
        if(refresh->text!="original" && ((mode && mode->text!="exclusive") || (!mode && complete)))
            throw std::invalid_argument("settings: display refresh requires exclusive window mode");
    }
    if (const auto* hdr = doc.find("hdr_output")) {
        const auto* up = doc.find("upscaler");
        if(hdr->text=="native" && up && up->text!="off")
            throw std::invalid_argument("settings: native HDR requires scene upscaler off");
    }
    if (const auto* v = doc.find("game_language"))
        if (!one_of(v->text, {"en", "fr", "it", "de", "es", "ar"}))
            throw std::invalid_argument("settings: invalid game language");
    if (const auto* v = doc.find("antialiasing"))
        if (!one_of(v->text, {"original", "off"}))
            throw std::invalid_argument("settings: invalid antialiasing override");
}

inline Json read_document(const fs::path& path, bool missing_ok = true) {
    Handle input(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (input.value == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND && missing_ok) return Json::object();
        win_error("settings read");
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(input.value, &size)) win_error("settings size");
    if (size.QuadPart <= 0 || size.QuadPart > max_document_bytes)
        throw std::invalid_argument("settings: empty or oversized document; preserved");
    std::string bytes(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD got = 0;
    if (!ReadFile(input.value, bytes.data(), static_cast<DWORD>(bytes.size()), &got, nullptr)) win_error("settings read bytes");
    if (got != bytes.size()) throw std::runtime_error("settings: short read");
    if (bytes.compare(0, 3, "\xef\xbb\xbf") == 0) bytes.erase(0, 3);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0))
        throw std::invalid_argument("settings: invalid UTF-8");
    // Bound nesting before calling the shared recursive JSON parser. Quoted
    // braces and escaped quotes must not count as containers.
    int depth = 0;
    bool quoted = false, escaped = false;
    for (char c : bytes) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') {
            if (++depth > 64) throw std::invalid_argument("settings: excessive JSON nesting");
        } else if (c == '}' || c == ']') --depth;
    }
    auto doc = ctr_online::parse_json(bytes);
    validate(doc);
    return doc;
}

// One immutable startup snapshot: menu edits are requested for the next launch.
inline const Json* runtime_snapshot() {
    static const std::optional<Json> snapshot = []() -> std::optional<Json> {
        wchar_t configured[32768]{};
        SetLastError(ERROR_SUCCESS);
        const auto count = GetEnvironmentVariableW(L"CTR_SETTINGS_PATH", configured, 32768);
        if (!count && GetLastError() == ERROR_ENVVAR_NOT_FOUND) return std::nullopt;
        if (!count || count >= 32768) throw std::invalid_argument("settings: invalid CTR_SETTINGS_PATH");
        return read_document(configured, false);
    }();
    return snapshot ? &*snapshot : nullptr;
}

// Legacy files have no schema and derive scene height from these two flags.
// Read migration is side-effect free; the next successful write records schema 1.
inline int scene_height(const Json& doc) {
    validate(doc);
    if (const auto* value = doc.find("scene_height")) return std::stoi(value->text);
    const auto flag = [&](const char* key, bool fallback) {
        const auto* v = doc.find(key); return v ? v->boolean : fallback;
    };
    return flag("native_4k", false) ? 2160 : (flag("ps4_pro", true) ? 1440 : 1080);
}

struct RestartValue {
    int requested = 1440;
    int applied = 1440;
    bool restart_required() const { return requested != applied; }
};

inline Json changed_fields(const Json& before, const Json& after) {
    if (!before.is_object() || !after.is_object()) throw std::invalid_argument("settings: snapshots must be objects");
    auto changes = Json::object();
    for (const auto& item : after.members) {
        const auto* previous = before.find(item.first);
        if (!previous || *previous != item.second) changes.set(item.first, item.second);
    }
    return changes;
}

class Store {
    fs::path path_;

    Handle lock_writer() const {
        fs::create_directories(path_.parent_path());
        Handle lock(CreateFileW((path_.wstring() + L".lock").c_str(), GENERIC_READ | GENERIC_WRITE,
                                0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (lock.value == INVALID_HANDLE_VALUE) win_error("settings writer busy");
        return Handle(std::exchange(lock.value, INVALID_HANDLE_VALUE));
    }

    void write_document(const Json& doc, bool keep_backup) const {
        validate(doc);
        const std::string bytes = ctr_online::dump_json(doc) + "\n";
        if (bytes.size() > max_document_bytes) throw std::invalid_argument("settings: oversized output");
        static std::atomic<unsigned> sequence{0};
        const fs::path temporary = path_.wstring() + L".tmp-" + std::to_wstring(GetCurrentProcessId()) +
            L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1));
        try {
            {
                Handle output(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
                if (output.value == INVALID_HANDLE_VALUE) win_error("settings temporary create");
                DWORD written = 0;
                if (!WriteFile(output.value, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr))
                    win_error("settings write");
                if (written != bytes.size()) throw std::runtime_error("settings: short write");
                if (!FlushFileBuffers(output.value)) win_error("settings flush");
            }
            const fs::path backup = path_.wstring() + L".bak";
            // A just-written file can be briefly held by other readers (for example an
            // antivirus scan): retry transient sharing failures, never other errors.
            BOOL replaced = FALSE;
            for (int attempt = 0; attempt < 8; ++attempt) {
                replaced = ReplaceFileW(path_.c_str(), temporary.c_str(), keep_backup ? backup.c_str() : nullptr,
                                        0, nullptr, nullptr);
                const DWORD error = replaced ? ERROR_SUCCESS : GetLastError();
                if (replaced || (error != ERROR_UNABLE_TO_REMOVE_REPLACED && error != ERROR_SHARING_VIOLATION &&
                                 error != ERROR_ACCESS_DENIED)) break;
                Sleep(25u << attempt);
                SetLastError(error);
            }
            if (!replaced) {
                const DWORD error = GetLastError();
                // Only a genuinely missing destination permits a new file. No
                // delete/rename fallback on sharing, permissions or disk errors.
                if (error != ERROR_FILE_NOT_FOUND || fs::exists(path_)) {
                    SetLastError(error); win_error("settings atomic replace");
                }
                if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_WRITE_THROUGH))
                    win_error("settings atomic create");
            }
        } catch (...) {
            DeleteFileW(temporary.c_str());
            throw;
        }
    }

public:
    explicit Store(fs::path path) : path_(std::move(path)) {
        if (path_.filename().empty() || path_.parent_path().empty())
            throw std::invalid_argument("settings: path must include a directory and filename");
    }
    const fs::path& path() const { return path_; }
    Json read() const { return read_document(path_); }

    // Re-read under the cross-process lock. A menu edit cannot overwrite an
    // unrelated setting saved by the launcher since its screen was opened.
    Json update(const Json& changes, std::initializer_list<const char*> remove = {}) const {
        if (!changes.is_object()) throw std::invalid_argument("settings: changes must be an object");
        validate(changes,false);
        const auto lock = lock_writer();
        auto doc = read();
        const auto expanded=presets::expand(changes);
        for (const auto& item : expanded.members) doc.set(item.first, item.second);
        for (const char* key : remove)
            doc.members.erase(std::remove_if(doc.members.begin(), doc.members.end(),
                              [&](const auto& item) { return item.first == key; }), doc.members.end());
        doc.set("settings_schema", Json::of(schema_version));
        write_document(doc, true);
        return doc;
    }

    // Explicit recovery only: never silently launch with a backup or discard a
    // corrupt document. Preserve both the rejected bytes and the last good file.
    void restore_backup() const {
        const auto lock = lock_writer();
        auto doc = read_document(path_.wstring() + L".bak", false);
        const auto rejected = path_.wstring() + L".rejected-" + std::to_wstring(GetTickCount64());
        if (fs::exists(path_) && !CopyFileW(path_.c_str(), rejected.c_str(), TRUE)) win_error("settings preserve rejected file");
        doc.set("settings_schema", Json::of(schema_version));
        write_document(doc, false);
    }
};
} // namespace ctrport::settings
