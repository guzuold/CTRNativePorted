// SPDX-License-Identifier: GPL-2.0-only
// GameBanana downloads for the launcher's MODS tab. The catalog is the public apiv11 of
// gamebanana.com for the PC ports of Crash Team Racing Nitro-Fueled (game 26138; the PS4/Switch
// game page, 15892, is not listed): Game/<id>/Subfeed for the submissions,
// <Model>/<id>/DownloadPage for the files. A download is checked against its MD5, extracted with the Windows tar.exe (bsdtar:
// ZIP, 7z and RAR on current Windows) into a staging folder, and installed in the formats of
// mod_overlay.hpp and guest_patch_xml.hpp:
// - every folder holding a game folder (archives/, icons/, ...) becomes runtime/mods/<name>;
//   without one, the folders holding loose .pak files become runtime/mods/<name>/archives;
// - every XML file with a <Patch> root is copied to runtime/patches/gb<id>-<file>.
// Mods in the octr-mod / nitro-mod formats (mod.json, nitro-mod.json) are installed only when
// their manifest asks for nothing beyond XML patches: asset operations inside the game's .pak
// archives (archive_asset), shaders, settings and variants are refused, not half-installed.
// Archives and folders dropped on the launcher are installed the same way.
// What each download installed is kept in runtime/user/gamebanana.json, so that it can be
// removed or replaced. Game and mod files outside those paths are never touched.
#pragma once
#include "guest_patch_xml.hpp"
#include "online_server/json.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <atomic>
#include <functional>
#include <future>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "winhttp.lib")

namespace ctrport::gamebanana {
namespace fs = std::filesystem;
using ctr_online::Json;

constexpr int kConsoleGame = 15892, kPcGame = 26138;

struct File {
    std::uint64_t id = 0;
    std::string name, md5, url;
    std::uint64_t size = 0;
};
struct Entry {
    std::uint64_t id = 0;
    std::string model, name, author, profile;
    std::wstring platform;  // L"PS4", L"Switch" or L"PC"
    std::vector<File> files;
};
struct Installed {
    std::string file;
    std::vector<std::wstring> mods, patches;  // folder names in runtime/mods, file names in runtime/patches
};

inline std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}
inline std::string text_of(const Json& object, const char* key) {
    const Json* value = object.find(key);
    return value && value->is_string() ? value->text : std::string();
}
inline std::uint64_t number_of(const Json& object, const char* key) {
    const Json* value = object.find(key);
    return value && value->is_integer() ? value->as_u64() : 0;
}
inline std::wstring size_text(std::uint64_t bytes) {
    wchar_t out[32];
    if (bytes >= 1024 * 1024) swprintf(out, 32, L"%.1f MB", bytes / 1048576.0);
    else swprintf(out, 32, L"%.0f KB", bytes / 1024.0 + 0.5);
    return out;
}

// --- HTTP ----------------------------------------------------------------------------------------
// GET over HTTPS with redirects (gamebanana.com/dl/<id> redirects to the file server). The body
// goes to `to_file` when given, else it is returned. `progress` receives (received, total).
inline std::string http_get(const std::wstring& url, const fs::path* to_file = nullptr,
                            const std::function<void(std::uint64_t, std::uint64_t)>& progress = {},
                            const std::atomic<bool>* cancel = nullptr) {
    URL_COMPONENTS parts{sizeof(parts)};
    wchar_t host[256]{}, path[2048]{};
    parts.lpszHostName = host; parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path; parts.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        throw std::runtime_error("not an https address: " + mods::utf8(url));
    struct Handle { HINTERNET h = nullptr; ~Handle() { if (h) WinHttpCloseHandle(h); } };
    Handle session{WinHttpOpen(L"CTRNativePorted", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.h) throw std::runtime_error("WinHttpOpen failed");
    WinHttpSetTimeouts(session.h, 15000, 15000, 30000, 60000);
    Handle connect{WinHttpConnect(session.h, host, parts.nPort, 0)};
    Handle request{connect.h ? WinHttpOpenRequest(connect.h, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr};
    if (!request.h || !WinHttpSendRequest(request.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.h, nullptr))
        throw std::runtime_error("cannot reach " + mods::utf8(host) + " (error " + std::to_string(GetLastError()) + ")");
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) throw std::runtime_error(mods::utf8(host) + " answered HTTP " + std::to_string(status));
    std::uint64_t total = 0;
    wchar_t length[32]{};
    DWORD length_size = sizeof(length);
    if (WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, length, &length_size, WINHTTP_NO_HEADER_INDEX))
        total = _wcstoui64(length, nullptr, 10);
    std::string body;
    std::ofstream out;
    if (to_file) {
        out.open(*to_file, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("cannot write " + mods::utf8(to_file->wstring()));
    }
    std::vector<char> buffer(1 << 16);
    std::uint64_t received = 0;
    for (;;) {
        if (cancel && *cancel) throw std::runtime_error("cancelled");
        DWORD read = 0;
        if (!WinHttpReadData(request.h, buffer.data(), static_cast<DWORD>(buffer.size()), &read))
            throw std::runtime_error("download interrupted (error " + std::to_string(GetLastError()) + ")");
        if (!read) break;
        if (to_file) out.write(buffer.data(), read);
        else body.append(buffer.data(), read);
        received += read;
        if (progress) progress(received, total);
    }
    if (to_file && !out.flush()) throw std::runtime_error("cannot write " + mods::utf8(to_file->wstring()));
    return body;
}

inline std::string md5_hex(const fs::path& file) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_MD5_ALGORITHM, nullptr, 0) < 0) throw std::runtime_error("MD5 unavailable");
    BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
    std::ifstream in(file, std::ios::binary);
    std::vector<char> buffer(1 << 16);
    while (in.read(buffer.data(), buffer.size()) || in.gcount())
        BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), static_cast<ULONG>(in.gcount()), 0);
    unsigned char digest[16];
    BCryptFinishHash(hash, digest, 16, 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    std::string hex;
    for (unsigned char byte : digest) { char two[3]; std::snprintf(two, 3, "%02x", byte); hex += two; }
    return hex;
}

// --- Catalog ---------------------------------------------------------------------------------------
// One Subfeed page: the submissions with files that can be installed (mods and sounds).
inline std::vector<Entry> parse_subfeed(const Json& page, int game, bool* complete = nullptr) {
    std::vector<Entry> entries;
    if (complete) {
        const Json* meta = page.find("_aMetadata");
        const Json* done = meta ? meta->find("_bIsComplete") : nullptr;
        *complete = !done || !done->is_bool() || done->boolean;
    }
    const Json* records = page.find("_aRecords");
    if (!records || !records->is_array()) return entries;
    for (const Json& record : records->items) {
        const auto model = text_of(record, "_sModelName");
        const Json* has_files = record.find("_bHasFiles");
        if ((model != "Mod" && model != "Sound") || !has_files || !has_files->is_bool() || !has_files->boolean) continue;
        Entry entry;
        entry.id = number_of(record, "_idRow");
        entry.model = model;
        entry.name = text_of(record, "_sName");
        entry.profile = text_of(record, "_sProfileUrl");
        if (const Json* submitter = record.find("_aSubmitter")) entry.author = text_of(*submitter, "_sName");
        const auto folded = mods::fold(wide(entry.name));
        entry.platform = game == kPcGame ? L"PC" : folded.find(L"switch") != std::wstring::npos ? L"Switch" : L"PS4";
        if (entry.id) entries.push_back(std::move(entry));
    }
    return entries;
}
inline std::vector<File> parse_download_page(const Json& page) {
    std::vector<File> files;
    const Json* list = page.find("_aFiles");
    if (!list || !list->is_array()) return files;
    for (const Json& item : list->items) {
        File file{number_of(item, "_idRow"), text_of(item, "_sFile"), text_of(item, "_sMd5Checksum"), text_of(item, "_sDownloadUrl"), number_of(item, "_nFilesize")};
        if (file.url.rfind("https://gamebanana.com/", 0) == 0 && !file.name.empty()) files.push_back(std::move(file));
    }
    return files;
}
inline std::vector<Entry> fetch_catalog(const std::atomic<bool>* cancel = nullptr) {
    std::vector<Entry> all;
    for (const int game : {kPcGame}) {
        bool complete = false;
        for (int page = 1; !complete && page <= 20; ++page) {
            const auto body = http_get(L"https://gamebanana.com/apiv11/Game/" + std::to_wstring(game) + L"/Subfeed?_nPage=" + std::to_wstring(page) + L"&_sSort=new", nullptr, {}, cancel);
            for (auto& entry : parse_subfeed(ctr_online::parse_json(body), game, &complete)) all.push_back(std::move(entry));
        }
    }
    std::vector<std::future<std::vector<File>>> pages;
    for (const auto& entry : all)
        pages.push_back(std::async(std::launch::async, [&entry, cancel] {
            return parse_download_page(ctr_online::parse_json(http_get(
                L"https://gamebanana.com/apiv11/" + wide(entry.model) + L"/" + std::to_wstring(entry.id) + L"/DownloadPage", nullptr, {}, cancel)));
        }));
    for (std::size_t i = 0; i < all.size(); ++i) all[i].files = pages[i].get();
    all.erase(std::remove_if(all.begin(), all.end(), [](const Entry& e) { return e.files.empty(); }), all.end());
    return all;
}

// --- Installed downloads (runtime/user/gamebanana.json) ---------------------------------------------
inline std::map<std::uint64_t, Installed> read_installed(const fs::path& path) {
    std::map<std::uint64_t, Installed> installed;
    std::error_code ec;
    if (!fs::exists(path, ec)) return installed;
    auto text = mods::read_text(path);
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
    const Json root = ctr_online::parse_json(text);
    for (const auto& [key, value] : root.members) {
        Installed item;
        item.file = text_of(value, "file");
        for (const char* list : {"mods", "patches"})
            if (const Json* names = value.find(list); names && names->is_array())
                for (const Json& name : names->items)
                    if (name.is_string()) (list[0] == 'm' ? item.mods : item.patches).push_back(wide(name.text));
        installed[std::strtoull(key.c_str(), nullptr, 10)] = std::move(item);
    }
    return installed;
}
inline void write_installed(const fs::path& path, const std::map<std::uint64_t, Installed>& installed) {
    Json root = Json::object();
    for (const auto& [id, item] : installed) {
        Json value = Json::object(), names_mods = Json::array(), names_patches = Json::array();
        value.set("file", Json::of(item.file));
        for (const auto& name : item.mods) names_mods.items.push_back(Json::of(mods::utf8(name)));
        for (const auto& name : item.patches) names_patches.items.push_back(Json::of(mods::utf8(name)));
        value.set("mods", std::move(names_mods));
        value.set("patches", std::move(names_patches));
        root.set(std::to_string(id), std::move(value));
    }
    fs::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary | std::ios::trunc) << ctr_online::dump_json_spaced(root) << "\n";
}

inline fs::path long_path(const fs::path& path);
// Removes what one download installed. Only the recorded names, only inside the two folders.
inline void remove_installed(const Installed& item, const fs::path& mods_dir, const fs::path& patches_dir) {
    for (const auto& name : item.mods)
        if (!name.empty() && fs::path(name).filename() == fs::path(name)) fs::remove_all(long_path(mods_dir / name));
    for (const auto& name : item.patches)
        if (!name.empty() && fs::path(name).filename() == fs::path(name)) fs::remove(patches_dir / name);
}

// --- Extraction and installation ---------------------------------------------------------------------
inline void extract(const fs::path& archive, const fs::path& into) {
    wchar_t system[MAX_PATH]{};
    GetSystemDirectoryW(system, MAX_PATH);
    const fs::path tar = fs::path(system) / L"tar.exe";
    std::error_code ec;
    if (!fs::exists(tar, ec)) throw std::runtime_error("Windows tar.exe not found: extract the archive into runtime/mods by hand");
    fs::create_directories(into);
    std::wstring command = L"\"" + tar.wstring() + L"\" -xf \"" + archive.wstring() + L"\" -C \"" + into.wstring() + L"\"";
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(tar.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
        throw std::runtime_error("cannot start tar.exe");
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (code != 0)
        throw std::runtime_error("tar.exe cannot extract " + mods::utf8(archive.filename().wstring()) +
                                 " (this Windows version may not read ." + mods::utf8(archive.extension().wstring()).substr(archive.has_extension() ? 1 : 0) +
                                 " archives): extract it into runtime/mods by hand");
}

inline std::wstring safe_name(const std::wstring& name) {
    std::wstring out;
    for (wchar_t c : name) out += (c < 32 || std::wcschr(L"<>:\"/\\|?*", c)) ? L'_' : c;
    while (!out.empty() && (out.back() == L' ' || out.back() == L'.')) out.pop_back();
    while (!out.empty() && (out.front() == L' ' || out.front() == L'.')) out.erase(0, 1);
    return out.empty() ? L"mod" : out;
}
// A Switch title folder such as 0100F9F00C696000 (16 hexadecimal digits).
inline bool switch_title(const std::wstring& name) {
    return name.size() == 16 && std::all_of(name.begin(), name.end(), [](wchar_t c) { return std::iswxdigit(c) != 0; });
}
// An absolute path in the \\?\ form: mods keep payloads deep enough to pass MAX_PATH.
inline fs::path long_path(const fs::path& path) {
    std::wstring text = fs::absolute(path).lexically_normal().wstring();
    if (text.rfind(LR"(\\?\)", 0) == 0) return text;
    for (auto& c : text) if (c == L'/') c = L'\\';
    return text.rfind(LR"(\\)", 0) == 0 ? LR"(\\?\UNC\)" + text.substr(2) : LR"(\\?\)" + text;
}

// mod.json (octr-mod) and nitro-mod.json manifests. The runtime applies (octr_mods.hpp) the
// archive_asset operations and the patch files of mod.json; anything else is refused here.
inline bool supported_capability(const std::string& name) {
    return name == "archive-assets" || name == "xml-patches" || name == "code-signatures";
}
struct ManifestSummary {
    std::size_t operations = 0, patches = 0;
    std::string problem;  // empty when the runtime can apply the manifest
};
inline ManifestSummary read_manifest(const fs::path& file) {
    ManifestSummary summary;
    Json manifest;
    try {
        auto text = mods::read_text(file);
        if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
        manifest = ctr_online::parse_json(text);
    } catch (const std::exception& e) {
        summary.problem = mods::utf8(file.filename().wstring()) + " cannot be read: " + e.what();
        return summary;
    }
    std::string missing;
    const auto add = [&](const std::string& name) { if (missing.find(name) == std::string::npos) missing += (missing.empty() ? "" : ", ") + name; };
    if (const Json* capabilities = manifest.find("requiresCapabilities"); capabilities && capabilities->is_array())
        for (const Json& capability : capabilities->items)
            if (capability.is_string() && !supported_capability(capability.text)) add(capability.text);
    if (const Json* operations = manifest.find("operations"); operations && operations->is_array())
        for (const Json& operation : operations->items) {
            const auto kind = text_of(operation, "kind");
            if (kind != "archive_asset") add("operation " + (kind.empty() ? std::string("?") : kind));
            ++summary.operations;
        }
    if (const Json* patches = manifest.find("patches"); patches && patches->is_array()) summary.patches = patches->items.size();
    const bool nitro = mods::fold(file.filename().wstring()) == L"nitro-mod.json";
    if (nitro && summary.operations) add("operations");
    if (!missing.empty()) summary.problem = "it needs features (" + missing + ") that the runtime cannot apply yet";
    return summary;
}
inline bool is_patch_xml(const fs::path& file) {
    if (mods::fold(file.extension().wstring()) != L".xml") return false;
    try {
        const auto text = mods::read_text(file);
        return text.find("<Patch>") != std::string::npos || text.find("<Patch ") != std::string::npos;
    } catch (const std::exception&) { return false; }
}

// Installs the extracted files of `staging` (a download, or a folder dropped on the launcher):
// - a folder with mod.json (octr-mod) is copied whole to runtime/mods/<name>;
// - otherwise game folders (or loose .pak files) become runtime/mods/<name>, and patch XML files
//   (also those of a nitro-mod.json asking only for XML patches) go to runtime/patches as
//   `patch_prefix` + file name (gb<id>- for downloads).
// `previous` (the earlier install of the same download) is removed first. Throws before writing
// anything when the download holds links, code (eboot.bin, sce_module, sce_sys), a manifest the
// runtime cannot apply, or nothing that can be installed.
inline Installed install_extracted(const fs::path& staging_folder, const fs::path& mods_dir, const fs::path& patches_dir,
                                   const std::wstring& patch_prefix, const std::wstring& title, const Installed* previous = nullptr) {
    const fs::path staging = long_path(staging_folder);
    std::vector<fs::path> octr, roots, patches, loose;  // loose: folders holding .pak files outside a game folder
    for (auto it = fs::recursive_directory_iterator(staging); it != fs::recursive_directory_iterator(); ++it) {
        const auto& path = it->path();
        if (mods::is_reparse(path)) throw std::runtime_error("the download contains a link: " + mods::utf8(path.lexically_relative(staging).generic_wstring()));
        const auto name = mods::fold(path.filename().wstring());
        if (name == L"eboot.bin" || name == L"sce_module" || name == L"sce_sys")
            throw std::runtime_error("the download contains game code (" + mods::utf8(path.filename().wstring()) + "), which mods cannot replace");
        if (it->is_regular_file() && (name == L"mod.json" || name == L"nitro-mod.json")) {
            if (const auto summary = read_manifest(path); !summary.problem.empty()) throw std::runtime_error(summary.problem);
            if (name == L"mod.json") octr.push_back(path.parent_path());
        }
    }
    const auto inside_octr = [&](const fs::path& path) {
        for (const auto& root : octr) {
            const auto relative = path.lexically_relative(root);
            if (!relative.empty() && *relative.begin() != L"..") return true;
        }
        return false;
    };
    for (auto it = fs::recursive_directory_iterator(staging); it != fs::recursive_directory_iterator(); ++it) {
        const auto& path = it->path();
        if (inside_octr(path)) { it.disable_recursion_pending(); continue; }
        if (it->is_directory() && mods::game_folder(path.filename().wstring())) {
            const auto root = path.parent_path();
            if (std::find(roots.begin(), roots.end(), root) == roots.end()) roots.push_back(root);
            it.disable_recursion_pending();  // game folders inside game folders are game content
        } else if (it->is_regular_file() && is_patch_xml(path)) {
            patches.push_back(path);
        } else if (it->is_regular_file() && mods::fold(path.extension().wstring()) == L".pak") {
            if (std::find(loose.begin(), loose.end(), path.parent_path()) == loose.end()) loose.push_back(path.parent_path());
        }
    }
    // Loose .pak files count only when the download has no game folder.
    const bool loose_paks = roots.empty() && !loose.empty();
    if (loose_paks) roots = loose;
    std::vector<std::pair<fs::path, bool>> units;  // folder, octr-mod
    for (const auto& root : octr) units.push_back({root, true});
    for (const auto& root : roots) units.push_back({root, false});
    if (units.empty() && patches.empty())
        throw std::runtime_error("the download has no mod.json, game folders (archives/, icons/, ...), .pak files or patch files");
    if (previous) remove_installed(*previous, mods_dir, patches_dir);
    Installed done;
    std::error_code ec;
    for (std::size_t i = 0; i < units.size(); ++i) {
        const auto& [root, whole] = units[i];
        std::wstring name = safe_name(title);
        if (units.size() > 1) {
            // Variants of one download: named after the nearest folder that is not a layout folder.
            fs::path label = root;
            while (label != staging && (mods::layout_folder(label.filename().wstring()) || mods::fold(label.filename().wstring()) == L"romfs" ||
                                        switch_title(label.filename().wstring())))
                label = label.parent_path();
            name += L" - " + (label == staging ? std::to_wstring(i + 1) : safe_name(label.filename().wstring()));
        }
        std::wstring unique = name;
        for (int n = 2; fs::exists(mods_dir / unique, ec); ++n) unique = name + L" (" + std::to_wstring(n) + L")";
        const fs::path target = long_path(mods_dir / unique);
        fs::create_directories(target);
        if (whole) {
            fs::copy(root, target, fs::copy_options::recursive);
        } else if (loose_paks) {
            fs::create_directories(target / L"archives");
            for (const auto& entry : fs::directory_iterator(root))
                if (entry.is_regular_file() && mods::fold(entry.path().extension().wstring()) == L".pak")
                    fs::copy_file(entry.path(), target / L"archives" / entry.path().filename());
        } else {
            for (const auto& entry : fs::directory_iterator(root))
                if (entry.is_directory() && mods::game_folder(entry.path().filename().wstring()))
                    fs::copy(entry.path(), target / entry.path().filename(), fs::copy_options::recursive);
        }
        done.mods.push_back(unique);
    }
    if (!patches.empty()) fs::create_directories(patches_dir);
    for (const auto& patch : patches) {
        const std::wstring name = patch_prefix + patch.filename().wstring();
        if (std::find(done.patches.begin(), done.patches.end(), name) != done.patches.end()) continue;
        fs::copy_file(patch, patches_dir / name, fs::copy_options::overwrite_existing);
        done.patches.push_back(name);
    }
    return done;
}
}  // namespace ctrport::gamebanana
