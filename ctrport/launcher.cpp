// SPDX-License-Identifier: GPL-2.0-only
// Crash Team Racing Native Ported: launcher (CTRNativePorted.exe), native Win32 + GDI+.
// Nitro Cinema presentation. Native launcher services and runtime integration retained.
// Presentation lives in nitro_*.hpp; game data is loaded read-only from the user dump.
// Zero external runtime dependencies (no Python). 100% compatible with launcher_config.json, lane lock, and ctrport.exe.
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include "nitro_layout.hpp"
#include "launcher_appearance.hpp"
#include "online_server/launcher_online.hpp"
#include "online_server/protocol.hpp"
#include "online_server/local_backup.hpp"
#include "online_server/friend_store.hpp"
#include "online_server/friend_store.hpp"
#include <commctrl.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <urlmon.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include "xbox_prepare.hpp"
#include "launcher_font.hpp"
#include "launcher_update.hpp"
#include "bounded_log.hpp"
#include "settings/store.hpp"
#include "settings/menu_catalog.hpp"
#include "guest_patch_xml.hpp"
#include "gamebanana.hpp"
#include "launcher_text.hpp"
#include "trophy_data.hpp"
#include "trophy_wiki.hpp"
#include <shlwapi.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <Xinput.h>
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;
using namespace Gdiplus;
using launcher_text::tr;
using launcher_text::trf;

namespace {
constexpr wchar_t kTitle[] = L"CTR Native Ported";
bool classic_skin = false;
constexpr int ID_LAUNCHER_SKIN = 909;
float kWidth = nitro::width, kHeight = nitro::height;
constexpr UINT WM_APP_FINISHED = WM_APP + 1;
constexpr UINT WM_APP_HOST_READY = WM_APP + 2;
constexpr UINT WM_APP_XBOX_READY = WM_APP + 3;
constexpr UINT WM_APP_SHADERS_PROGRESS = WM_APP + 4;
constexpr UINT WM_APP_SHADERS_READY = WM_APP + 5;
constexpr UINT WM_APP_GB_CATALOG = WM_APP + 6;
constexpr UINT WM_APP_GB_PROGRESS = WM_APP + 7;
constexpr UINT WM_APP_GB_DONE = WM_APP + 8;
constexpr UINT WM_APP_TROPHY_WIKI = WM_APP + 9;

// Design 2a Pit Lane Palette (from Claude Design token specification)
ARGB kArcadeBg      = 0xFF10152C;  // Deep arcade purple/indigo
ARGB kArcadeDark    = 0xFF0A1025;  // Dark ink outline & hard drop shadow
ARGB kArcadeCard    = 0xFF1B233D;  // Card surface
ARGB kArcadeCardSub = 0xFF10152C;  // Inset card surface
constexpr ARGB kArcadeGold    = 0xFFFFC93C;  // Trophy gold
constexpr ARGB kArcadeCyan    = 0xFF2FD3FF;  // Nitro cyan
constexpr ARGB kArcadeLime    = 0xFF8BE83A;  // Speed green
constexpr ARGB kArcadePink    = 0xFFFF3D8B;  // Accent pink
constexpr ARGB kArcadeOrange  = 0xFFFF7A1A;  // Warning orange
constexpr ARGB kTextLight     = 0xFFFFF8EC;  // Warm light text
ARGB kTextMuted     = 0xFFB9C2DA;  // Indigo muted text
ARGB kTextOff       = 0xFF56617D;  // Disabled / toggle off

enum Tab : int {
    TAB_HOME = 0,
    TAB_MODS,
    TAB_TROPHIES,
    TAB_SETTINGS,
    TAB_ADVANCED,
    TAB_COUNT
};

const wchar_t* const kTabNames[TAB_COUNT] = {
    L"HOME",
    L"MODS",
    L"TROPHIES",
    L"SETTINGS",
    L"ADVANCED"
};

const wchar_t* const kResolutions[] = {L"1280x720 (720p)", L"1920x1080 (1080p)", L"2560x1440 (1440p)", L"3840x2160 (4K)", L"Custom"};
// Upscaling mode: the share of the output resolution the 3D scene is rendered at
// (ctrport/settings/store.hpp upscaling_scale); used only with an upscaling method.
const wchar_t* const kModes[] = {L"native", L"quality", L"balanced", L"performance", L"ultra_performance"};
constexpr int ID_UPSCALER = 905;
const wchar_t* const kUpscalers[] = {L"off", L"fsr1", L"fsr3", L"dlss"};
const wchar_t* const kUpscalerLabels[] = {L"Off", L"AMD FSR 1", L"AMD FSR 3", L"NVIDIA DLSS"};
// Renderer: the GPU library (shadPS4 Vulkan) or the engine-state Direct3D 11 backend (ctrport/d3d).
constexpr int ID_RENDERER = 910;
const wchar_t* const kRenderers[] = {L"vulkan", L"d3d11"};
const wchar_t* const kRendererLabels[] = {L"Vulkan (shadPS4)", L"Direct3D 11 (native)"};
const wchar_t* const kModeLabels[] = {L"Native (100%)", L"Quality (67%)", L"Balanced (58%)", L"Performance (50%)", L"Ultra performance (33%)"};
const wchar_t* const kPills[] = {L"720p", L"1080p", L"1440p", L"4K", L"Custom"};
const int kPresetSize[][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};

struct GameLanguage { const wchar_t* code; const wchar_t* label; int system_id; };
constexpr GameLanguage kGameLanguages[] = {
    {L"en", L"English", 1}, {L"fr", L"Fran\u00e7ais", 2}, {L"it", L"Italiano", 5},
    {L"de", L"Deutsch", 4}, {L"es", L"Espa\u00f1ol", 3}, {L"ar", L"\u0627\u0644\u0639\u0631\u0628\u064a\u0629", 21}
};
int game_language_index(const std::wstring& code) {
    for (int i = 0; i < static_cast<int>(std::size(kGameLanguages)); ++i)
        if (code == kGameLanguages[i].code) return i;
    return 0;
}

enum : int {
    ID_GAME_LANGUAGE = 193, ID_SPLIT_SCREEN = 194,
    ID_MINIMIZE = 191, ID_CLOSE,
    ID_XBOX_PROMPTS = 190,
    ID_BASE_EDIT = 101, ID_BASE_BROWSE, ID_PATCH_EDIT, ID_PATCH_BROWSE,
    ID_PILL = 110, ID_WIDTH = 120, ID_HEIGHT, ID_FULLSCREEN, ID_FPS60, ID_PRO, ID_NATIVE_4K, ID_SCENE_RES, ID_UNLOCK, ID_LAUNCH,
    ID_TAB_BASE = 130, // 130..133 for 4 tabs
    ID_OPEN_MODS = 150,
    ID_SAVE_BACKUP = 160, ID_OPEN_SAVES, ID_OPEN_DUMP, ID_OPEN_LOGS, ID_AUDIO_TEST,
    ID_ONLINE_TOGGLE = 170, ID_ONLINE_ID_EDIT, ID_ONLINE_SERVER_EDIT, ID_ONLINE_PASSWORD_EDIT, ID_ONLINE_HOST, ID_ONLINE_FRIENDS, ID_WUMPA_X100
};

// MODS tab: a scrolling list (launcher_mods.hpp) drawn with a pool of kModRows row slots, each
// with a toggle and move buttons (installed mods, patches) or page and install buttons (GameBanana).
constexpr int kModRows = 9;
enum : int {
    ID_MODS_FILES = 200, ID_MODS_PATCHES, ID_MODS_MASTER, ID_MODS_REFRESH,
    ID_MOD_ROW_TOGGLE = 210, ID_MOD_ROW_UP = 220, ID_MOD_ROW_DOWN = 230,
    ID_TROPHY_KEY_EDIT = 240, ID_TROPHY_IMPORT,
    ID_PLAYER_PAD = 250,  // 250..253: the controller of players 1..4
    ID_GB_ROW_PAGE = 260, ID_GB_ROW_GET = 270  // GameBanana rows: open the mod's page, install or remove
};
// TROPHIES tab: a grid of the game's trophy icons.
constexpr int kTrophyColumns = 12;
constexpr float kTrophyLeft = 296, kTrophyTop = 124, kTrophyCell = 57, kTrophyIcon = 50;

struct Config {
    std::wstring base_dir, patch_dir, resolution = L"2560x1440 (1440p)", custom_w = L"1920", custom_h = L"1080";
    std::wstring game_language = L"en";
    std::wstring upscaling_mode = L"native";
    std::wstring upscaler = L"off";
    std::wstring renderer = L"d3d11";  // v0.7.1: Direct3D 11 is the default renderer
    bool use_patch = true, fps_60 = true, ps4_pro = true, native_4k = false, unlock_all = false, fullscreen = false;
    bool online = false;
    bool wumpa_x100 = false;
    bool xbox_prompts = true;
    bool split_screen = false;
    bool mods_enabled = true;
    std::wstring trophy_key;  // the user's PS4 trophy key (32 hex digits), optional
    // Controller of each player: "auto", "none", "keyboard", "xinput:N" or "hid:<device path>".
    std::wstring player_pad[4] = {L"auto", L"auto", L"auto", L"auto"};
    std::wstring online_id = L"CTRPlayer";
    std::wstring online_server = L"127.0.0.1";
    std::wstring online_friend_code;
    // Friend code of the server used for public matchmaking; empty: this launcher's own server.
    std::wstring public_server;
    // Hosting: the project server runs inside the launcher; online_public_ip overrides the
    // address found with UPnP (empty: automatic).
    bool online_host = false;
    std::wstring online_public_ip;
};

// ---- Settings JSON parsing ----
std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}
std::string narrow(const std::wstring& text) {
    if (text.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n, nullptr, nullptr);
    return out;
}
bool parse_flat_json(const std::wstring& s, std::map<std::wstring, std::wstring>& strings, std::map<std::wstring, bool>& bools) {
    std::size_t i = 0;
    const auto ws = [&] { while (i < s.size() && iswspace(s[i])) ++i; };
    const auto str = [&](std::wstring& out) {
        if (i >= s.size() || s[i] != L'"') return false;
        for (++i; i < s.size(); ++i) {
            const wchar_t c = s[i];
            if (c == L'"') { ++i; return true; }
            if (c != L'\\') { out += c; continue; }
            if (++i >= s.size()) return false;
            switch (s[i]) {
            case L'n': out += L'\n'; break;
            case L't': out += L'\t'; break;
            case L'r': out += L'\r'; break;
            case L'b': out += L'\b'; break;
            case L'f': out += L'\f'; break;
            case L'u':
                if (i + 4 >= s.size()) return false;
                out += static_cast<wchar_t>(std::stoul(s.substr(i + 1, 4), nullptr, 16));
                i += 4;
                break;
            default: out += s[i];
            }
        }
        return false;
    };
    ws();
    if (i >= s.size() || s[i++] != L'{') return false;
    for (;;) {
        ws();
        if (i < s.size() && s[i] == L'}') return true;
        std::wstring key;
        if (!str(key)) return false;
        ws();
        if (i >= s.size() || s[i++] != L':') return false;
        ws();
        if (i < s.size() && s[i] == L'"') {
            std::wstring value;
            if (!str(value)) return false;
            strings[key] = value;
        } else if (s.compare(i, 4, L"true") == 0) {
            bools[key] = true; i += 4;
        } else if (s.compare(i, 5, L"false") == 0) {
            bools[key] = false; i += 5;
        } else {
            while (i < s.size() && s[i] != L',' && s[i] != L'}') ++i;
        }
        ws();
        if (i < s.size() && s[i] == L',') { ++i; continue; }
        if (i < s.size() && s[i] == L'}') return true;
        return false;
    }
}
std::string json_string(const std::wstring& value) {
    std::string out = "\"";
    char buffer[8];
    for (wchar_t c : value) {
        if (c == L'"') out += "\\\"";
        else if (c == L'\\') out += "\\\\";
        else if (c == L'\n') out += "\\n";
        else if (c < 0x20 || c > 0x7e) { std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c)); out += buffer; }
        else out += static_cast<char>(c);
    }
    return out + "\"";
}

// Process helpers
std::wstring quote_argument(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) return arg;
    std::wstring out = L"\"";
    std::size_t slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') { ++slashes; continue; }
        out.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, L'\\');
    return out + L"\"";
}
std::wstring lower(std::wstring text) {
    if (!text.empty()) CharLowerBuffW(text.data(), static_cast<DWORD>(text.size()));
    return text;
}
std::wstring timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    wchar_t buffer[32];
    std::wcsftime(buffer, 32, L"%Y%m%d_%H%M%S", &local);
    return buffer;
}
std::string read_tail(const fs::path& path, std::size_t limit) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    const auto size = static_cast<std::size_t>(in.tellg());
    const std::size_t start = size > limit ? size - limit : 0;
    in.seekg(static_cast<std::streamoff>(start));
    std::string data(size - start, '\0');
    in.read(data.data(), static_cast<std::streamsize>(data.size()));
    return data;
}

// Exclusive lane lock guard
struct LaneLock {
    fs::path path;
    std::string token;
    bool held = false;
    bool acquire(const fs::path& root, HWND owner) {
        std::error_code ec;
        if (!fs::exists(root / "locks", ec)) return true;
        path = root / "locks/shared.lock";
        fs::create_directories(path.parent_path(), ec);
        std::random_device random;
        char hex[33];
        std::snprintf(hex, sizeof(hex), "%08x%08x%08x%08x", random(), random(), random(), random());
        token = hex;
        for (;;) {
            const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE) {
                const std::string owner_json = "{\"owner\": \"launcher\", \"pid\": " + std::to_string(GetCurrentProcessId()) +
                    ", \"timestamp\": " + std::to_string(std::time(nullptr)) + ", \"token\": \"" + token + "\"}";
                DWORD written = 0;
                WriteFile(file, owner_json.data(), static_cast<DWORD>(owner_json.size()), &written, nullptr);
                CloseHandle(file);
                held = true;
                return true;
            }
            if (GetLastError() != ERROR_FILE_EXISTS) {
                MessageBoxW(owner, trf(L"Unable to create lock file {0}", path.wstring()).c_str(), kTitle, MB_ICONERROR);
                return false;
            }
            const std::wstring text = tr(L"The runtime is currently in use by another process:") + std::wstring(L"\n\n") +
                widen(read_tail(path, 4096)) + L"\n\n" + tr(L"Retry?");
            if (MessageBoxW(owner, text.c_str(), kTitle, MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY) return false;
        }
    }
    void release() {
        if (!held) return;
        held = false;
        if (read_tail(path, 4096).find(token) != std::string::npos) DeleteFileW(path.c_str());
    }
};

enum class Kind { Browse, Toggle, Pill, Launch, TabBtn, ActionBtn, Segment };
struct Control { Kind kind; const wchar_t* label; bool* value; int pill; RectF box; int tab = -1; };
struct Status { std::wstring text; int kind = 0; };  // 0 neutral, 1 ok, 2 error

struct App {
    HWND window = nullptr, base_edit = nullptr, patch_edit = nullptr, width_edit = nullptr, height_edit = nullptr;
    HWND scene_combo = nullptr;
    HWND upscaler_combo = nullptr;
    HWND renderer_combo = nullptr;
    HWND patch_browse = nullptr, fps_toggle = nullptr, launch = nullptr, hot = nullptr;
    HWND online_toggle = nullptr, online_id_edit = nullptr;
    // Friends drawer: the rubrica (runtime/user/online/friends.json) with each friend's presence
    // from the local service; 110 copies the own code, 111 adds the code typed in 113, 109 removes.
    HWND friends_drawer = nullptr, friends_manage = nullptr;
    HWND friend_code_edit = nullptr, friend_add = nullptr, friend_remove = nullptr;
    std::wstring friends_notice;
    bool friends_notice_error = false;
    bool friends_open = false, friends_animating = false;
    float friends_reveal = 0, friends_from = 0;
    ULONGLONG friends_started = 0;
    int friends_scroll = 0, friends_selected = -1;
    bool lobby_scroll_drag = false;
    float lobby_scroll_grab = 0;
    int lobby_wheel_delta = 0;
    std::vector<ctr_online::FriendState> friends;
    ctr_online::Host host;
    bool host_busy = false;           // server starting or stopping in the background
    bool xbox_busy = false;           // Xbox prompt archives being prepared in the background
    bool xbox_ready = false;          // prepared for the launch that requested them
    bool shaders_busy = false;        // Direct3D 11 shaders being translated by ctr_shaders.exe
    bool shaders_ready = false;       // translated for the launch that requested them
    int shaders_percent = 0;
    ctr_online::HostStatus host_status;
    std::vector<HWND> pills;
    HWND language_combo = nullptr;
    std::vector<HWND> tab_buttons;
    std::map<HWND, Control> controls;
    // MODS tab (mod_overlay.hpp, guest_patch_xml.hpp)
    std::vector<ctrport::mods::Mod> mod_list;
    std::vector<std::wstring> mod_details;
    std::vector<bool> mod_bad;
    std::vector<ctrport::guest_patches::Patch> patch_list;
    std::vector<std::wstring> patch_details;
    std::vector<bool> patch_warn;
    ctrport::guest_patches::Selection patch_selection;
    std::wstring mods_notice, mods_game;
    bool mods_notice_error = false;
    int mods_view = 0;  // view 0: file mods (installed, then GameBanana), 1: patches
    float mods_scroll = 0;  // list scroll position (page coordinates)
    int mod_slot_item[kModRows]{};  // the list item each row slot shows, -1 when unused
    bool mod_row_on[kModRows]{};
    HWND mod_row_toggle[kModRows]{}, mod_row_up[kModRows]{}, mod_row_down[kModRows]{};
    // GameBanana rows (gamebanana.hpp): the online catalog and what was installed from it.
    std::vector<ctrport::gamebanana::Entry> gb_list;
    std::map<std::uint64_t, ctrport::gamebanana::Installed> gb_installed;
    std::wstring gb_error;
    bool gb_loading = false, gb_loaded = false;
    HWND gb_row_page[kModRows]{}, gb_row_get[kModRows]{};
    // The install in progress (a download or a dropped file): one at a time.
    bool mods_busy = false;
    std::uint64_t mods_busy_id = 0;  // GameBanana id, 0 for a dropped file
    int mods_busy_percent = -1;      // download progress, -1 while extracting and installing
    std::wstring mods_busy_name;
    // TROPHIES tab (trophy_data.hpp)
    ctr_trophy::Set trophy_set;
    std::map<int, std::int64_t> trophy_unlocked;
    std::vector<std::unique_ptr<Bitmap>> trophy_icons;
    std::wstring trophy_notice;
    bool trophy_notice_error = false;
    int trophy_hot = -1;
    // Without the trophy key the texts come from the Crash Bandicoot Wiki (trophy_wiki.hpp).
    bool trophy_wiki = false, trophy_wiki_loading = false, trophy_wiki_tried = false;
    std::wstring trophy_wiki_error;
    HWND trophy_key_edit = nullptr;
    // Controllers found on this PC, and the four player menus.
    struct PadOption { std::wstring value, label; };
    std::vector<PadOption> pads;
    HWND player_combo[4]{};
    std::vector<std::wstring> pad_values[4];  // the choice behind each menu entry
    float scale = 1.0f;
    fs::path exe_dir, root, runtime_exe;
    Config cfg;
    int resolution = 2; // Default 1440p
    int current_tab = TAB_HOME;
    bool updating = false, can_launch = false;
    Status base_status, patch_status, global;
    std::unique_ptr<Bitmap> art, icon, logo;
    fs::path art_source;
    bool art_checked = false;
    HBITMAP base_layer = nullptr;
    HFONT edit_font = nullptr;
    HBRUSH field_brush = nullptr, field_off_brush = nullptr;
    HICON window_icon = nullptr;
    HANDLE process = nullptr;
    fs::path log_stem;
    LaneLock lock;
} g;

constexpr int ID_CHECK_UPDATE = 906;
HWND update_button = nullptr;
HANDLE update_install_lock = INVALID_HANDLE_VALUE;
std::future<ctr_online::Json> update_job;
ctr_online::Json update_offer;
fs::path update_result;
bool update_preparing = false, update_manual = false, update_handoff = false;
std::wstring update_label = L"Check updates";

void set_update_label(const wchar_t* label) {
    update_label = label;
    if (update_button) {
        g.controls.at(update_button).label = update_label.c_str();
        SetWindowTextW(update_button, tr(update_label.c_str()));
        InvalidateRect(update_button, nullptr, FALSE);
    }
}
bool update_session_busy() {
    return g.process || g.host_busy || g.xbox_busy || g.shaders_busy || g.cfg.online || g.host.running();
}
void check_launcher_update(bool manual) {
    if (update_job.valid()) return;
    const auto script = g.exe_dir / "launcher_update.ps1";
    if (!fs::exists(script)) {
        if (manual) MessageBoxW(g.window, tr(L"launcher_update.ps1 is missing next to the launcher."), tr(L"Updates"), MB_ICONERROR);
        return;
    }
    try {
        const auto dir = g.root / "runtime/user/updates";
        ctr_update::assert_plain_path(dir);
        fs::create_directories(dir);
        update_result = dir / (L"check-" + std::to_wstring(GetCurrentProcessId()) + L".json");
        ctr_update::assert_plain_path(update_result);
        std::error_code ignored;
        fs::remove(update_result, ignored);
        update_manual = manual;
        update_preparing = false;
        set_update_label(L"Checking...");
        EnableWindow(update_button, FALSE);
        update_job = std::async(std::launch::async, [root=g.root, script, result=update_result] {
            return ctr_update::run(root, script, result, false);
        });
    } catch (const std::exception& e) {
        EnableWindow(update_button, TRUE);
        set_update_label(L"Check updates");
        if (manual) MessageBoxW(g.window, ctr_update::wide(e.what()).c_str(), tr(L"Updates"), MB_ICONERROR);
    }
}
void offer_launcher_update() {
    if (update_job.valid() || !update_offer.is_object()) return;
    if (!ctr_update::portable(g.root, g.exe_dir)) {
        MessageBoxW(g.window, tr(L"Automatic installation works only in the release folder."), tr(L"Updates"), MB_ICONINFORMATION); return;
    }
    if (update_session_busy()) {
        MessageBoxW(g.window, tr(L"Close the game and turn off Multiplayer Online first."), tr(L"Updates"), MB_ICONINFORMATION); return;
    }
    const auto error = update_offer.at("error").text;
    if (!error.empty()) {
        MessageBoxW(g.window, ctr_update::wide(error).c_str(), tr(L"Release cannot be installed"), MB_ICONERROR);
        update_offer = {}; set_update_label(L"Check updates"); return;
    }
    const std::wstring prompt = trf(L"Installed version: {0}\nNew version: {1}", ctr_update::wide(ctr_update::version),
            ctr_update::wide(update_offer.at("version").text) + (update_offer.at("prerelease").boolean ? L" (prerelease)" : L"")) +
        L"\n\n" + ctr_update::wide(update_offer.at("notes").text).substr(0, 1800) +
        L"\n\n" + tr(L"Download and install? The launcher restarts; saves, settings and dump are kept.");
    if (MessageBoxW(g.window, prompt.c_str(), tr(L"Update available"), MB_YESNO | MB_ICONINFORMATION | MB_DEFBUTTON2) != IDYES) return;
    update_preparing = true;
    set_update_label(L"Downloading...");
    EnableWindow(g.window, FALSE);
    update_job = std::async(std::launch::async, [root=g.root, script=g.exe_dir / "launcher_update.ps1", result=update_result] {
        return ctr_update::run(root, script, result, true);
    });
}
void poll_launcher_update() {
    if (!update_job.valid() || update_job.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    const bool preparing = update_preparing;
    update_preparing = false;
    EnableWindow(g.window, TRUE);
    EnableWindow(update_button, TRUE);
    try {
        auto response = update_job.get();
        if (!response.at("ok").boolean) {
            set_update_label(L"Check updates");
            update_offer = {};
            if (preparing || update_manual)
                MessageBoxW(g.window, ctr_update::wide(response.at("error").text).c_str(), tr(L"Update failed"), MB_ICONERROR);
            return;
        }
        if (preparing) {
            if (update_session_busy()) throw std::runtime_error("Close the game and disable Online, then retry.");
            const fs::path txn = ctr_update::wide(response.at("transaction").text);
            ctr_update::launch_apply(g.root, txn / "apply.ps1", txn);
            update_handoff = true;
            SendMessageW(g.window, WM_CLOSE, 0, 0);
            return;
        }
        if (!response.at("available").boolean) {
            set_update_label(L"Check updates");
            if (update_manual) MessageBoxW(g.window, tr(L"No newer version available."), tr(L"Updates"), MB_ICONINFORMATION);
            return;
        }
        update_offer = response.at("offer");
        set_update_label(L"Update available");
        if (update_manual || !update_session_busy()) offer_launcher_update();
    } catch (const std::exception& e) {
        update_offer = {};
        set_update_label(L"Check updates");
        MessageBoxW(g.window, ctr_update::wide(e.what()).c_str(), tr(L"Update failed"), MB_ICONERROR);
    }
}

#include "nitro_state.hpp"

int px(float logical) { return static_cast<int>(std::lround(logical * g.scale)); }
RectF rect(float x, float y, float w, float h) { return RectF(x, y, w, h); }

fs::path config_path() { return (g.root.empty() ? g.exe_dir : g.root / "runtime/user") / "launcher_config.json"; }

fs::path resolve(const std::wstring& folder) {
    fs::path path(folder);
    if (!folder.empty() && path.is_relative() && !g.root.empty()) path = (g.root / path).lexically_normal();
    return path;
}
std::wstring relative_to_root(const std::wstring& folder) {
    if (g.root.empty()) return folder;
    const fs::path relative = fs::path(folder).lexically_relative(g.root);
    return relative.empty() || relative.native().starts_with(L"..") ? folder : relative.wstring();
}

ctr_online::Json settings_last_ui_snapshot = ctr_online::Json::object();
ctr_online::Json settings_ui_snapshot();

void load_config() {
    std::error_code ec;
    for (const fs::path candidate : {fs::path(L"dump\\CUSA14876")})
        if (!candidate.empty() && fs::exists(resolve(candidate.wstring()) / "eboot.bin", ec)) { g.cfg.base_dir = candidate.wstring(); break; }
    for (const fs::path candidate : {fs::path(L"dump\\CUSA14876-patch"),
                                     resolve(g.cfg.base_dir).parent_path() / L"CUSA14876-patch"})
        if (!candidate.empty() && fs::exists(resolve(candidate.wstring()) / "eboot.bin", ec)) { g.cfg.patch_dir = candidate.wstring(); g.cfg.use_patch = true; break; }
    ctr_online::Json document = ctr_online::Json::object();
    try { document = ctrport::settings::Store(config_path()).read(); }
    catch (const std::exception& error) {
        const std::wstring message = tr(L"Settings could not be loaded. The file has been preserved.") + std::wstring(L"\n") + widen(error.what()) +
            L"\n\n" + tr(L"Restore the previous settings? The current file will be preserved separately.");
        if (MessageBoxW(nullptr, message.c_str(), tr(L"Settings recovery"), MB_YESNO | MB_ICONWARNING) == IDYES) {
            try {
                ctrport::settings::Store(config_path()).restore_backup();
                document = ctrport::settings::Store(config_path()).read();
            } catch (const std::exception& recovery) {
                MessageBoxW(nullptr, widen(recovery.what()).c_str(), tr(L"Settings recovery failed"), MB_ICONERROR);
            }
        }
    }
    {
        std::map<std::wstring, std::wstring> strings;
        std::map<std::wstring, bool> bools;
        for (const auto& item : document.members) {
            if (item.second.is_string()) strings.emplace(widen(item.first), widen(item.second.text));
            else if (item.second.is_bool()) bools.emplace(widen(item.first), item.second.boolean);
        }
        const bool parsed = true;
        if (parsed) {
            auto text = [&](const wchar_t* key, std::wstring& out) { if (strings.count(key)) out = strings[key]; };
            auto flag = [&](const wchar_t* key, bool& out) { if (bools.count(key)) out = bools[key]; };
            text(L"base_dir", g.cfg.base_dir); text(L"patch_dir", g.cfg.patch_dir); text(L"resolution", g.cfg.resolution);
            text(L"game_language", g.cfg.game_language);
            text(L"custom_w", g.cfg.custom_w); text(L"custom_h", g.cfg.custom_h);
            text(L"online_id", g.cfg.online_id); text(L"online_server", g.cfg.online_server); text(L"online_friend_code", g.cfg.online_friend_code);
            text(L"online_public_ip", g.cfg.online_public_ip); flag(L"online_host", g.cfg.online_host);
            text(L"public_server", g.cfg.public_server);
            flag(L"use_patch", g.cfg.use_patch); flag(L"fps_60", g.cfg.fps_60); flag(L"ps4_pro", g.cfg.ps4_pro);
            flag(L"split_screen", g.cfg.split_screen); flag(L"mods_enabled", g.cfg.mods_enabled); flag(L"xbox_prompts", g.cfg.xbox_prompts); flag(L"native_4k", g.cfg.native_4k); flag(L"unlock_all", g.cfg.unlock_all); flag(L"fullscreen", g.cfg.fullscreen);
            // older files: the mode nearest to their internal 3D resolution
            const auto migrated = ctrport::settings::effective(document);
            if (const auto* mode = migrated.find("upscaling_mode")) g.cfg.upscaling_mode = widen(mode->text);
            text(L"upscaler", g.cfg.upscaler);
            text(L"renderer", g.cfg.renderer);
            text(L"trophy_key", g.cfg.trophy_key);
            text(L"player1_pad", g.cfg.player_pad[0]); text(L"player2_pad", g.cfg.player_pad[1]);
            text(L"player3_pad", g.cfg.player_pad[2]); text(L"player4_pad", g.cfg.player_pad[3]);
            flag(L"online", g.cfg.online); flag(L"wumpa_x100", g.cfg.wumpa_x100);
        }
    }
    if (std::find(std::begin(kModes), std::end(kModes), g.cfg.upscaling_mode) == std::end(kModes)) g.cfg.upscaling_mode = L"native";
    if (std::find(std::begin(kUpscalers), std::end(kUpscalers), g.cfg.upscaler) == std::end(kUpscalers)) g.cfg.upscaler = L"off";
    if (std::find(std::begin(kRenderers), std::end(kRenderers), g.cfg.renderer) == std::end(kRenderers)) g.cfg.renderer = L"d3d11";
    // v0.7.1 makes Direct3D 11 the default. The v0.7.0 launcher saved its renderer (Vulkan unless
    // changed) in every file, so each file switches once; renderer_default records it, and a
    // later choice of Vulkan stays. A package without ctr_d3d.dll keeps Vulkan (validate).
    if (!document.find("renderer_default")) {
        g.cfg.renderer = L"d3d11";
        try {
            auto changes = ctr_online::Json::object();
            changes.set("renderer", ctr_online::Json::of("d3d11"));
            changes.set("renderer_default", ctr_online::Json::of("d3d11"));
            ctrport::settings::Store(config_path()).update(changes);
        } catch (const std::exception&) {}
    }
    g.cfg.game_language = kGameLanguages[game_language_index(g.cfg.game_language)].code;
    launcher_text::set_language(g.cfg.game_language);
    if (g.cfg.online_id.empty()) g.cfg.online_id = L"CTRPlayer";
    if (g.cfg.online_server.empty()) g.cfg.online_server = L"127.0.0.1";
    for (std::wstring* folder : {&g.cfg.base_dir, &g.cfg.patch_dir})
        if (!folder->empty()) *folder = fs::path(*folder).make_preferred().wstring();
    settings_last_ui_snapshot = settings_ui_snapshot();
    g.resolution = 2; // Default 1440p
    for (int i = 0; i < 5; ++i)
        if (g.cfg.resolution == kResolutions[i] || (i == 4 && g.cfg.resolution == L"Personalizzata")) g.resolution = i;
}

std::wstring window_text(HWND control) {
    std::wstring text(static_cast<std::size_t>(GetWindowTextLengthW(control)) + 1, L'\0');
    GetWindowTextW(control, text.data(), static_cast<int>(text.size()));
    text.resize(wcslen(text.c_str()));
    return text;
}

ctr_online::Json settings_ui_snapshot() {
    const auto flag = [](bool v) { return std::string(v ? "true" : "false"); };
    const std::string json = "{\n  \"upscaler\": " + json_string(g.cfg.upscaler) + ",\n  \"renderer\": " + json_string(g.cfg.renderer) + ",\n  \"upscaling_mode\": " + json_string(g.cfg.upscaling_mode) + ",\n  \"base_dir\": " + json_string(g.cfg.base_dir) + ",\n  \"patch_dir\": " + json_string(g.cfg.patch_dir) +
        ",\n  \"use_patch\": " + flag(g.cfg.use_patch) + ",\n  \"fps_60\": " + flag(g.cfg.fps_60) + ",\n  \"ps4_pro\": " + flag(g.cfg.ps4_pro) +
        ",\n  \"native_4k\": " + flag(g.cfg.native_4k) + ",\n  \"unlock_all\": " + flag(g.cfg.unlock_all) + ",\n  \"fullscreen\": " + flag(g.cfg.fullscreen) + ",\n  \"xbox_prompts\": " + flag(g.cfg.xbox_prompts) + ",\n  \"split_screen\": " + flag(g.cfg.split_screen) + ",\n  \"mods_enabled\": " + flag(g.cfg.mods_enabled) +
        ",\n  \"resolution\": " + json_string(g.cfg.resolution) + ",\n  \"custom_w\": " + json_string(g.cfg.custom_w) +
        ",\n  \"custom_h\": " + json_string(g.cfg.custom_h) + ",\n  \"game_language\": " + json_string(g.cfg.game_language) +
        ",\n  \"online\": " + flag(g.cfg.online) +
        ",\n  \"wumpa_x100\": " + flag(g.cfg.wumpa_x100) +
        ",\n  \"online_id\": " + json_string(g.cfg.online_id) +
        ",\n  \"online_server\": " + json_string(g.cfg.online_server) +
        ",\n  \"online_friend_code\": " + json_string(g.cfg.online_friend_code) +
        ",\n  \"public_server\": " + json_string(g.cfg.public_server) +
        ",\n  \"online_host\": " + flag(g.cfg.online_host) +
        ",\n  \"online_public_ip\": " + json_string(g.cfg.online_public_ip) +
        ",\n  \"trophy_key\": " + json_string(g.cfg.trophy_key) +
        ",\n  \"player1_pad\": " + json_string(g.cfg.player_pad[0]) + ",\n  \"player2_pad\": " + json_string(g.cfg.player_pad[1]) +
        ",\n  \"player3_pad\": " + json_string(g.cfg.player_pad[2]) + ",\n  \"player4_pad\": " + json_string(g.cfg.player_pad[3]) + "\n}";
    return ctr_online::parse_json(json);
}

bool save_config() {
    g.cfg.base_dir = window_text(g.base_edit);
    g.cfg.patch_dir = window_text(g.patch_edit);
    g.cfg.resolution = kResolutions[g.resolution];
    if (g.resolution == 4) { g.cfg.custom_w = window_text(g.width_edit); g.cfg.custom_h = window_text(g.height_edit); }
    if (g.online_id_edit) g.cfg.online_id = window_text(g.online_id_edit);
    const auto json = ctr_online::dump_json(settings_ui_snapshot());
    try {
        auto snapshot = ctr_online::parse_json(json);
        auto changes = ctrport::settings::changed_fields(settings_last_ui_snapshot, snapshot);
        ctrport::settings::Store(config_path()).update(changes);
        settings_last_ui_snapshot = std::move(snapshot);
        return true;
    } catch (const std::exception& error) {
        MessageBoxW(g.window, widen(error.what()).c_str(), tr(L"Settings were not saved"), MB_ICONERROR);
        return false;
    }
}

// Drawing helpers
void round_rect(GraphicsPath& path, const RectF& r, float radius) {
    const float d = radius * 2;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
}
void fill_round(Graphics& gr, const RectF& r, float radius, const Brush& brush) {
    GraphicsPath path;
    round_rect(path, r, radius);
    gr.FillPath(&brush, &path);
}
void stroke_round(Graphics& gr, const RectF& r, float radius, ARGB color, float width) {
    GraphicsPath path;
    round_rect(path, r, radius);
    Pen pen(Color(color), width);
    gr.DrawPath(&pen, &path);
}
ARGB fade(ARGB color, float alpha) {
    return (static_cast<ARGB>(static_cast<float>(color >> 24) * alpha) << 24) | (color & 0xFFFFFF);
}

// Slanted box with hard drop shadow (the core motif of Option 2a)
void draw_slanted_box(Graphics& gr, const RectF& r, float skew_x, float shadow_d, const Brush& fill, ARGB border, float border_w) {
    GraphicsPath body;
    round_rect(body, rect(0, 0, r.Width, r.Height), std::min(12.f, r.Height / 3));
    Matrix skew(1, 0, -skew_x, 1, r.X + r.Height * skew_x, r.Y);
    body.Transform(&skew);
    if (shadow_d > 0) {
        std::unique_ptr<GraphicsPath> shadow(body.Clone());
        Matrix offset;
        offset.Translate(0, shadow_d);
        shadow->Transform(&offset);
        SolidBrush ink{Color(kArcadeDark)};
        gr.FillPath(&ink, shadow.get());
    }
    gr.FillPath(&fill, &body);
    if (border_w > 0) {
        Pen pen(Color(border), border_w);
        gr.DrawPath(&pen, &body);
    }
}

ctrport::launcher_font::Set retail_fonts;
std::future<ctrport::launcher_font::Set> font_load;
std::wstring font_requested_key, font_loading_key;
std::vector<fs::path> font_requested_sources;
std::unique_ptr<FontFamily> family_cache[4];
const FontFamily& family(int which) {
    static const wchar_t* const names[4][3] = {{L"Lilita One", L"Segoe UI Black", L"Arial Black"},
                                               {L"Rubik", L"Segoe UI Semibold", L"Arial"},
                                               {L"Segoe UI Semibold", L"Segoe UI", L"Arial"},
                                               {L"Bahnschrift SemiBold", L"Segoe UI Semibold", L"Arial"}};
    auto& cached = family_cache[which];
    if (!cached) {
        for (const wchar_t* name : names[which]) {
            auto candidate = std::make_unique<FontFamily>(name);
            if (candidate->IsAvailable()) { cached = std::move(candidate); break; }
        }
        if (!cached) cached = std::unique_ptr<FontFamily>(FontFamily::GenericSansSerif()->Clone());
    }
    return *cached;
}
int usable_style(const FontFamily& font_family, int style) {
    for (int candidate : {style, style & ~FontStyleBold, style & FontStyleItalic, 0})
        if (font_family.IsStyleAvailable(candidate)) return candidate;
    return 0;
}
void text(Graphics& gr, const std::wstring& value, int which, float size, int style, const RectF& r, ARGB color,
          StringAlignment align = StringAlignmentNear) {
    gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const auto* retail = which == 0 ? retail_fonts.title.get() : classic_skin ? retail_fonts.body.get() : nullptr;
    if (retail && retail->draw(gr, value, which == 0 ? size : size * 1.25f, r, color, align)) return;
    const int usable = usable_style(family(which), style);
    StringFormat format;
    format.SetAlignment(align);
    format.SetLineAlignment(StringAlignmentCenter);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    if (value.find(L'\n') == std::wstring::npos) {
        format.SetFormatFlags(StringFormatFlagsNoWrap);
        // a line too wide (longer words in other languages) is drawn smaller, down to 65%
        Font natural(&family(which), size, usable, UnitPixel);
        RectF needed;
        gr.MeasureString(value.c_str(), -1, &natural, PointF(0, 0), &format, &needed);
        if (needed.Width > r.Width && needed.Width > 0) size *= std::max(0.65f, r.Width / needed.Width);
    }
    Font font(&family(which), size, usable, UnitPixel);
    SolidBrush brush{Color(color)};
    gr.DrawString(value.c_str(), -1, &font, r, &format, &brush);
}
void nitro_resolve(HDC target,HBITMAP high,int w,int h) {
    // Resolve via GDI+ rather than display-driver-dependent GDI HALFTONE.
    Bitmap image(high,nullptr);Graphics output(target);
    output.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    output.SetPixelOffsetMode(PixelOffsetModeHalf);
    ImageAttributes attr;attr.SetWrapMode(WrapModeTileFlipXY);
    output.DrawImage(&image,Rect(0,0,w,h),0,0,w*nitro::samples,h*nitro::samples,UnitPixel,&attr);
}
void draw_cover(Graphics& gr, Bitmap& image, const RectF& dest) {
    const float iw = static_cast<float>(image.GetWidth()), ih = static_cast<float>(image.GetHeight());
    const float ratio = std::max(dest.Width / iw, dest.Height / ih);
    const float sw = dest.Width / ratio, sh = dest.Height / ratio;
    ImageAttributes attributes;
    attributes.SetWrapMode(WrapModeTileFlipXY);
    gr.DrawImage(&image, dest, (iw - sw) / 2, 0, sw, sh, UnitPixel, &attributes);
}

void select_tab(int tab);
void sync_mod_rows();

#include "classic_shell.hpp"
#include "nitro_shell.hpp"

void draw_check(Graphics& gr, float x, float y, int kind) {
    const ARGB color = kind == 1 ? kArcadeLime : kArcadeOrange;
    SolidBrush circle{Color(fade(color, 0.25f))};
    gr.FillEllipse(&circle, x, y, 16.f, 16.f);
    Pen pen(Color(color), 2.5f);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    if (kind == 1) {
        PointF tick[] = {PointF(x + 4.0f, y + 8.5f), PointF(x + 7.0f, y + 11.5f), PointF(x + 12.0f, y + 5.0f)};
        gr.DrawLines(&pen, tick, 3);
    } else {
        gr.DrawLine(&pen, x + 5, y + 5, x + 11, y + 11);
        gr.DrawLine(&pen, x + 11, y + 5, x + 5, y + 11);
    }
}
void draw_status(Graphics& gr, const Status& status, float x, float y, float w) {
    if (status.kind) draw_check(gr, x, y + 2, status.kind);
    const float offset = status.kind ? 22.f : 0.f;
    text(gr, status.text, 1, 12.0f, FontStyleRegular, rect(x + offset, y, w - offset, 20),
         status.kind == 1 ? kArcadeLime : status.kind == 2 ? kArcadeOrange : kTextMuted);
}
void draw_field(Graphics& gr, const RectF& r, HWND edit) {
    const bool enabled = IsWindowEnabled(edit) != FALSE, focused = GetFocus() == edit;
    fill_round(gr, r, 10, SolidBrush(Color(enabled ? kArcadeCardSub : 0xFF0D0A22)));
    stroke_round(gr, r, 10, focused ? kArcadeGold : kArcadeDark, focused ? 3.0f : 2.5f);
}

// Right-edge friends drawer: every friend of the rubrica, with the presence the local service
// last received from that friend's own service (offline while Online is off).
fs::path friends_path() { return g.root / "runtime/user/online/friends.json"; }
void reload_friends() {
    const auto live = g.host.friends();
    // The last name each friend's service reported, kept by the service (friend_names.json):
    // shown also while Online is off.
    ctr_online::Json saved_names = ctr_online::Json::object();
    try {
        std::ifstream file(friends_path().parent_path() / "friend_names.json", std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        if (file) saved_names = ctr_online::parse_json(text.str());
    } catch (const std::exception&) {}
    std::vector<ctr_online::FriendState> rows;
    for (const auto& entry : ctr_online::FriendStore(friends_path()).entries()) {
        ctr_online::FriendState row;
        for (const auto& state : live) if (state.code == entry.friend_code) row = state;
        if (row.name.empty() && saved_names.is_object())
            if (const auto* name = saved_names.find(entry.friend_code); name && name->is_string()) row.name = name->text;
        row.code = entry.friend_code;
        row.nickname = entry.nickname;
        rows.push_back(std::move(row));
    }
    std::stable_sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
        const auto rank = [](const auto& row) { return row.in_game ? 0 : row.online ? 1 : 2; };
        return rank(a) < rank(b);
    });
    const std::string selected = g.friends_selected >= 0 && g.friends_selected < static_cast<int>(g.friends.size())
        ? g.friends[g.friends_selected].code : std::string();
    g.friends = std::move(rows);
    g.friends_selected = -1;
    for (int i = 0; i < static_cast<int>(g.friends.size()); ++i) if (g.friends[i].code == selected) g.friends_selected = i;
    g.friends_scroll = std::clamp(g.friends_scroll, 0, std::max(0, static_cast<int>(g.friends.size()) - 4));
}
std::wstring friend_label(const ctr_online::FriendState& row) {
    return widen(row.name.empty() ? row.nickname : row.name);
}
void paint_window(HDC target);
constexpr UINT_PTR kFriendsTimer = 1;
float kFriendsTravel = 378.f;
void friend_action(int action);
void update_friend_controls();

RectF lobby_scroll_thumb() {
    const auto count=static_cast<int>(g.friends.size());
    const float height=count>4 ? 164.f*4.f/static_cast<float>(count) : 164.f;
    const float offset=count>4 ? (164.f-height)*static_cast<float>(g.friends_scroll)/static_cast<float>(count-4) : 0.f;
    return rect(329,78+offset,10,height);
}
void drag_lobby_scroll(HWND window,float y) {
    const auto thumb=lobby_scroll_thumb();
    const int maximum=std::max(0,static_cast<int>(g.friends.size())-4);
    if(maximum && thumb.Height<164.f) {
        const float progress=std::clamp((y-78.f-g.lobby_scroll_grab)/(164.f-thumb.Height),0.f,1.f);
        g.friends_scroll=static_cast<int>(std::lround(progress*static_cast<float>(maximum)));
        InvalidateRect(window,nullptr,FALSE);
    }
}

void update_friend_controls() {
    EnableWindow(g.friends_manage, TRUE);
    EnableWindow(g.friend_add, !g.process);
    EnableWindow(g.friend_remove, !g.process && g.friends_selected >= 0);
    for (HWND control : {g.friends_manage, g.friend_add, g.friend_remove}) InvalidateRect(control, nullptr, FALSE);
}
constexpr float kFriendsDuration = 320.f;

// Match Brief2's CSS cubic-bezier(.3,1.4,.5,1), including its gentle overshoot.
float friends_ease(float progress) { return nitro::ease(progress); }
void position_friends() {
    // Keep a full-size surface while translating it; the parent clips its right
    // edge. No binary HRGN cuts through the antialiased rounded outlines.
    SetWindowPos(g.friends_drawer, HWND_TOP, px(kWidth - (classic_skin?38.f:0.f) - g.friends_reveal), px(classic_skin?62.f:100.f),
                 px(370), px(530), SWP_NOACTIVATE | SWP_NOCOPYBITS);
    ShowWindow(g.friends_drawer, g.current_tab == TAB_HOME ? SW_SHOWNA : SW_HIDE);
    const bool show=g.current_tab==TAB_HOME && g.friends_open && !g.friends_animating;
    for(HWND control:{g.friends_manage,g.friend_code_edit,g.friend_add,g.friend_remove})
        ShowWindow(control,show ? SW_SHOWNA : SW_HIDE);
    update_friend_controls();
    RECT damage{px(kWidth-kFriendsTravel-38),px(classic_skin?62.f:100.f),px(kWidth),px(classic_skin?592.f:630.f)};
    InvalidateRect(g.window,&damage,FALSE);
    InvalidateRect(g.friends_drawer,nullptr,FALSE);
}
void animate_friends(bool open) {
    if (g.current_tab != TAB_HOME) return;
    if(g.lobby_scroll_drag){g.lobby_scroll_drag=false;ReleaseCapture();}
    g.friends_open = open;
    g.friends_from = g.friends_reveal;
    g.friends_started = GetTickCount64();
    g.friends_animating = true;
    reload_friends();
    if(!nitro_motion) {g.friends_reveal=open?kFriendsTravel:0;g.friends_animating=false;}
    else SetTimer(g.friends_drawer, kFriendsTimer, 15, nullptr);
    position_friends();
}
void reset_friends() {
    if(g.lobby_scroll_drag){g.lobby_scroll_drag=false;ReleaseCapture();}
    KillTimer(g.friends_drawer, kFriendsTimer);
    g.friends_open = g.friends_animating = false;
    g.friends_reveal = 0;
}
LRESULT CALLBACK friends_drawer_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_CREATE: {
        auto button=[&](int id,const wchar_t* title,float x,float y,float w) {
            return CreateWindowExW(0,L"BUTTON",title,WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,
                px(x),px(y),px(w),px(32),window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);
        };
        g.friends_manage=button(110,tr(L"COPY MY CODE"),50,256,290);
        g.friend_code_edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,
            px(60),px(330),px(270),px(20),window,reinterpret_cast<HMENU>(113),nullptr,nullptr);
        SendMessageW(g.friend_code_edit,WM_SETFONT,reinterpret_cast<WPARAM>(g.edit_font),FALSE);
        SendMessageW(g.friend_code_edit,EM_SETLIMITTEXT,512,0);
        SendMessageW(g.friend_code_edit,EM_SETCUEBANNER,TRUE,reinterpret_cast<LPARAM>(tr(L"Paste a friend's code")));
        g.friend_add=button(111,tr(L"ADD"),50,366,140);
        g.friend_remove=button(109,tr(L"REMOVE"),200,366,140);
        return 0;
    }
    case WM_COMMAND:
        if(HIWORD(wp)==BN_CLICKED && (LOWORD(wp)==110 || LOWORD(wp)==111 || LOWORD(wp)==109))
            friend_action(LOWORD(wp));
        if(HIWORD(wp)==EN_SETFOCUS || HIWORD(wp)==EN_KILLFOCUS)InvalidateRect(window,nullptr,FALSE);
        return 0;
    case WM_CTLCOLOREDIT: {
        const auto dc=reinterpret_cast<HDC>(wp);
        SetTextColor(dc,RGB(255,248,236));SetBkColor(dc,RGB(27,20,70));
        return reinterpret_cast<LRESULT>(g.field_brush);
    }
    case WM_DRAWITEM: {
        const auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        Graphics gr(item->hDC);gr.Clear(Color(kArcadeCard));gr.ScaleTransform(g.scale,g.scale);
        gr.SetSmoothingMode(SmoothingModeAntiAlias);gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        const float width=item->CtlID==110 ? 290.f : 140.f;
        const bool enabled=(item->itemState & ODS_DISABLED)==0;
        const auto color=!enabled ? kArcadeCardSub : item->CtlID==109 ? kArcadePink : kArcadeCyan;
        fill_round(gr,rect(0,0,width,32),9,SolidBrush(Color(color)));
        const auto label=tr(item->CtlID==110 ? L"COPY MY CODE" : item->CtlID==111 ? L"ADD" : L"REMOVE");
        text(gr,label,1,12,FontStyleBold,rect(0,0,width,32),enabled ? kArcadeDark : kTextMuted,StringAlignmentCenter);
        if(item->itemState & (ODS_FOCUS|ODS_SELECTED))stroke_round(gr,rect(2,2,width-4,28),7,kArcadeGold,2);
        return TRUE;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (wp == kFriendsTimer && g.friends_animating) {
            const float elapsed = static_cast<float>(GetTickCount64() - g.friends_started);
            const float progress = std::min(1.f, elapsed / kFriendsDuration);
            const float target = g.friends_open ? kFriendsTravel : 0.f;
            g.friends_reveal = g.friends_from + (target - g.friends_from) * friends_ease(progress);
            // The closing overshoot ends beyond the right edge, as in the HTML.
            if (progress >= 1.f) {
                g.friends_reveal = target;
                g.friends_animating = false;
                KillTimer(window, kFriendsTimer);
            }
            position_friends();
        }
        return 0;
    case WM_NCHITTEST: {
        POINT pt{static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp))};
        ScreenToClient(window, &pt);
        if (pt.x < px(38) && (pt.y < px(18) || pt.y > px(172))) return HTTRANSPARENT;
        break;
    }
    case WM_DESTROY:
        KillTimer(window, kFriendsTimer);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC target = BeginPaint(window, &ps);
        HDC dc = CreateCompatibleDC(target);
        HBITMAP bitmap = CreateCompatibleBitmap(target, px(370), px(530));
        const HGDIOBJ previous = SelectObject(dc, bitmap);
        // Composite onto the exact Home background so curves have smooth edge
        // pixels instead of the old mismatched GDI region radius (diameter).
        HDC backdrop=CreateCompatibleDC(target);const auto backdrop_old=SelectObject(backdrop,g.base_layer);
        BitBlt(dc,0,0,px(370),px(530),backdrop,px(kWidth-(classic_skin?38.f:0.f)-g.friends_reveal),px(classic_skin?62.f:100.f),SRCCOPY);
        SelectObject(backdrop,backdrop_old);DeleteDC(backdrop);
        {
            Graphics gr(dc);
            gr.ScaleTransform(g.scale, g.scale);
            gr.SetSmoothingMode(SmoothingModeAntiAlias);
            gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            gr.SetPixelOffsetMode(PixelOffsetModeHalf);
            fill_round(gr, rect(0,23,46,150),12,SolidBrush(Color(kArcadeDark)));
            fill_round(gr, rect(4,19,44,148),12,SolidBrush(Color(kArcadePink)));
            stroke_round(gr, rect(4,19,44,148),12,kArcadeDark,3);
            text(gr, g.friends_open ? L">" : L"<",1,18,FontStyleBold,rect(4,26,30,26),kArcadeDark,StringAlignmentCenter);
            const auto saved = gr.Save();
            gr.TranslateTransform(4,154);
            gr.RotateTransform(-90);
            text(gr,tr(L"FRIENDS"),0,16,FontStyleBold,rect(0,0,102,30),kArcadeDark,StringAlignmentCenter);
            gr.Restore(saved);
            if (g.friends_reveal > 0) {
                fill_round(gr,rect(33,7,330,520),16,SolidBrush(Color(kArcadeDark)));
                fill_round(gr,rect(39,2,320,518),16,SolidBrush(Color(kArcadeCard)));
                stroke_round(gr,rect(39,2,320,518),16,kArcadeDark,3);
                Pen highlight(Color(0xFF473B91), 1.f);
                gr.DrawLine(&highlight, 57.f, 6.f, 346.f, 6.f);
                text(gr,tr(L"FRIENDS"),0,22,FontStyleBold,rect(50,14,145,30),kArcadeGold);
                std::size_t playing=0;
                for(const auto& f:g.friends) if(f.online) ++playing;
                fill_round(gr,rect(286,18,50,24),6,SolidBrush(Color(kArcadeLime)));
                text(gr,std::to_wstring(playing)+L"/"+std::to_wstring(g.friends.size()),1,12,FontStyleBold,rect(286,18,50,24),kArcadeDark,StringAlignmentCenter);
                const std::wstring service_label=tr(!g.cfg.online ? L"Online is off" :
                    g.host_busy ? L"Connecting..." : !g.host_status.running ? L"Online service not started" : L"Online");
                text(gr,service_label,1,11,FontStyleRegular,rect(50,47,290,22),kTextMuted);
                if (g.friends.empty()) {
                    text(gr,tr(L"No friends yet."),1,13,FontStyleRegular,rect(54,100,280,100),kTextMuted);
                }
                for (int row=0; row<4 && row+g.friends_scroll<static_cast<int>(g.friends.size()); ++row) {
                    const int index=row+g.friends_scroll;
                    const auto& f=g.friends[index];
                    const float y=78.f+row*42.f;
                    const float width=g.friends.size()>4 ? 272.f : 291.f;
                    fill_round(gr,rect(49,y,width,38),10,SolidBrush(Color(kArcadeBg)));
                    if(index==g.friends_selected) stroke_round(gr,rect(49,y,width,38),10,kArcadeGold,2);
                    const auto dot=f.in_game ? kArcadeLime : f.online ? kArcadeCyan : kTextOff;
                    fill_round(gr,rect(57,y+5,28,28),9,SolidBrush(Color(dot)));
                    const auto name=friend_label(f);
                    text(gr,name.substr(0,1),0,17,FontStyleBold,rect(57,y+5,28,28),kArcadeDark,StringAlignmentCenter);
                    text(gr,name,1,13,FontStyleBold,rect(96,y+1,215,20),kTextLight);
                    const std::wstring status=tr(f.in_game ? L"In game" : f.online ? L"Online (game closed)" :
                        f.name.empty() && g.host_status.running ? L"Offline \u00b7 must add your code" : L"Offline");
                    text(gr,status,1,10,FontStyleRegular,rect(96,y+20,215,16),f.in_game ? kArcadeLime : kTextMuted);
                }
                if(g.friends.size()>4) {
                    fill_round(gr,rect(327,76,14,168),7,SolidBrush(Color(kArcadeDark)));
                    fill_round(gr,lobby_scroll_thumb(),5,SolidBrush(Color(g.lobby_scroll_drag ? kArcadeGold : kArcadeCyan)));
                }
                text(gr,tr(L"ADD A FRIEND"),1,10,FontStyleBold,rect(52,307,288,16),kTextMuted);
                draw_field(gr,rect(50,324,290,32),g.friend_code_edit);
                text(gr,g.friends_notice,1,11,FontStyleRegular,rect(52,410,286,54),g.friends_notice_error ? kArcadePink : kArcadeLime);
            }
        }
        BitBlt(target,0,0,px(370),px(530),dc,0,0,SRCCOPY);
        SelectObject(dc,previous);
        DeleteObject(bitmap);
        DeleteDC(dc);
        EndPaint(window,&ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        const float x=static_cast<float>(static_cast<short>(LOWORD(lp)))/g.scale;
        const float y=static_cast<float>(static_cast<short>(HIWORD(lp)))/g.scale;
        if(g.friends_open && !g.friends_animating && x>=49 && x<(g.friends.size()>4 ? 321.f : 340.f) && y>=78 && y<246) {
            const int row=static_cast<int>((y-78.f)/42.f);
            const int index=row+g.friends_scroll;
            if(row<4 && std::fmod(y-78.f,42.f)<38.f && index<static_cast<int>(g.friends.size())) {
                g.friends_selected=index==g.friends_selected ? -1 : index;
                update_friend_controls();InvalidateRect(window,nullptr,FALSE);return 0;
            }
        }
        if(g.friends_open && !g.friends_animating && g.friends.size()>4 && x>=324 && x<=344 && y>=76 && y<=244) {
            const auto thumb=lobby_scroll_thumb();
            g.lobby_scroll_grab=y>=thumb.Y && y<=thumb.GetBottom() ? y-thumb.Y : thumb.Height/2;
            g.lobby_scroll_drag=true;SetCapture(window);SetFocus(window);
            drag_lobby_scroll(window,y);InvalidateRect(window,nullptr,FALSE);return 0;
        }
        break;
    }
    case WM_MOUSEMOVE:
        if(g.lobby_scroll_drag) {
            drag_lobby_scroll(window,static_cast<float>(static_cast<short>(HIWORD(lp)))/g.scale);return 0;
        }
        break;
    case WM_CAPTURECHANGED:
        g.lobby_scroll_drag=false;InvalidateRect(window,nullptr,FALSE);return 0;
    case WM_LBUTTONUP:
        if(g.lobby_scroll_drag){g.lobby_scroll_drag=false;ReleaseCapture();InvalidateRect(window,nullptr,FALSE);return 0;}
        if (static_cast<short>(LOWORD(lp)) < px(38)) {
            SetFocus(window);
            animate_friends(!g.friends_open);
        }
        return 0;
    case WM_KEYDOWN:
        if(g.friends_open && (wp==VK_UP || wp==VK_DOWN || wp==VK_HOME || wp==VK_END || wp==VK_PRIOR || wp==VK_NEXT)) {
            if(wp==VK_HOME)g.friends_scroll=0;
            else if(wp==VK_END)g.friends_scroll=static_cast<int>(g.friends.size());
            else g.friends_scroll+=wp==VK_UP ? -1 : wp==VK_DOWN ? 1 : wp==VK_PRIOR ? -4 : 4;
            reload_friends();InvalidateRect(window,nullptr,FALSE);return 0;
        }
        if (wp == VK_ESCAPE || wp == VK_SPACE || wp == VK_RETURN) {
            animate_friends(wp == VK_ESCAPE ? false : !g.friends_open);
            return 0;
        }
        break;
    case WM_MOUSEWHEEL:
        g.lobby_wheel_delta+=static_cast<short>(HIWORD(wp));
        g.friends_scroll-=g.lobby_wheel_delta/WHEEL_DELTA;
        g.lobby_wheel_delta%=WHEEL_DELTA;
        reload_friends(); InvalidateRect(window,nullptr,FALSE); return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}

// Settings tab scroll position (page coordinates), launcher_graphics.hpp
inline float settings_scroll = 0;
void validate();
#include "launcher_graphics.hpp"
#include "launcher_mods.hpp"
#include "launcher_trophies.hpp"
#include "classic_pages.hpp"
#include "nitro_pages.hpp"
#include "classic_controls.hpp"
#include "nitro_controls.hpp"
#include "nitro_transition.hpp"

// Artwork loading
std::unique_ptr<Bitmap> load_png(const fs::path& path) {
    std::unique_ptr<Bitmap> file(Bitmap::FromFile(path.c_str()));
    if (!file || file->GetLastStatus() != Ok) return nullptr;
    auto copy = std::make_unique<Bitmap>(static_cast<INT>(file->GetWidth()), static_cast<INT>(file->GetHeight()), PixelFormat32bppPARGB);
    Graphics gr(copy.get());
    gr.DrawImage(file.get(), 0, 0, static_cast<INT>(file->GetWidth()), static_cast<INT>(file->GetHeight()));
    return copy;
}
std::unique_ptr<Bitmap> load_launcher_logo() {
    if (auto logo = load_png(g.exe_dir / "assets/launcher/logo.png")) return logo;
    // Keep branding available even when only the executable is copied.
    const HMODULE module = GetModuleHandleW(nullptr);
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(101), RT_RCDATA);
    if (!resource) return load_png(g.root / "ctrport/assets/launcher/logo.png");
    const DWORD size = SizeofResource(module, resource);
    const void* data = LockResource(LoadResource(module, resource));
    if (!data || !size) return nullptr;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return nullptr;
    void* destination = GlobalLock(memory);
    if (!destination) { GlobalFree(memory); return nullptr; }
    std::memcpy(destination, data, size);
    GlobalUnlock(memory);
    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) { GlobalFree(memory); return nullptr; }
    std::unique_ptr<Bitmap> copy;
    {
        Bitmap source(stream);
        if (source.GetLastStatus() == Ok) {
            copy = std::make_unique<Bitmap>(INT(source.GetWidth()), INT(source.GetHeight()), PixelFormat32bppPARGB);
            Gdiplus::Status drawn;
            {
                Graphics graphics(copy.get());
                drawn = graphics.DrawImage(&source, 0, 0, INT(source.GetWidth()), INT(source.GetHeight()));
            }
            if (drawn != Ok) copy.reset();
        }
    }
    stream->Release();
    return copy;
}
void poll_fonts() {
    if (font_load.valid() && font_load.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        auto loaded = font_load.get();
        if (font_loading_key == font_requested_key) {
            retail_fonts = std::move(loaded);
            std::error_code ec;
            fs::create_directories(g.root / "runtime/user", ec);
            std::ofstream(g.root / "runtime/user/launcher_fonts.log") << retail_fonts.diagnostics;
            render_base_layer();
            RedrawWindow(g.window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
    }
    if (!font_load.valid() && font_loading_key != font_requested_key) {
        font_loading_key = font_requested_key;
        const auto sources = font_requested_sources;
        font_load = std::async(std::launch::async, [sources] { return ctrport::launcher_font::load(sources); });
    }
}
void request_fonts() {
    std::vector<fs::path> sources;
    std::wstring key;
    std::error_code ec;
    const auto base = resolve(g.cfg.base_dir), patch = resolve(g.cfg.patch_dir);
    for (const wchar_t* name : {L"update.pak", L"permanent.pak"}) {
        const auto relative = fs::path("archives") / name;
        fs::path source;
        if (g.cfg.use_patch && !g.cfg.patch_dir.empty() && fs::is_regular_file(patch / relative, ec)) source = patch / relative;
        else if (!g.cfg.base_dir.empty() && fs::is_regular_file(base / relative, ec)) source = base / relative;
        if (source.empty()) continue;
        sources.push_back(source);
        const auto stamp = fs::last_write_time(source, ec);
        key += source.wstring() + L"|" + std::to_wstring(stamp.time_since_epoch().count()) + L"\n";
    }
    if (key != font_requested_key) {
        font_requested_key = key;
        font_requested_sources = std::move(sources);
        retail_fonts = {};
        render_base_layer();
        RedrawWindow(g.window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    poll_fonts();
}
void refresh_artwork() {
    request_fonts();
    std::error_code ec;
    fs::path folder;
    for (const fs::path candidate : {g.cfg.use_patch ? resolve(g.cfg.patch_dir) : fs::path(), resolve(g.cfg.base_dir)})
        if (!candidate.empty() && fs::exists(candidate / "sce_sys/pic1.png", ec)) { folder = candidate; break; }
    if (g.art_checked && folder == g.art_source) return;
    g.art_checked = true;
    g.art_source = folder;
    g.art = folder.empty() ? nullptr : load_png(folder / "sce_sys/pic1.png");
    g.icon = folder.empty() ? nullptr : load_png(folder / "sce_sys/icon0.png");
    if (g.icon) {
        HICON icon = nullptr;
        if (g.icon->GetHICON(&icon) == Ok) {
            SendMessageW(g.window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
            SendMessageW(g.window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
            if (g.window_icon) DestroyIcon(g.window_icon);
            g.window_icon = icon;
        }
    }
    render_base_layer();
    RedrawWindow(g.window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
}

// Validation
bool parse_size(const std::wstring& value, int low, int high, int& out) {
    if (value.empty() || value.size() > 5 || !std::all_of(value.begin(), value.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; })) return false;
    out = std::stoi(value);
    return out >= low && out <= high;
}
void validate() {
    if (g.updating) return;
    std::error_code ec;
    g.cfg.base_dir = window_text(g.base_edit);
    g.cfg.patch_dir = window_text(g.patch_edit);
    const fs::path base = resolve(g.cfg.base_dir), patch = resolve(g.cfg.patch_dir);
    const bool base_eboot = !g.cfg.base_dir.empty() && fs::exists(base / "eboot.bin", ec);
    const bool base_modules = base_eboot && fs::exists(base / "sce_module", ec);
    const bool patch_ok = !g.cfg.patch_dir.empty() && fs::exists(patch / "eboot.bin", ec);
    g.base_status = !base_eboot ? Status{tr(L"eboot.bin not found in the base folder"), 2}
                  : !base_modules ? Status{tr(L"sce_module folder missing from the base game"), 2}
                                  : Status{tr(L"Base game found"), 1};
    if (!g.cfg.use_patch) g.patch_status = {tr(L"Update disabled: v01.00 will be started"), 0};
    else g.patch_status = patch_ok ? Status{tr(L"Title update found"), 1} : Status{tr(L"Update folder or eboot.bin missing"), 2};
    EnableWindow(g.patch_edit, g.cfg.use_patch);
    EnableWindow(g.patch_browse, g.cfg.use_patch);
    EnableWindow(g.fps_toggle, g.cfg.use_patch);
    // a package without the Direct3D 11 files (the step release that 0.6.1 updaters accept): Vulkan
    const bool d3d11_installed = fs::exists(g.runtime_exe.parent_path() / "ctr_d3d.dll", ec);
    if (!d3d11_installed && g.cfg.renderer == L"d3d11") {
        g.cfg.renderer = L"vulkan";
        if (g.renderer_combo) SendMessageW(g.renderer_combo, CB_SETCURSEL, 0, 0);
    }
    if (g.renderer_combo) EnableWindow(g.renderer_combo, d3d11_installed);
    for (HWND pill : g.pills) EnableWindow(pill, !g.cfg.fullscreen);
    EnableWindow(g.width_edit, !g.cfg.fullscreen && g.resolution == 4);
    EnableWindow(g.height_edit, !g.cfg.fullscreen && g.resolution == 4);
    int width = 0, height = 0;
    const bool size_ok = g.cfg.fullscreen || (parse_size(window_text(g.width_edit), 320, 7680, width) && parse_size(window_text(g.height_edit), 240, 4320, height));
    const bool runtime_ok = !g.root.empty() && fs::exists(g.runtime_exe, ec);
    const auto player_name=g.online_id_edit ? window_text(g.online_id_edit) : g.cfg.online_id;
    const bool name_ok=player_name.size()>=3 && player_name.size()<=16 &&
        player_name.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::wstring::npos;
    g.can_launch = runtime_ok && base_modules && (!g.cfg.use_patch || patch_ok) && size_ok && name_ok;
    if (!runtime_ok) g.global = {tr(L"Runtime not found: ctrport.exe and ctr_gpu.dll required"), 2};
    else if (!base_modules || (g.cfg.use_patch && !patch_ok)) g.global = {tr(L"Select valid dump folders to start the game"), 2};
    else if (!size_ok) g.global = {tr(L"Invalid resolution (320\u00d7240 to 7680\u00d74320)"), 2};
    else if (!name_ok) g.global = {tr(L"Player name: 3-16 letters, digits, hyphen or underscore"), 2};
    else {
        std::wstring summary = std::wstring(tr(L"Ready")) + L" \u00b7 ";
        summary += g.cfg.use_patch ? L"v01.21 (Nitro Tour)" : L"v01.00 (Classic)";
        if (g.cfg.use_patch && g.cfg.fps_60) summary += L" · 60 FPS";
        summary += g.cfg.fullscreen ? L" \u00b7 " + std::wstring(tr(L"Fullscreen")) : L" · " + std::to_wstring(width) + L"×" + std::to_wstring(height);
        if (g.cfg.renderer == L"d3d11") summary += L" · Direct3D 11";
        for (size_t i = 0; i < std::size(kUpscalers); ++i)
            if (g.cfg.upscaler != L"off" && g.cfg.upscaler == kUpscalers[i]) summary += std::wstring(L" · ") + kUpscalerLabels[i];
        for (size_t i = 0; i < std::size(kModes); ++i)
            if (g.cfg.upscaler != L"off" && g.cfg.upscaling_mode == kModes[i]) summary += std::wstring(L" ") + tr(kModeLabels[i]);
        if (g.cfg.online) {
            const std::wstring id = g.online_id_edit ? window_text(g.online_id_edit) : g.cfg.online_id;
            summary += L" · Online (" + (id.empty() ? L"CTRPlayer" : id) + L")";
        }
        g.global = {summary, 1};
    }
    EnableWindow(g.online_toggle,!g.process && !g.host_busy);
    EnableWindow(g.online_id_edit,!g.process);
    EnableWindow(g.launch, g.can_launch);
    for (const auto& row : graphics_section::rows) {  // 60 FPS mode: v01.21; upscaling mode: with a method
        if (row.control->key == "fps_60") EnableWindow(row.combo, g.cfg.use_patch);
        if (row.control->key == "upscaling_mode") EnableWindow(row.combo, g.cfg.upscaler != L"off");
    }
    graphics_section_apply_scroll();
    refresh_artwork();
    InvalidateRect(g.window, nullptr, FALSE);
}

void select_resolution(int index) {
    g.resolution = index;
    g.updating = true;
    if (index < 4) {
        SetWindowTextW(g.width_edit, std::to_wstring(kPresetSize[index][0]).c_str());
        SetWindowTextW(g.height_edit, std::to_wstring(kPresetSize[index][1]).c_str());
    } else {
        SetWindowTextW(g.width_edit, g.cfg.custom_w.c_str());
        SetWindowTextW(g.height_edit, g.cfg.custom_h.c_str());
    }
    g.updating = false;
    for (HWND pill : g.pills) InvalidateRect(pill, nullptr, FALSE);
    validate();
}

void select_tab(int tab) { if(classic_skin)select_tab_now(tab);else nitro_begin(tab); }

void select_tab_now(int tab) {
    if (tab < 0 || tab >= TAB_COUNT) return;
    if (tab != g.current_tab) settings_scroll = 0;
    if (g.friends_drawer && tab != g.current_tab) reset_friends();
    g.current_tab = tab;
    render_base_layer(true);
    if (g.friends_drawer) position_friends();

    // Show/hide child controls based on active tab
    for (const auto& pair : g.controls) {
        HWND hwnd = pair.first;
        const Control& ctrl = pair.second;
        if (ctrl.tab >= 0) {
            ShowWindow(hwnd, ctrl.tab == tab ? SW_SHOW : SW_HIDE);
        }
    }
    sync_mod_rows();  // hides the unused rows of the MODS page
    ShowWindow(g.base_edit, tab == TAB_ADVANCED ? SW_SHOW : SW_HIDE);
    ShowWindow(g.trophy_key_edit, tab == TAB_TROPHIES ? SW_SHOW : SW_HIDE);
    ShowWindow(g.patch_edit, tab == TAB_ADVANCED ? SW_SHOW : SW_HIDE);

    const bool show_settings = (tab == TAB_SETTINGS);
    ShowWindow(g.language_combo, show_settings ? SW_SHOW : SW_HIDE);
    for (HWND combo : g.player_combo) if (combo) ShowWindow(combo, show_settings ? SW_SHOW : SW_HIDE);
    // upscaling method and mode: in the GRAPHICS section (launcher_graphics.hpp)
    ShowWindow(g.scene_combo, SW_HIDE);
    ShowWindow(g.upscaler_combo, SW_HIDE);
    ShowWindow(g.renderer_combo, show_settings ? SW_SHOW : SW_HIDE);
    if (g.online_id_edit) ShowWindow(g.online_id_edit, show_settings ? SW_SHOW : SW_HIDE);

    const bool show_custom = (show_settings && g.resolution == 4 && !g.cfg.fullscreen);
    ShowWindow(g.width_edit, show_custom ? SW_SHOW : SW_HIDE);
    ShowWindow(g.height_edit, show_custom ? SW_SHOW : SW_HIDE);
    graphics_section_apply_scroll();

    for (const auto& entry : g.controls) InvalidateRect(entry.first, nullptr, FALSE);
    InvalidateRect(g.window, nullptr, FALSE);
}

void browse(HWND edit, const wchar_t* title) {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dialog->SetTitle(title);
    const std::wstring current = resolve(window_text(edit)).wstring();
    IShellItem* folder = nullptr;
    if (!current.empty() && SUCCEEDED(SHCreateItemFromParsingName(current.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
        dialog->SetFolder(folder);
        folder->Release();
    }
    if (SUCCEEDED(dialog->Show(g.window))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dialog->GetResult(&result))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                SetWindowTextW(edit, relative_to_root(path).c_str());
                CoTaskMemFree(path);
            }
            result->Release();
        }
    }
    dialog->Release();
}

void backup_save_data() {
    std::error_code ec;
    const fs::path saves = g.root / "runtime/user/savedata";
    if (!fs::exists(saves, ec)) {
        MessageBoxW(g.window, tr(L"No saves found."), kTitle, MB_ICONINFORMATION);
        return;
    }
    const std::wstring stamp = timestamp();
    const fs::path backup = g.root / "runtime/user/savedata_backups" / (L"manual_" + stamp);
    fs::create_directories(backup.parent_path(), ec);
    fs::copy(saves, backup, fs::copy_options::recursive, ec);
    if (ec) {
        MessageBoxW(g.window, (std::wstring(tr(L"Save backup failed:")) + L"\n" + widen(ec.message())).c_str(), kTitle, MB_ICONERROR);
    } else {
        MessageBoxW(g.window, (std::wstring(tr(L"Backup completed:")) + L"\n" + backup.wstring()).c_str(), kTitle, MB_ICONINFORMATION);
    }
}

// Hosting: the project server runs in this process while the launcher is open.
bool copy_to_clipboard(const std::wstring& value) {
    const auto bytes=(value.size()+1)*sizeof(wchar_t);
    HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory)return false;
    void* data=GlobalLock(memory);if(!data){GlobalFree(memory);return false;}
    std::memcpy(data,value.c_str(),bytes);GlobalUnlock(memory);
    if(!OpenClipboard(g.window)){GlobalFree(memory);return false;}
    const bool ok=EmptyClipboard() && SetClipboardData(CF_UNICODETEXT,memory);
    CloseClipboard();if(!ok)GlobalFree(memory);return ok;
}

void host_toggled(bool = false) {
    if(g.host_busy) return;
    g.host_busy=true;
    g.cfg.online_host=g.cfg.online;
    g.friends_notice.clear();
    EnableWindow(g.online_toggle,FALSE);
    std::thread([start=g.cfg.online,root=g.root,window=g.window,name=narrow(g.cfg.online_id)] {
        if(start) g.host.start(root,{},true,name); else g.host.stop();
        PostMessageW(window,WM_APP_HOST_READY,start ? 1 : 0,0);
    }).detach();
    validate();
}
void host_ready(bool started) {
    g.host_busy=false;EnableWindow(g.online_toggle,TRUE);
    g.host_status=g.host.status();
    if(started && !g.host_status.running) {
        g.friends_notice=widen(g.host_status.error);g.friends_notice_error=true;
        if(g.current_tab==TAB_HOME)animate_friends(true);
    }
    reload_friends();update_friend_controls();validate();InvalidateRect(g.friends_drawer,nullptr,FALSE);
}
// 110: copy the own friend code; 111: add the code typed in the drawer; 109: remove the selected friend.
void friend_action(int action) {
    g.friends_notice_error=false;
    if(action==110) {
        const auto own=g.host_status.friend_code.empty() ? ctr_online::local_friend_code(g.root) : g.host_status.friend_code;
        const bool copied=!own.empty() && copy_to_clipboard(widen(own));
        g.friends_notice=tr(copied ? L"Code copied. You must add each other." : L"Could not copy the code.");
        g.friends_notice_error=!copied;
    } else if(action==111) {
        auto code=narrow(window_text(g.friend_code_edit));
        code.erase(0,code.find_first_not_of(" \t\r\n"));code.erase(code.find_last_not_of(" \t\r\n")+1);
        ctr_online::FriendCodeInfo info;
        std::string error;
        if(ctr_online::parse_friend_code(code,info)!=ctr_online::CodeParseError::Ok) {
            g.friends_notice=tr(L"Invalid friend code.");g.friends_notice_error=true;
        } else if(info.code==ctr_online::local_friend_code(g.root)) {
            g.friends_notice=tr(L"This is your own code.");g.friends_notice_error=true;
        } else if(!ctr_online::FriendStore(friends_path()).add_or_update(info.code,"",&error)) {
            g.friends_notice=widen(error);g.friends_notice_error=true;
        } else {
            SetWindowTextW(g.friend_code_edit,L"");
            g.friends_notice=tr(L"Friend added.");
        }
    } else if(action==109 && g.friends_selected>=0 && g.friends_selected<static_cast<int>(g.friends.size())) {
        const auto row=g.friends[g.friends_selected];
        const auto question=trf(L"Remove {0} from friends?",friend_label(row));
        if(MessageBoxW(g.window,question.c_str(),kTitle,MB_YESNO|MB_ICONQUESTION)!=IDYES) return;
        ctr_online::FriendStore(friends_path()).remove(row.code);
        g.friends_selected=-1;
        g.friends_notice=trf(L"{0} removed.",friend_label(row));
    }
    reload_friends();update_friend_controls();InvalidateRect(g.friends_drawer,nullptr,FALSE);
}

// Launching ctrport.exe
std::vector<wchar_t> environment_block(const std::map<std::wstring, std::wstring>& set, const std::vector<std::wstring>& remove) {
    std::vector<std::wstring> entries;
    if (wchar_t* strings = GetEnvironmentStringsW()) {
        for (const wchar_t* p = strings; *p; p += wcslen(p) + 1) entries.emplace_back(p);
        FreeEnvironmentStringsW(strings);
    }
    const auto key = [](const std::wstring& entry) { return lower(entry.substr(0, entry.find(L'=', 1))); };
    std::vector<std::wstring> drop;
    for (const auto& name : remove) drop.push_back(lower(name));
    for (const auto& item : set) drop.push_back(lower(item.first));
    std::erase_if(entries, [&](const std::wstring& entry) { return std::find(drop.begin(), drop.end(), key(entry)) != drop.end(); });
    for (const auto& item : set) entries.push_back(item.first + L"=" + item.second);
    std::sort(entries.begin(), entries.end(), [&](const std::wstring& a, const std::wstring& b) { return key(a) < key(b); });
    std::vector<wchar_t> block;
    for (const auto& entry : entries) { block.insert(block.end(), entry.begin(), entry.end()); block.push_back(L'\0'); }
    block.push_back(L'\0');
    return block;
}

// --- MODS tab -------------------------------------------------------------------------------
// Loose-file mods in runtime/mods with runtime/user/mods.json, and patch XML files in
// runtime/patches with runtime/user/patches.json: the formats of SacredFire / GetLostIn
// CTR-NitroFueled-PC. The runtime reads the same files (mod_overlay.hpp, guest_patch_xml.hpp).
fs::path mods_dir() { return g.root / "runtime/mods"; }
fs::path mods_config_path() { return g.root / "runtime/user/mods.json"; }
fs::path patches_dir() { return g.root / "runtime/patches"; }
fs::path patches_config_path() { return g.root / "runtime/user/patches.json"; }
// What was installed from GameBanana, and the folder for downloads and extraction.
fs::path gamebanana_registry() { return g.root / "runtime/user/gamebanana.json"; }
fs::path gamebanana_work() { return g.root / "runtime/user/gamebanana"; }

void reload_mods() {
    namespace mods = ctrport::mods;
    namespace patches = ctrport::guest_patches;
    g.mods_notice.clear();
    g.mods_notice_error = false;
    g.mod_list.clear(); g.mod_details.clear(); g.mod_bad.clear();
    g.patch_list.clear(); g.patch_details.clear(); g.patch_warn.clear();
    try {
        g.mod_list = mods::list_mods(mods_dir(), mods::read_config(mods_config_path()));
    } catch (const std::exception& e) { g.mods_notice = widen(e.what()); g.mods_notice_error = true; }
    // What the runtime did with each mod at the last start (mods_status.json, mod_overlay.hpp):
    // not applied, or adapted from the other region of the game.
    std::map<std::wstring, std::wstring> skipped, adapted;
    try {
        const auto list = ctr_online::parse_json(mods::read_text(g.root / "runtime/user/logs/mods_status.json"));
        for (const auto& item : list.items) {
            const auto mod = widen(ctrport::gamebanana::text_of(item, "mod")), reason = widen(ctrport::gamebanana::text_of(item, "reason"));
            auto& into = ctrport::gamebanana::text_of(item, "state") == "adapted" ? adapted[mod] : skipped[mod];
            into += (into.empty() ? L"" : L"; ") + reason;
        }
    } catch (const std::exception&) {}
    for (const auto& mod : g.mod_list) {
        try {
            auto single = mod;
            single.enabled = true;
            const auto index = mods::build_index({single}, {});
            const auto inner = mods::content_root(mod.folder).lexically_relative(mod.folder).generic_wstring();
            g.mod_details.push_back(trf(L"{0} game files", std::to_wstring(index.files.size())) + (inner.empty() || inner == L"." ? L"" : L" \u00b7 " + inner + L"/"));
            g.mod_bad.push_back(index.files.empty());
            if (const auto root = mods::content_root(mod.folder); mods::octr_mod(root)) {
                // octr-mod: the runtime applies its manifest (octr_mods.hpp)
                const auto manifest = ctrport::gamebanana::read_manifest(root / L"mod.json");
                g.mod_details.back() = !manifest.problem.empty() ? widen(manifest.problem)
                    : trf(L"{0} asset changes", std::to_wstring(manifest.operations)) + L" \u00b7 " + trf(L"{0} patch files", std::to_wstring(manifest.patches)) +
                      (index.files.empty() ? L"" : L" \u00b7 " + trf(L"{0} game files", std::to_wstring(index.files.size())));
                g.mod_bad.back() = !manifest.problem.empty();
                if (const auto why = skipped.find(mod.name); why != skipped.end() && mod.enabled) {
                    g.mod_details.back() = trf(L"Not applied at the last start: {0}", why->second);
                    g.mod_bad.back() = true;
                } else if (const auto how = adapted.find(mod.name); how != adapted.end() && mod.enabled) {
                    g.mod_details.back() = trf(L"Adapted from the other region of the game: {0}", how->second);
                }
            } else if (index.files.empty()) {
                g.mod_details.back() = tr(L"No game files found");
            }
        } catch (const std::exception& e) {
            g.mod_details.push_back(widen(e.what()));
            g.mod_bad.push_back(true);
        }
    }
    try { g.gb_installed = ctrport::gamebanana::read_installed(gamebanana_registry()); }
    catch (const std::exception& e) { if (g.mods_notice.empty()) { g.mods_notice = widen(e.what()); g.mods_notice_error = true; } }
    // The game the patches are checked against: TITLE_ID and APP_VER of the eboot that will run.
    std::string title, version;
    g.mods_game.clear();
    try {
        const fs::path game = resolve(g.cfg.use_patch && !g.cfg.patch_dir.empty() ? g.cfg.patch_dir : g.cfg.base_dir);
        const auto sfo = patches::read_sfo(game / "sce_sys/param.sfo");
        if (sfo.count("TITLE_ID") && sfo.count("APP_VER")) {
            title = sfo.at("TITLE_ID");
            version = sfo.at("APP_VER");
            g.mods_game = widen(title + " " + version);
        }
    } catch (const std::exception&) {}
    try {
        g.patch_selection = patches::read_selection(patches_config_path());
        std::wstring unreadable;
        for (auto& file : patches::load_folder(patches_dir())) {
            if (!file.error.empty()) { unreadable += (unreadable.empty() ? L"" : L", ") + file.file; continue; }
            for (auto& patch : file.patches) {
                const auto why = title.empty() ? std::string("game version unknown") : patches::mismatch(patch, title, version);
                std::wstring detail = why.empty() ? std::wstring() : trf(L"Not for this game: {0}", widen(why)) + L"  \u00b7  ";
                if (!patch.author.empty()) detail += widen(patch.author) + L"  ·  ";
                if (!patch.app_ver.empty()) detail += L"AppVer " + widen(patch.app_ver) + L"  ·  ";
                detail += patch.file;
                g.patch_details.push_back(detail);
                g.patch_warn.push_back(!why.empty());
                g.patch_list.push_back(std::move(patch));
            }
        }
        if (!unreadable.empty() && g.mods_notice.empty()) {
            g.mods_notice = trf(L"Unreadable patch files: {0}", unreadable);
            g.mods_notice_error = true;
        }
    } catch (const std::exception& e) {
        if (g.mods_notice.empty()) { g.mods_notice = widen(e.what()); g.mods_notice_error = true; }
    }
}

// --- Controllers ------------------------------------------------------------------------------
// XInput slots (Xbox and compatible controllers) and the other HID gamepads, which the game
// reads through SDL in ctr_gpu.dll: DualShock 4, DualSense and others.
std::vector<App::PadOption> detect_pads() {
    std::vector<App::PadOption> found;
    using GetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    if (HMODULE xinput = LoadLibraryExW(L"xinput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
        if (auto get = reinterpret_cast<GetState>(GetProcAddress(xinput, "XInputGetState")))
            for (DWORD slot = 0; slot < 4; ++slot) {
                XINPUT_STATE state{};
                if (get(slot, &state) == ERROR_SUCCESS)
                    found.push_back({L"xinput:" + std::to_wstring(slot), trf(L"Xbox controller {0}", std::to_wstring(slot + 1))});
            }
        FreeLibrary(xinput);
    }
    GUID hid{};
    HidD_GetHidGuid(&hid);
    const HDEVINFO set = SetupDiGetClassDevsW(&hid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set != INVALID_HANDLE_VALUE) {
        std::map<std::wstring, int> seen;
        SP_DEVICE_INTERFACE_DATA item{sizeof(item)};
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(set, nullptr, &hid, i, &item); ++i) {
            DWORD size = 0;
            SetupDiGetDeviceInterfaceDetailW(set, &item, nullptr, 0, &size, nullptr);
            std::vector<unsigned char> buffer(size);
            auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
            detail->cbSize = sizeof(*detail);
            if (!size || !SetupDiGetDeviceInterfaceDetailW(set, &item, detail, size, nullptr, nullptr)) continue;
            const std::wstring path = detail->DevicePath;
            if (lower(path).find(L"ig_") != std::wstring::npos) continue;  // an XInput controller, listed above
            const HANDLE device = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (device == INVALID_HANDLE_VALUE) continue;
            HIDD_ATTRIBUTES attributes{sizeof(attributes)};
            PHIDP_PREPARSED_DATA preparsed = nullptr;
            HIDP_CAPS caps{};
            const bool ok = HidD_GetAttributes(device, &attributes) && HidD_GetPreparsedData(device, &preparsed) &&
                            HidP_GetCaps(preparsed, &caps) == HIDP_STATUS_SUCCESS;
            if (preparsed) HidD_FreePreparsedData(preparsed);
            wchar_t product[128]{};
            if (ok) HidD_GetProductString(device, product, sizeof(product));
            CloseHandle(device);
            if (!ok || caps.UsagePage != 1 || (caps.Usage != 4 && caps.Usage != 5)) continue;
            std::wstring label = product[0] ? product : L"Gamepad";
            if (attributes.VendorID == 0x054C) {
                if (attributes.ProductID == 0x0CE6) label = L"DualSense";
                else if (attributes.ProductID == 0x0DF2) label = L"DualSense Edge";
                else if (attributes.ProductID == 0x05C4 || attributes.ProductID == 0x09CC || attributes.ProductID == 0x0BA0) label = L"DualShock 4";
            }
            if (const int n = ++seen[label]; n > 1) label += L" (" + std::to_wstring(n) + L")";
            found.push_back({L"hid:" + path, label});
        }
        SetupDiDestroyDeviceInfoList(set);
    }
    return found;
}

// Fills the four player menus: Auto, None (players 2-4), Keyboard, then the controllers found.
// A saved controller that is not connected stays selectable as "not connected".
void refresh_pads() {
    if (!g.player_combo[0]) return;
    g.pads = detect_pads();
    for (int player = 0; player < 4; ++player) {
        HWND combo = g.player_combo[player];
        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        std::vector<std::wstring> values;
        const auto add = [&](const std::wstring& value, const std::wstring& label) {
            values.push_back(value);
            SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        };
        add(L"auto", tr(L"Auto"));
        if (player) add(L"none", tr(L"None"));
        add(L"keyboard", tr(L"Keyboard"));
        for (const auto& pad : g.pads) add(pad.value, pad.label);
        const auto& chosen = g.cfg.player_pad[player];
        auto at = std::find(values.begin(), values.end(), chosen);
        if (at == values.end()) {
            if (chosen.rfind(L"xinput:", 0) == 0 || chosen.rfind(L"hid:", 0) == 0) {
                add(chosen, tr(L"Not connected"));
                at = values.end() - 1;
            } else {
                at = values.begin();
            }
        }
        SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(at - values.begin()), 0);
        g.pad_values[player] = values;
    }
}

// --- TROPHIES tab -----------------------------------------------------------------------------
void load_trophy_wiki();
void reload_trophies() {
    g.trophy_set = {};
    g.trophy_unlocked.clear();
    g.trophy_icons.clear();
    g.trophy_notice.clear();
    g.trophy_notice_error = false;
    try {
        const int language = kGameLanguages[game_language_index(g.cfg.game_language)].system_id;
        g.trophy_set = ctr_trophy::load({g.cfg.use_patch ? resolve(g.cfg.patch_dir) : fs::path(), resolve(g.cfg.base_dir)},
                                        narrow(g.cfg.trophy_key), language);
        const fs::path user = g.root / "runtime/user";
        const auto file = ctr_trophy::unlock_file(user, g.trophy_set.np_comm_id);
        ctr_trophy::import_jsonl(user, file);
        g.trophy_unlocked = ctr_trophy::read_unlocks(file);
        for (const auto& t : g.trophy_set.trophies) {
            std::unique_ptr<Bitmap> image;
            const auto png = ctr_trophy::icon(g.trophy_set, t.id);
            if (!png.empty())
                if (IStream* stream = SHCreateMemStream(png.data(), static_cast<UINT>(png.size()))) {
                    image.reset(Bitmap::FromStream(stream));
                    stream->Release();
                    if (image && image->GetLastStatus() != Ok) image.reset();
                }
            g.trophy_icons.push_back(std::move(image));
        }
        if (!g.trophy_set.error.empty()) { g.trophy_notice = widen(g.trophy_set.error); g.trophy_notice_error = true; }
        // No key: the wiki's texts, from the local copy, refreshed at most once a week.
        g.trophy_wiki = false;
        if (!g.trophy_set.texts && !g.trophy_set.trophies.empty()) {
            std::int64_t fetched = 0;
            std::vector<ctr_trophy::wiki::Row> rows;
            try { rows = ctr_trophy::wiki::read_cache(ctr_trophy::wiki::cache_file(user), &fetched); } catch (const std::exception&) {}
            g.trophy_wiki = ctr_trophy::wiki::apply(g.trophy_set, rows);
            if (!g.trophy_wiki_tried && (!g.trophy_wiki || std::time(nullptr) - fetched > 7 * 24 * 3600)) load_trophy_wiki();
        }
    } catch (const std::exception&) {
        g.trophy_set = {};
    }
    if (g.trophy_hot >= static_cast<int>(g.trophy_set.trophies.size())) g.trophy_hot = -1;
}

// Downloads the wiki's trophy table in the background (WM_APP_TROPHY_WIKI), once per run.
void load_trophy_wiki() {
    if (g.trophy_wiki_loading) return;
    g.trophy_wiki_loading = g.trophy_wiki_tried = true;
    const HWND window = g.window;
    const fs::path file = ctr_trophy::wiki::cache_file(g.root / "runtime/user");
    std::thread([window, file] {
        auto* error = new std::wstring;
        try { ctr_trophy::wiki::write_cache(file, ctr_trophy::wiki::fetch()); }
        catch (const std::exception& e) { *error = widen(e.what()); }
        if (!PostMessageW(window, WM_APP_TROPHY_WIKI, 0, reinterpret_cast<LPARAM>(error))) delete error;
    }).detach();
}
void trophy_wiki_ready(std::wstring* raw_error) {
    std::unique_ptr<std::wstring> error(raw_error);
    g.trophy_wiki_loading = false;
    g.trophy_wiki_error = *error;
    reload_trophies();
    InvalidateRect(g.window, nullptr, FALSE);
}

// The trophy key shadPS4 keeps in %APPDATA%/shadPS4/keys.json (TrophyKeySet.ReleaseTrophyKey).
void import_trophy_key() {
    wchar_t appdata[MAX_PATH]{};
    std::string key;
    if (GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH)) {
        std::ifstream in(fs::path(appdata) / "shadPS4/keys.json", std::ios::binary);
        if (in) {
            std::stringstream buffer;
            buffer << in.rdbuf();
            try {
                const auto keys = ctr_online::parse_json(buffer.str().rfind("\xEF\xBB\xBF", 0) == 0 ? buffer.str().substr(3) : buffer.str());
                if (const auto* set = keys.find("TrophyKeySet"))
                    if (const auto* value = set->find("ReleaseTrophyKey")) key = value->text;
            } catch (const std::exception&) {}
        }
    }
    unsigned char check[16];
    if (!ctr_trophy::parse_key(key, check)) {
        g.trophy_notice = tr(L"No trophy key in shadPS4's keys.json.");
        g.trophy_notice_error = true;
        InvalidateRect(g.window, nullptr, FALSE);
        return;
    }
    SetWindowTextW(g.trophy_key_edit, widen(key).c_str());  // EN_CHANGE saves and reloads
}

// Saves the order and switches of the file mods, or the patch choices.
void save_mods_choice(int index, bool toggle, int move) {
    try {
        if (g.mods_view == 0) {
            if (index < 0 || index >= static_cast<int>(g.mod_list.size())) return;
            if (toggle) g.mod_list[index].enabled = !g.mod_list[index].enabled;
            const int other = index + move;
            if (move && other >= 0 && other < static_cast<int>(g.mod_list.size())) {
                std::swap(g.mod_list[index], g.mod_list[other]);
                std::swap(g.mod_details[index], g.mod_details[other]);
                const bool bad = g.mod_bad[index];
                g.mod_bad[index] = g.mod_bad[other];
                g.mod_bad[other] = bad;
            }
            ctrport::mods::write_config(mods_config_path(), ctrport::mods::config_of(g.mod_list));
        } else {
            if (index < 0 || index >= static_cast<int>(g.patch_list.size())) return;
            const auto& patch = g.patch_list[index];
            ctrport::guest_patches::choose(g.patch_selection, patch, !ctrport::guest_patches::selected(patch, g.patch_selection));
            ctrport::guest_patches::write_selection(patches_config_path(), g.patch_selection);
        }
    } catch (const std::exception& e) {
        g.mods_notice = widen(e.what());
        g.mods_notice_error = true;
    }
    sync_mod_rows();
}

// The list index (mod, patch or GameBanana entry) of the item a row slot shows, -1 for none.
int slot_index(int slot) {
    if (slot < 0 || slot >= kModRows || g.mod_slot_item[slot] < 0) return -1;
    const auto list = mods_list::items();
    return g.mod_slot_item[slot] < static_cast<int>(list.size()) ? list[g.mod_slot_item[slot]].index : -1;
}

// --- GameBanana and dropped mods (gamebanana.hpp) -----------------------------------------------
// Work for a background thread: the catalog, or one install (a GameBanana file or a dropped
// archive or folder). The result comes back to the window with WM_APP_GB_CATALOG / _DONE.
struct ModsJob {
    std::vector<ctrport::gamebanana::Entry> catalog;
    std::uint64_t id = 0;               // GameBanana id, 0 for a dropped file
    ctrport::gamebanana::File file;     // the GameBanana file
    fs::path source;                    // the dropped archive or folder
    std::wstring title;
    bool has_previous = false;
    ctrport::gamebanana::Installed previous, installed;
    std::wstring error;
};

void load_gamebanana(bool force) {
    if (g.gb_loading || (g.gb_loaded && !force)) return;
    g.gb_loading = true;
    g.gb_error.clear();
    const HWND window = g.window;
    std::thread([window] {
        auto* job = new ModsJob;
        try { job->catalog = ctrport::gamebanana::fetch_catalog(); }
        catch (const std::exception& e) { job->error = widen(e.what()); }
        if (!PostMessageW(window, WM_APP_GB_CATALOG, 0, reinterpret_cast<LPARAM>(job))) delete job;
    }).detach();
    sync_mod_rows();
}

void gamebanana_catalog_ready(ModsJob* job) {
    std::unique_ptr<ModsJob> owned(job);
    g.gb_loading = false;
    g.gb_loaded = job->error.empty();
    g.gb_error = job->error;
    if (job->error.empty()) g.gb_list = std::move(job->catalog);
    sync_mod_rows();
}

// Runs one install on a background thread: download (GameBanana), extraction, installation.
void start_mods_job(std::unique_ptr<ModsJob> job) {
    namespace gb = ctrport::gamebanana;
    g.mods_busy = true;
    g.mods_busy_id = job->id;
    g.mods_busy_percent = job->id ? 0 : -1;
    g.mods_busy_name = job->title;
    g.mods_notice.clear();
    g.mods_notice_error = false;
    const HWND window = g.window;
    const fs::path work = gamebanana_work(), mods = mods_dir(), patches = patches_dir();
    std::thread([window, work, mods, patches, job = job.release()] {
        const fs::path staging = work / (L"staging-" + std::to_wstring(GetCurrentThreadId()));
        fs::path archive;
        try {
            std::error_code ec;
            fs::remove_all(staging, ec);
            fs::create_directories(work);
            fs::path from = job->source;
            if (job->id) {
                archive = work / (std::to_wstring(job->file.id) + L"-" + gb::safe_name(gb::wide(job->file.name)));
                int shown = -1;
                gb::http_get(widen(job->file.url), &archive, [&](std::uint64_t received, std::uint64_t total) {
                    const int percent = total ? static_cast<int>(received * 100 / total) : 0;
                    if (percent != shown) PostMessageW(window, WM_APP_GB_PROGRESS, static_cast<WPARAM>(shown = percent), 0);
                });
                PostMessageW(window, WM_APP_GB_PROGRESS, static_cast<WPARAM>(-1), 0);
                if (!job->file.md5.empty() && gb::md5_hex(archive) != job->file.md5)
                    throw std::runtime_error("the downloaded file does not match its MD5 on GameBanana");
                from = archive;
            }
            if (!fs::is_directory(from)) {
                gb::extract(from, staging);
                from = staging;
            }
            const std::wstring prefix = job->id ? L"gb" + std::to_wstring(job->id) + L"-" : L"";
            job->installed = gb::install_extracted(from, mods, patches, prefix, job->title, job->has_previous ? &job->previous : nullptr);
            job->installed.file = job->id ? job->file.name : narrow(job->source.filename().wstring());
        } catch (const std::exception& e) {
            job->error = widen(e.what());
        }
        std::error_code ec;
        fs::remove_all(staging, ec);
        if (!archive.empty()) fs::remove(archive, ec);
        if (!PostMessageW(window, WM_APP_GB_DONE, 0, reinterpret_cast<LPARAM>(job))) delete job;
    }).detach();
    sync_mod_rows();
}

std::vector<fs::path> dropped_queue;  // dropped while another install runs
void install_dropped(const std::vector<fs::path>& paths);

void mods_install_done(ModsJob* job) {
    std::unique_ptr<ModsJob> owned(job);
    g.mods_busy = false;
    g.mods_busy_id = 0;
    g.mods_busy_percent = -1;
    std::wstring notice;
    bool error = !job->error.empty();
    if (error) {
        notice = trf(L"{0} was not installed: {1}", job->title, job->error);
    } else {
        if (job->id) {
            g.gb_installed[job->id] = job->installed;
            try { ctrport::gamebanana::write_installed(gamebanana_registry(), g.gb_installed); }
            catch (const std::exception& e) { notice = widen(e.what()); error = true; }
        }
        if (!error)
            notice = job->installed.patches.empty() ? trf(L"Installed: {0}", job->title)
                                                    : trf(L"Installed: {0}. Turn its patches on under PATCHES.", job->title);
    }
    reload_mods();
    g.mods_notice = notice;
    g.mods_notice_error = error;
    sync_mod_rows();
    if (error) dropped_queue.clear();  // the notice shows what failed
    else if (!dropped_queue.empty()) install_dropped({});
}

// INSTALL or REMOVE of a GameBanana row.
void gamebanana_get(int index) {
    namespace gb = ctrport::gamebanana;
    if (g.mods_busy || index < 0 || index >= static_cast<int>(g.gb_list.size())) return;
    const gb::Entry& entry = g.gb_list[index];
    const auto found = g.gb_installed.find(entry.id);
    if (found != g.gb_installed.end()) {
        if (MessageBoxW(g.window, trf(L"Remove {0}?", gb::wide(entry.name)).c_str(), kTitle, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        try {
            gb::remove_installed(found->second, mods_dir(), patches_dir());
            g.gb_installed.erase(found);
            gb::write_installed(gamebanana_registry(), g.gb_installed);
            reload_mods();
            g.mods_notice = trf(L"Removed: {0}", gb::wide(entry.name));
        } catch (const std::exception& e) {
            reload_mods();
            g.mods_notice = widen(e.what());
            g.mods_notice_error = true;
        }
        sync_mod_rows();
        return;
    }
    // Downloads with several files (variants): the user picks one.
    std::size_t chosen = 0;
    if (entry.files.size() > 1) {
        const HMENU menu = CreatePopupMenu();
        for (std::size_t i = 0; i < entry.files.size(); ++i)
            AppendMenuW(menu, MF_STRING, i + 1, (gb::wide(entry.files[i].name) + L"\t" + gb::size_text(entry.files[i].size)).c_str());
        POINT at{};
        GetCursorPos(&at);
        const int picked = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, at.x, at.y, 0, g.window, nullptr);
        DestroyMenu(menu);
        if (picked <= 0) return;
        chosen = static_cast<std::size_t>(picked - 1);
    }
    auto job = std::make_unique<ModsJob>();
    job->id = entry.id;
    job->file = entry.files[chosen];
    job->title = gb::wide(entry.name);
    start_mods_job(std::move(job));
}

// Archives (.zip, .7z, .rar) and folders dropped on the launcher, installed one after another.
void install_dropped(const std::vector<fs::path>& paths) {
    if (g.current_tab != TAB_MODS) { reload_mods(); load_gamebanana(false); select_tab(TAB_MODS); }
    if (g.mods_view != 0) { g.mods_view = 0; g.mods_scroll = 0; }
    for (const auto& path : paths) {
        std::error_code ec;
        const auto ext = ctrport::mods::fold(path.extension().wstring());
        if (fs::is_directory(path, ec) || ext == L".zip" || ext == L".7z" || ext == L".rar") dropped_queue.push_back(path);
        else { g.mods_notice = trf(L"Not a mod archive or folder: {0}", path.filename().wstring()); g.mods_notice_error = true; }
    }
    if (g.mods_busy || dropped_queue.empty()) { sync_mod_rows(); return; }
    auto job = std::make_unique<ModsJob>();
    job->source = dropped_queue.front();
    dropped_queue.erase(dropped_queue.begin());
    job->title = fs::is_directory(job->source) ? job->source.filename().wstring() : job->source.stem().wstring();
    start_mods_job(std::move(job));
}

// Checked again at launch: the runtime refuses the same problems, but here they get a dialog.
bool check_mods_before_launch(const fs::path& base, const fs::path& patch) {
    if (!g.cfg.mods_enabled) return true;
    try {
        ctrport::mods::build_index(ctrport::mods::list_mods(mods_dir(), ctrport::mods::read_config(mods_config_path())), {patch, base});
        return true;
    } catch (const std::exception& e) {
        MessageBoxW(g.window, (std::wstring(tr(L"A mod cannot be loaded:")) + L"\n" + widen(e.what())).c_str(),
                    kTitle, MB_ICONERROR);
        return false;
    }
}

// Redraws every text after a language change (labels are translated when drawn).
void refresh_language() {
    SetWindowTextW(update_button, tr(update_label.c_str()));
    if (g.friends_manage) {
        SetWindowTextW(g.friends_manage, tr(L"COPY MY CODE"));
        SetWindowTextW(g.friend_add, tr(L"ADD"));
        SetWindowTextW(g.friend_remove, tr(L"REMOVE"));
        SendMessageW(g.friend_code_edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(tr(L"Paste a friend's code")));
        InvalidateRect(g.friends_drawer, nullptr, TRUE);
    }
    reload_mods();
    validate();
    select_tab(g.current_tab);
}

void launch_game();

// The archives the runtime reads through app0_read_path: patch first, then base.
std::vector<fs::path> xbox_prompt_sources() {
    std::error_code ec;
    std::vector<fs::path> sources;
    const fs::path base = resolve(g.cfg.base_dir), patch = resolve(g.cfg.patch_dir);
    for (const wchar_t* name : {L"permanent.pak", L"languagestartup.pak", L"update.pak"}) {
        const fs::path relative = fs::path(L"archives") / name;
        if (g.cfg.use_patch && fs::is_regular_file(patch / relative, ec)) sources.push_back(patch / relative);
        else if (fs::is_regular_file(base / relative, ec)) sources.push_back(base / relative);
    }
    return sources;
}

// Builds the Xbox prompt archives from the user's dump without Python (first launch only),
// then resumes the launch. Releases ship the CC0 PNGs in assets/xbox-default.
void prepare_xbox_prompts() {
    std::error_code ec;
    fs::path assets = g.exe_dir / "assets/xbox-default";
    if (!fs::exists(assets / "T_X_A_Color.png", ec)) assets = g.root / "ctrport/assets/xbox-default";
    g.xbox_busy = true;
    InvalidateRect(g.launch, nullptr, FALSE);
    std::thread([sources = xbox_prompt_sources(), user = g.root / "runtime/user", assets, window = g.window] {
        auto* error = new std::wstring;
        try {
            fs::create_directories(user / "mods/xbox-default");
            for (const auto& source : sources) ctrport::xbox_prepare::prepare_cached(source, user, assets);
        } catch (const std::exception& e) {
            *error = widen(e.what());
        }
        PostMessageW(window, WM_APP_XBOX_READY, 0, reinterpret_cast<LPARAM>(error));
    }).detach();
}

void xbox_prompts_ready(std::wstring* raw_error) {
    const std::unique_ptr<std::wstring> error(raw_error);
    g.xbox_busy = false;
    InvalidateRect(g.launch, nullptr, FALSE);
    if (!error->empty()) {
        MessageBoxW(g.window, (tr(L"Xbox button icon preparation failed:") + std::wstring(L"\n") + *error + L"\n\n" +
                               tr(L"Turn off the Xbox icons in Settings to use the PS4 button icons.")).c_str(),
                    kTitle, MB_ICONERROR);
        return;
    }
    g.xbox_ready = true;
    launch_game();
}

// Translates the game's shaders from the user's dump (ctr_shaders.exe: GCN -> HLSL -> DXBC) into
// runtime/user/gpu_d3d/shaders.bin, then resumes the launch. The first build takes minutes; later
// ones (new shaders met at run time, a newer translator) reuse the compiled bytecode cache.
void prepare_d3d_shaders() {
    const fs::path user = g.root / "runtime/user/gpu_d3d", logs = g.root / "runtime/user/logs";
    std::error_code ec;
    fs::create_directories(logs, ec);
    std::wstring command = L"\"" + (g.runtime_exe.parent_path() / "ctr_shaders.exe").wstring() + L"\" --base \"" +
                           resolve(g.cfg.base_dir).wstring() + L"\" --user \"" + user.wstring() + L"\"";
    if (g.cfg.use_patch && !g.cfg.patch_dir.empty()) command += L" --patch \"" + resolve(g.cfg.patch_dir).wstring() + L"\"";
    g.shaders_busy = true;
    g.shaders_percent = 0;
    InvalidateRect(g.launch, nullptr, FALSE);
    InvalidateRect(g.window, nullptr, FALSE);
    std::thread([command, log = logs / "ctr_shaders.log", cwd = g.root, window = g.window]() mutable {
        auto* error = new std::wstring;
        DWORD code = 1;
        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE read = nullptr, write = nullptr;
        STARTUPINFOW si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        if (CreatePipe(&read, &write, &sa, 0) && SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0)) {
            si.hStdOutput = si.hStdError = write;
            if (CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi)) {
                CloseHandle(write);
                write = nullptr;
                std::ofstream out(log, std::ios::binary);
                std::string pending, last_error;
                char buffer[4096];
                DWORD n = 0;
                while (ReadFile(read, buffer, sizeof(buffer), &n, nullptr) && n) {
                    out.write(buffer, n);
                    out.flush();
                    pending.append(buffer, n);
                    for (size_t end; (end = pending.find('\n')) != std::string::npos;) {
                        std::string line = pending.substr(0, end);
                        pending.erase(0, end + 1);
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        unsigned done = 0, total = 0;
                        if (sscanf_s(line.c_str(), "progress %u %u", &done, &total) == 2 && total)
                            PostMessageW(window, WM_APP_SHADERS_PROGRESS, done * 100 / total, 0);
                        else if (line.rfind("error", 0) == 0)
                            last_error = line;
                    }
                }
                WaitForSingleObject(pi.hProcess, INFINITE);
                GetExitCodeProcess(pi.hProcess, &code);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                if (code) *error = widen(last_error.empty() ? "exit code " + std::to_string(code) : last_error);
            } else {
                *error = trf(L"ctr_shaders.exe did not start (error {0})", std::to_wstring(GetLastError()));
            }
        } else {
            *error = L"pipe creation failed";
        }
        if (write) CloseHandle(write);
        if (read) CloseHandle(read);
        PostMessageW(window, WM_APP_SHADERS_READY, code, reinterpret_cast<LPARAM>(error));
    }).detach();
}

void d3d_shaders_ready(DWORD code, std::wstring* raw_error) {
    const std::unique_ptr<std::wstring> error(raw_error);
    g.shaders_busy = false;
    InvalidateRect(g.launch, nullptr, FALSE);
    InvalidateRect(g.window, nullptr, FALSE);
    std::error_code ec;
    if (code || !fs::exists(g.root / "runtime/user/gpu_d3d/shaders.bin", ec)) {
        const std::wstring log = (g.root / "runtime/user/logs/ctr_shaders.log").wstring();
        MessageBoxW(g.window, trf(L"The Direct3D 11 shaders could not be built from your dump:\n{0}\n\nLog: {1}", *error, log).c_str(),
                    tr(L"Direct3D 11 unavailable"), MB_ICONERROR);
        return;
    }
    g.shaders_ready = true;
    launch_game();
}

// shaders.bin is rebuilt when ctr_d3d has met shaders it lacks, or the translator is newer
bool d3d_shaders_outdated(const fs::path& shaders) {
    std::error_code ec;
    const auto built = fs::last_write_time(shaders, ec);
    if (ec) return true;
    if (fs::last_write_time(g.runtime_exe.parent_path() / "ctr_shaders.exe", ec) > built && !ec) return true;
    for (const auto& item : fs::directory_iterator(g.root / "runtime/user/gpu_d3d/missing_shaders", ec))
        if (item.path().extension() == ".gnm" && item.last_write_time(ec) > built) return true;
    return false;
}

// Direct3D 11: the backend DLL, the shaders translated from this dump, and no PC setting that only
// the Vulkan renderer provides (with them the runtime would refuse to start).
bool d3d11_ready() {
    std::error_code ec;
    const fs::path runtime = g.runtime_exe.parent_path();
    if (!fs::exists(runtime / "ctr_d3d.dll", ec) || !fs::exists(runtime / "ctr_shaders.exe", ec)) {
        MessageBoxW(g.window, tr(L"Direct3D 11 needs ctr_d3d.dll and ctr_shaders.exe next to ctrport.exe."), tr(L"Direct3D 11 unavailable"), MB_ICONERROR);
        return false;
    }
    const fs::path shaders = g.root / "runtime/user/gpu_d3d/shaders.bin";
    if (g.shaders_ready) {
        g.shaders_ready = false;
    } else if (!fs::exists(shaders, ec)) {
        if (MessageBoxW(g.window, tr(L"Direct3D 11 first translates the game's shaders from your dump. This takes a few minutes and is done once. Translate them now?"),
                        tr(L"Direct3D 11"), MB_OKCANCEL | MB_ICONINFORMATION) == IDOK)
            prepare_d3d_shaders();
        return false;
    } else if (d3d_shaders_outdated(shaders)) {
        prepare_d3d_shaders();
        return false;
    }
    // the temporal upscalers need the GPU library's Vulkan integration
    if (g.cfg.upscaler == L"fsr3" || g.cfg.upscaler == L"dlss") {
        MessageBoxW(g.window, tr(L"With Direct3D 11, choose Scene upscaling Off or AMD FSR 1."), tr(L"Direct3D 11 unavailable"), MB_ICONERROR);
        return false;
    }
    return true;
}

void launch_game() {
    if (update_preparing || update_handoff) return;
    validate();
    if (!g.can_launch || g.process || g.xbox_busy || g.shaders_busy) return;
    if (g.mods_busy) {
        MessageBoxW(g.window, tr(L"Wait until the mod is installed."), kTitle, MB_ICONINFORMATION);
        return;
    }
    if (g.cfg.upscaler == L"dlss" &&
        (!fs::exists(g.runtime_exe.parent_path() / "ctr_dlss.dll") ||
         !fs::exists(g.runtime_exe.parent_path() / "nvngx_dlss.dll"))) {
        MessageBoxW(g.window, tr(L"DLSS needs ctr_dlss.dll and nvngx_dlss.dll next to ctrport.exe. Choose Off or AMD FSR 1."), tr(L"DLSS unavailable"), MB_ICONERROR);
        return;
    }
    if (g.cfg.renderer == L"d3d11" && !d3d11_ready()) return;
    if (g.cfg.xbox_prompts && !g.xbox_ready) { prepare_xbox_prompts(); return; }
    g.xbox_ready = false;
    if (!check_mods_before_launch(resolve(g.cfg.base_dir), g.cfg.use_patch ? resolve(g.cfg.patch_dir) : fs::path())) return;
    if (!save_config()) return;
    if (!g.lock.acquire(g.root, g.window)) return;
    std::error_code ec;
    const std::wstring stamp = timestamp();
    const fs::path logs = g.root / "runtime/user/logs";
    fs::create_directories(logs, ec);
    ctrport::bounded_log::prune(logs);
    g.log_stem = logs / (L"play_" + stamp);
    try { ctr_online::backup_savedata(g.root, "before_launch"); }
    catch (const std::exception& error) {
        MessageBoxW(g.window, (tr(L"Save backup failed; launch cancelled:") + std::wstring(L"\n") + widen(error.what())).c_str(), kTitle, MB_ICONERROR);
        g.lock.release();
        return;
    }
    const fs::path base = resolve(g.cfg.base_dir), patch = resolve(g.cfg.patch_dir);
    const fs::path eboot = (g.cfg.use_patch ? patch : base) / "eboot.bin";
    std::vector<fs::path> modules;
    for (const auto& entry : fs::directory_iterator(base / "sce_module", ec))
        if (lower(entry.path().extension().wstring()) == L".prx") modules.push_back(entry.path());
    std::sort(modules.begin(), modules.end(), [](const fs::path& a, const fs::path& b) { return lower(a.wstring()) < lower(b.wstring()); });
    std::wstring command = quote_argument(g.runtime_exe.wstring()) + L" --entry-after-libc-init-images " + quote_argument(eboot.wstring());
    for (const auto& module : modules) command += L" " + quote_argument(module.wstring());

    std::map<std::wstring, std::wstring> set{{L"CTR_GPU_LIBRARY", L"1"}, {L"CTR_TLS_PATCH", L"1"}, {L"CTR_GPU_LOG", L"error"},
                                             {L"CTR_NO_FRAME_OBSERVATION", L"1"}, {L"CTR_BASE_DIR", base.wstring()}};
    set[L"CTR_SYSTEM_LANGUAGE"] = std::to_wstring(kGameLanguages[game_language_index(g.cfg.game_language)].system_id);
    bool fixed_guest = false;
    std::wstring players;
    for (int player = 0; player < 4; ++player) {
        players += (player ? L"|" : L"") + g.cfg.player_pad[player];
        if (player && g.cfg.player_pad[player] != L"auto" && g.cfg.player_pad[player] != L"none") fixed_guest = true;
    }
    set[L"CTR_PAD_PLAYERS"] = players;
    set[L"CTR_SPLIT_SCREEN"] = g.cfg.split_screen || fixed_guest ? L"1" : L"0";
    set[L"CTR_XBOX_PROMPTS"] = g.cfg.xbox_prompts ? L"1" : L"0";
    set[L"CTR_WUMPA_MULTIPLIER"] = g.cfg.wumpa_x100 ? L"100" : L"1";
    set[L"CTR_ONLINE_ID"] = g.cfg.online_id; // Display name applies to offline sessions too.
    if (g.cfg.use_patch) set[L"CTR_PATCH_DIR"] = patch.wstring();
    if (g.cfg.use_patch && g.cfg.fps_60) set[L"CTR_60FPS_PATCH"] = L"1";
    set[L"CTR_NEO_MODE"] = L"1";
    set[L"CTR_SETTINGS_PATH"] = config_path().wstring();
    // CTR_SCENE_HEIGHT: set by the runtime from the upscaling mode and the output resolution
    set[L"CTR_UPSCALER"] = g.cfg.upscaler;
    if (g.cfg.renderer == L"d3d11") set[L"CTR_GPU_BACKEND"] = L"d3d11";
    set[L"CTR_GPU_FSR"] = L"0"; // Scene upscale replaces the final-frame FSR path.
    if (g.cfg.unlock_all) set[L"CTR_UNLOCK_ALL"] = L"1";
    if (!g.cfg.trophy_key.empty()) set[L"CTR_TROPHY_KEY"] = g.cfg.trophy_key;
    if (g.cfg.mods_enabled) {
        set[L"CTR_MODS_DIR"] = mods_dir().wstring();
        set[L"CTR_MODS_CONFIG"] = mods_config_path().wstring();
        set[L"CTR_PATCHES_DIR"] = patches_dir().wstring();
        set[L"CTR_PATCHES_CONFIG"] = patches_config_path().wstring();
    }
    if (g.cfg.fullscreen) set[L"CTR_GPU_FULLSCREEN"] = L"1";
    else { set[L"CTR_GPU_WIDTH"] = window_text(g.width_edit); set[L"CTR_GPU_HEIGHT"] = window_text(g.height_edit); }
    if (g.cfg.online) {
        if(g.host_busy || !g.host_status.running) {
            g.lock.release();g.friends_notice=tr(L"Wait for Online to connect.");
            g.friends_notice_error=true;animate_friends(true);return;
        }
        try {
            // The game always talks to this PC's own service, which reaches friends' services
            // and the public matchmaking by itself.
            const std::string server_ip="127.0.0.1";
            const auto verified=ctr_online::verify_candidate_endpoint(server_ip+":8780",g.host_status.friend_code);
            if(verified.status!=ctr_online::VerifyStatus::Verified) throw std::runtime_error("Local service identity not verified");
            const auto online_dir=g.root / "runtime/user/online";
            fs::create_directories(online_dir);
            const auto key_path=online_dir / "party_traffic_key.der";
            std::ofstream key(key_path,std::ios::binary|std::ios::trunc);
            key.write(reinterpret_cast<const char*>(verified.traffic_key.data()),static_cast<std::streamsize>(verified.traffic_key.size()));
            key.close();if(!key) throw std::runtime_error("Could not save the lobby key");
            set[L"CTR_ONLINE"]=L"1";
            set[L"CTR_ONLINE_SERVER"]=widen(server_ip);
            set[L"CTR_ONLINE_TRAFFIC_KEY"]=key_path.wstring();
            set[L"CTR_ONLINE_PORT_OFFSET"]=std::to_wstring(ctr_online::kHostGamePortOffset);
            set[L"CTR_ONLINE_ACCOUNT"]=std::to_wstring(ctr_online::player_account(g.host_status.friend_code,narrow(g.cfg.online_id)));
        } catch(const std::exception& e) {
            g.lock.release();g.friends_notice=widen(e.what());g.friends_notice_error=true;
            animate_friends(true);return;
        }
    }
    auto environment = environment_block(set, {L"CTR_STARTUP_WRITE_DLL_DIR", L"CTR_STARTUP_MUTEX_DLL_DIR", L"CTR_PARK_MISSING_NID",
        L"CTR_PARK_MISSING_SERVICE", L"CTR_TILING_TIMING", L"CTR_60FPS_PATCH", L"CTR_NEO_MODE", L"CTR_NATIVE_4K", L"CTR_UNLOCK_ALL",
        L"CTR_SETTINGS_PATH", L"CTR_SCENE_HEIGHT", L"CTR_GPU_INTERNAL_WIDTH", L"CTR_GPU_INTERNAL_HEIGHT",
        L"CTR_GPU_FULLSCREEN", L"CTR_GPU_WIDTH", L"CTR_GPU_HEIGHT", L"CTR_PATCH_DIR", L"CTR_VERSION", L"CTR_GPU_BACKEND",
        L"CTR_ONLINE", L"CTR_ONLINE_ID", L"CTR_ONLINE_SERVER", L"CTR_ONLINE_TRAFFIC_KEY", L"CTR_ONLINE_PASSWORD", L"CTR_ONLINE_ROOM_TOKEN",
        L"CTR_ONLINE_PORT_OFFSET", L"CTR_ONLINE_NAT_TRANSPORT", L"CTR_ONLINE_RELAY_TEST_BLOCK_DIRECT",
        L"CTR_MODS_DIR", L"CTR_MODS_CONFIG", L"CTR_PATCHES_DIR", L"CTR_PATCHES_CONFIG", L"CTR_ONLINE_ACCOUNT", L"CTR_TROPHY_KEY", L"CTR_PAD_PLAYERS"});

    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    // Bounded capture (bounded_log.hpp): the game writes into pipes, not into the files.
    auto out_log = std::make_shared<ctrport::bounded_log::Capture>(g.log_stem.wstring() + L".stdout.log");
    auto err_log = std::make_shared<ctrport::bounded_log::Capture>(g.log_stem.wstring() + L".stderr.log");
    const HANDLE out = out_log->valid() ? out_log->write() : INVALID_HANDLE_VALUE;
    const HANDLE err = err_log->valid() ? err_log->write() : INVALID_HANDLE_VALUE;
    const HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup{sizeof(startup)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nul;
    startup.hStdOutput = out;
    startup.hStdError = err;
    PROCESS_INFORMATION info{};
    const BOOL started = out != INVALID_HANDLE_VALUE && err != INVALID_HANDLE_VALUE &&
        AllowSetForegroundWindow(ASFW_ANY);  // the game window comes in front of the launcher
        CreateProcessW(g.runtime_exe.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                       environment.data(), g.root.c_str(), &startup, &info);
    const DWORD error = GetLastError();
    out_log->close_writer(); err_log->close_writer();
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!started) {
        g.lock.release();
        MessageBoxW(g.window, trf(L"Unable to start ctrport.exe (error {0}).", std::to_wstring(error)).c_str(), kTitle, MB_ICONERROR);
        return;
    }
    CloseHandle(info.hThread);
    out_log->start(); err_log->start();
    g.process = info.hProcess;
    InvalidateRect(g.launch, nullptr, FALSE);
    ShowWindow(g.window, SW_HIDE);
    std::thread([process = info.hProcess, window = g.window, root = g.root, out_log, err_log] {
        WaitForSingleObject(process, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        // The crash report reads the log tails: let both captures finish writing first.
        out_log->wait(10000); err_log->wait(10000);
        try {
            ctr_online::backup_savedata(root, "after_exit");
        } catch (const std::exception& error) {
            MessageBoxW(window, (tr(L"Local backup incomplete:") + std::wstring(L"\n") + widen(error.what())).c_str(), kTitle, MB_ICONWARNING);
        }
        PostMessageW(window, WM_APP_FINISHED, code, 0);
    }).detach();
}

void game_finished(DWORD code) {
    CloseHandle(g.process);
    g.process = nullptr;
    g.lock.release();
    load_config();
    reload_mods();  // mods the runtime did not apply are marked
    sync_mod_rows();
    for (int i=0; i<static_cast<int>(std::size(kModes)); ++i)
        if(g.cfg.upscaling_mode==kModes[i]) SendMessageW(g.scene_combo,CB_SETCURSEL,i,0);
    for (int i=0; i<static_cast<int>(std::size(kUpscalers)); ++i)
        if(g.cfg.upscaler==kUpscalers[i]) SendMessageW(g.upscaler_combo,CB_SETCURSEL,i,0);
    for (int i=0; i<static_cast<int>(std::size(kRenderers)); ++i)
        if(g.cfg.renderer==kRenderers[i]) SendMessageW(g.renderer_combo,CB_SETCURSEL,i,0);
    SendMessageW(g.language_combo,CB_SETCURSEL,game_language_index(g.cfg.game_language),0);
    InvalidateRect(g.window, nullptr, FALSE);
    std::vector<std::string> reasons;
    for (const wchar_t* suffix : {L".stdout.log", L".stderr.log"}) {
        std::istringstream lines(read_tail(g.log_stem.wstring() + suffix, 8u << 20));
        std::vector<std::string> all;
        for (std::string line; std::getline(lines, line);) all.push_back(line);
        for (std::size_t i = all.size() > 4000 ? all.size() - 4000 : 0; i < all.size(); ++i) {
            const std::string& line = all[i];
            const bool reason = line.find("startup_libc_dependency_boundary") != std::string::npos || line.find("unresolved_access") != std::string::npos ||
                                line.find("unsupported:") != std::string::npos || line.find("STOP:") != std::string::npos || line.find("window closed") != std::string::npos;
            const bool ignored = line.find("obuxdTiwkF8") != std::string::npos || line.find("PlayGo") != std::string::npos || line.find("fZo48un7LK4") != std::string::npos;
            if (reason && !ignored) reasons.push_back(line.substr(0, 300));
        }
    }
    ShowWindow(g.window, SW_SHOW);
    SetForegroundWindow(g.window);
    InvalidateRect(g.launch, nullptr, FALSE);
    const bool closed_by_user = !reasons.empty() && reasons.back().find("window closed") != std::string::npos;
    if (code == 0 && (reasons.empty() || closed_by_user)) return;
    std::wstring message = trf(L"The game stopped (exit code {0}).", std::to_wstring(code)) + L"\n\n";
    if (reasons.empty()) message += tr(L"No specific error found in the logs.") + std::wstring(L"\n");
    for (std::size_t i = reasons.size() > 5 ? reasons.size() - 5 : 0; i < reasons.size(); ++i) message += widen(reasons[i]) + L"\n";
    message += L"\nLog: " + g.log_stem.wstring() + L".stdout.log";
    MessageBoxW(g.window, message.c_str(), kTitle, MB_ICONWARNING);
}

// Controls
LRESULT CALLBACK control_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    switch (message) {
    case WM_MOUSEMOVE:
        if (!IsWindowEnabled(window)) break;
        if (g.hot != window) {
            const HWND old = g.hot;
            g.hot = window;
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
            TrackMouseEvent(&track);
            if (old) InvalidateRect(old, nullptr, FALSE);
            InvalidateRect(window, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        if (g.hot == window) { g.hot = nullptr; InvalidateRect(window, nullptr, FALSE); }
        break;
    case WM_SETCURSOR:
        if (IsWindowEnabled(window)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

HWND add_button(int id, Kind kind, const RectF& box, const wchar_t* label, bool* value = nullptr, int pill = -1, int tab = -1) {
    HWND button = CreateWindowExW(0, L"BUTTON", tr(label), WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, px(box.X), px(box.Y),
                                  px(box.Width), px(box.Height), g.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SetWindowSubclass(button, control_proc, 1, 0);
    g.controls[button] = Control{kind, label, value, pill, box, tab};
    return button;
}
HWND add_edit(int id, const RectF& field, const std::wstring& value, DWORD extra = 0) {
    TEXTMETRICW metrics{};
    HDC dc = GetDC(g.window);
    const HGDIOBJ previous = SelectObject(dc, g.edit_font);
    GetTextMetricsW(dc, &metrics);
    SelectObject(dc, previous);
    ReleaseDC(g.window, dc);
    const int height = metrics.tmHeight + 2;
    HWND edit = CreateWindowExW(0, L"EDIT", value.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | extra,
                                px(field.X + 10), px(field.Y) + (px(field.Height) - height) / 2, px(field.Width - 20), height,
                                g.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(g.edit_font), FALSE);
    SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    return edit;
}

#include "classic_create.hpp"
#include "nitro_create.hpp"
#include "launcher_skin_switch.hpp"

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_MOUSEWHEEL:
        if (graphics_section_wheel(GET_WHEEL_DELTA_WPARAM(wparam))) return 0;
        if (mods_list_wheel(GET_WHEEL_DELTA_WPARAM(wparam))) return 0;
        break;
    case WM_DROPFILES: {
        // Archives and folders dropped anywhere on the launcher are installed as mods.
        const HDROP drop = reinterpret_cast<HDROP>(wparam);
        std::vector<fs::path> paths;
        for (UINT i = 0, n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0); i < n; ++i) {
            std::wstring path(DragQueryFileW(drop, i, nullptr, 0) + 1, L'\0');
            path.resize(DragQueryFileW(drop, i, path.data(), static_cast<UINT>(path.size())));
            paths.push_back(path);
        }
        DragFinish(drop);
        install_dropped(paths);
        return 0;
    }
    case WM_CREATE: {
        g.window = window;
        g.field_brush = CreateSolidBrush(RGB(16, 21, 44));
        g.field_off_brush = CreateSolidBrush(RGB(13, 10, 34));
        render_base_layer();
        g.updating = true;
        create_controls();
        DragAcceptFiles(window, TRUE);  // mods dropped on the launcher (WM_DROPFILES)
        for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
            SetWindowLongPtrW(child,GWL_STYLE,GetWindowLongPtrW(child,GWL_STYLE)|WS_CLIPSIBLINGS);
        g.updating = false;
        WNDCLASSW friends_class{};
        friends_class.lpfnWndProc = friends_drawer_proc;
        friends_class.hInstance = GetModuleHandleW(nullptr);
        friends_class.lpszClassName = L"CTRFriendsDrawer";
        friends_class.hCursor = LoadCursorW(nullptr, IDC_HAND);
        RegisterClassW(&friends_class);
        g.friends_drawer = CreateWindowExW(WS_EX_CONTROLPARENT, friends_class.lpszClassName, L"Friends",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,0,0,0,0,window,nullptr,friends_class.hInstance,nullptr);
        SetTimer(window,20,1000,nullptr); // friends' presence, fonts
        reload_friends();
        position_friends();
        select_tab(TAB_HOME);
        select_resolution(g.resolution);
        if (g.cfg.online) host_toggled(true);
        SetFocus(IsWindowEnabled(g.launch) ? g.launch : g.window);
        SendMessageW(window,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);
        SetTimer(window, 31, 250, nullptr);
        if (ctr_update::portable(g.root, g.exe_dir)) check_launcher_update(false);
        return 0;
    }
    case WM_DEVICECHANGE:
        if (wparam == 0x0007 /* DBT_DEVNODES_CHANGED */) refresh_pads();
        break;
    case WM_MOUSEMOVE:
        if (g.current_tab == TAB_TROPHIES) {
            const float mouse_x=static_cast<short>(LOWORD(lparam))/g.scale, mouse_y=static_cast<short>(HIWORD(lparam))/g.scale;
            const float x=(classic_skin?mouse_x:(mouse_x-40)/1.73f+296)-kTrophyLeft;
            const float y=mouse_y-(classic_skin?0.f:40.f)-kTrophyTop;
            int hot = -1;
            if (x >= 0 && y >= 0 && std::fmod(x, kTrophyCell) < kTrophyIcon/(classic_skin?1.f:1.73f) && std::fmod(y, kTrophyCell) < kTrophyIcon) {
                const int index = static_cast<int>(y / kTrophyCell) * kTrophyColumns + static_cast<int>(x / kTrophyCell);
                if (static_cast<int>(x / kTrophyCell) < kTrophyColumns && index < static_cast<int>(g.trophy_set.trophies.size())) hot = index;
            }
            if (hot >= 0 && hot != g.trophy_hot) { g.trophy_hot = hot; InvalidateRect(window, nullptr, FALSE); }
        }
        break;
    case WM_NCHITTEST: {
        POINT pt{static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam))};
        ScreenToClient(window, &pt);
        if (pt.y < px(24) || (!classic_skin && pt.y<px(nitro::header) && pt.x<px(215))) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        const bool indicator=nitro_animating && paint.rcPaint.top>=px(51) && paint.rcPaint.bottom<=px(nitro::header);
        const bool drawer=!classic_skin && g.current_tab==TAB_HOME && paint.rcPaint.left>=px(kWidth-kFriendsTravel)
            && paint.rcPaint.top>=px(100) && paint.rcPaint.bottom<=px(630);
        if(nitro_finishing && nitro_to_dc){BitBlt(dc,0,0,px(kWidth),px(kHeight),nitro_to_dc,0,0,SRCCOPY);}
        else if(indicator||drawer){
            HDC source=CreateCompatibleDC(dc);const auto old=SelectObject(source,g.base_layer);
            const auto& r=paint.rcPaint;BitBlt(dc,r.left,r.top,r.right-r.left,r.bottom-r.top,source,r.left,r.top,SRCCOPY);
            SelectObject(source,old);DeleteDC(source);
            if(indicator){Graphics gr(dc);gr.ScaleTransform(g.scale,g.scale);gr.SetSmoothingMode(SmoothingModeAntiAlias);
                const float n=nitro_nav_from+(nitro_nav_to-nitro_nav_from)*nitro_progress();
                fill_round(gr,rect(nitro::nav_center(n)-33,53,66,5),2.5f,SolidBrush(Color(kArcadeGold)));}
        } else if(nitro_preparing && nitro_page_background && nitro_page_background_tab==g.current_tab){
            HDC cache=CreateCompatibleDC(dc);const auto old=SelectObject(cache,nitro_page_background);
            BitBlt(dc,0,0,px(kWidth),px(kHeight),cache,0,0,SRCCOPY);SelectObject(cache,old);DeleteDC(cache);
        } else paint_window(dc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_DRAWITEM:
        draw_control(*reinterpret_cast<const DRAWITEMSTRUCT*>(lparam));
        return TRUE;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        const bool enabled = IsWindowEnabled(reinterpret_cast<HWND>(lparam)) != FALSE;
        HDC dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, enabled ? RGB(255, 248, 236) : RGB(185, 176, 240));
        SetBkColor(dc, enabled ? RGB(16, 21, 44) : RGB(13, 10, 34));
        return reinterpret_cast<LRESULT>(enabled ? g.field_brush : g.field_off_brush);
    }
    case WM_COMMAND: {
        const int id = LOWORD(wparam), code = HIWORD(wparam);
        if(g.updating)return 0;
        if(id==ID_LAUNCHER_SKIN && code==BN_CLICKED){switch_launcher_skin();return 0;}
        if(graphics_section_command(id,code))return 0;
        if(id==ID_NITRO_FRIENDS) {nitro_finish();if(g.current_tab!=TAB_HOME)select_tab_now(TAB_HOME);nitro_nav_from=nitro_nav_to=0;animate_friends(!g.friends_open);return 0;}
        if(id==ID_NITRO_MOTION) {nitro_motion=!nitro_motion;nitro_finish();if(!save_launcher_appearance())nitro_motion=!nitro_motion;InvalidateRect(reinterpret_cast<HWND>(lparam),nullptr,FALSE);return 0;}
        if (id == ID_CHECK_UPDATE) {
            if (update_offer.is_object()) offer_launcher_update(); else check_launcher_update(true);
            return 0;
        }
        if(id==IDOK && GetFocus()==g.friend_code_edit){friend_action(111);return 0;}
        if(id==IDCANCEL && g.friends_open){animate_friends(false);return 0;}
        if (id == IDOK || (id == ID_LAUNCH && (code == BN_CLICKED || code == BN_DOUBLECLICKED))) { launch_game(); return 0; }
        if (id == ID_SCENE_RES && code == CBN_SELCHANGE) {
            const auto selected = SendMessageW(g.scene_combo, CB_GETCURSEL, 0, 0);
            if (selected >= 0 && selected < static_cast<LRESULT>(std::size(kModes))) g.cfg.upscaling_mode = kModes[selected];
            validate();
            return 0;
        }
        if (id == ID_UPSCALER && code == CBN_SELCHANGE) {
            const auto selected = SendMessageW(g.upscaler_combo, CB_GETCURSEL, 0, 0);
            if (selected >= 0 && selected < static_cast<LRESULT>(std::size(kUpscalers))) g.cfg.upscaler = kUpscalers[selected];
            validate();
            return 0;
        }
        if (id == ID_RENDERER && code == CBN_SELCHANGE) {
            const auto selected = SendMessageW(g.renderer_combo, CB_GETCURSEL, 0, 0);
            if (selected >= 0 && selected < static_cast<LRESULT>(std::size(kRenderers))) g.cfg.renderer = kRenderers[selected];
            graphics_section_refresh();  // the choices this renderer provides
            validate();
            InvalidateRect(g.window, nullptr, FALSE);
            return 0;
        }
        if (id >= ID_PLAYER_PAD && id < ID_PLAYER_PAD + 4 && code == CBN_SELCHANGE) {
            const int player = id - ID_PLAYER_PAD;
            const auto selected = SendMessageW(g.player_combo[player], CB_GETCURSEL, 0, 0);
            if (selected >= 0 && static_cast<std::size_t>(selected) < g.pad_values[player].size()) {
                const auto value = g.pad_values[player][static_cast<std::size_t>(selected)];
                // A controller belongs to one player: it leaves the player that had it.
                if (value != L"auto" && value != L"none")
                    for (int other = 0; other < 4; ++other)
                        if (other != player && g.cfg.player_pad[other] == value) g.cfg.player_pad[other] = L"auto";
                g.cfg.player_pad[player] = value;
                save_config();
                refresh_pads();
            }
            return 0;
        }
        if (id == ID_GAME_LANGUAGE && code == CBN_SELCHANGE) {
            const auto selected = SendMessageW(g.language_combo, CB_GETCURSEL, 0, 0);
            if (selected >= 0 && selected < static_cast<LRESULT>(std::size(kGameLanguages)))
                g.cfg.game_language = kGameLanguages[selected].code;
            launcher_text::set_language(g.cfg.game_language);
            refresh_language();
            return 0;
        }
        if (id == ID_TROPHY_KEY_EDIT && code == EN_CHANGE) {
            g.cfg.trophy_key = window_text(g.trophy_key_edit);
            unsigned char check[16];
            if (g.cfg.trophy_key.empty() || ctr_trophy::parse_key(narrow(g.cfg.trophy_key), check)) {
                save_config();
                reload_trophies();
            }
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        if (code == EN_CHANGE) {
            if (!g.updating && g.resolution == 4 && (id == ID_WIDTH || id == ID_HEIGHT)) {
                g.cfg.custom_w = window_text(g.width_edit);
                g.cfg.custom_h = window_text(g.height_edit);
            }
            if (!g.updating && id == ID_ONLINE_ID_EDIT && g.online_id_edit) {
                g.cfg.online_id = window_text(g.online_id_edit);
            }
            validate();
            return 0;
        }
        if (code == EN_SETFOCUS || code == EN_KILLFOCUS) { InvalidateRect(window, nullptr, FALSE); return 0; }
        if (code != BN_CLICKED && code != BN_DOUBLECLICKED) break;

        if (id == ID_MINIMIZE) { ShowWindow(window, SW_MINIMIZE); return 0; }
        if (id == ID_CLOSE) { SendMessageW(window, WM_CLOSE, 0, 0); return 0; }

        // Tab selection
        if (id >= ID_TAB_BASE && id < ID_TAB_BASE + static_cast<int>(TAB_COUNT)) {
            const int tab = id - ID_TAB_BASE;
            if (tab == TAB_MODS) { reload_mods(); load_gamebanana(false); }  // folders may have changed outside the launcher
            if (tab == TAB_TROPHIES) reload_trophies();  // the game may have unlocked trophies
            if (tab == TAB_SETTINGS) refresh_pads();
            select_tab(tab);
            return 0;
        }

        // MODS tab: row buttons act on the list item their slot shows
        if (id >= ID_MOD_ROW_TOGGLE && id < ID_MOD_ROW_TOGGLE + kModRows) {
            save_mods_choice(slot_index(id - ID_MOD_ROW_TOGGLE), true, 0);
            return 0;
        }
        if (id >= ID_MOD_ROW_UP && id < ID_MOD_ROW_UP + kModRows) {
            save_mods_choice(slot_index(id - ID_MOD_ROW_UP), false, -1);
            return 0;
        }
        if (id >= ID_MOD_ROW_DOWN && id < ID_MOD_ROW_DOWN + kModRows) {
            save_mods_choice(slot_index(id - ID_MOD_ROW_DOWN), false, 1);
            return 0;
        }
        if (id >= ID_GB_ROW_PAGE && id < ID_GB_ROW_PAGE + kModRows) {
            const int index = slot_index(id - ID_GB_ROW_PAGE);
            if (index >= 0 && index < static_cast<int>(g.gb_list.size()) && g.gb_list[index].profile.rfind("https://gamebanana.com/", 0) == 0)
                ShellExecuteW(nullptr, L"open", widen(g.gb_list[index].profile).c_str(), nullptr, nullptr, SW_SHOW);
            return 0;
        }
        if (id >= ID_GB_ROW_GET && id < ID_GB_ROW_GET + kModRows) {
            gamebanana_get(slot_index(id - ID_GB_ROW_GET));
            return 0;
        }
        if (id == ID_MODS_FILES || id == ID_MODS_PATCHES) {
            g.mods_view = id == ID_MODS_PATCHES;
            g.mods_scroll = 0;
            sync_mod_rows();
            return 0;
        }
        if (id == ID_MODS_REFRESH) { reload_mods(); load_gamebanana(true); sync_mod_rows(); return 0; }
        if (id == ID_TROPHY_IMPORT) { import_trophy_key(); return 0; }
        if (id == ID_MODS_MASTER) {
            g.cfg.mods_enabled = !g.cfg.mods_enabled;
            save_config();
            InvalidateRect(reinterpret_cast<HWND>(lparam), nullptr, FALSE);
            sync_mod_rows();
            return 0;
        }

        if (id == ID_OPEN_MODS) {
            const fs::path folder = g.mods_view == 1 ? patches_dir() : mods_dir();
            std::error_code ec;
            fs::create_directories(folder, ec);
            ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOW);
            return 0;
        }
        if (id == ID_SAVE_BACKUP) { backup_save_data(); return 0; }
        if (id == ID_OPEN_SAVES) {
            const fs::path saves = g.root / "runtime/user/savedata";
            std::error_code ec;
            fs::create_directories(saves, ec);
            ShellExecuteW(nullptr, L"open", saves.c_str(), nullptr, nullptr, SW_SHOW);
            return 0;
        }
        if (id == ID_OPEN_LOGS) {
            const fs::path logs = g.root / "runtime/user/logs";
            std::error_code ec;
            fs::create_directories(logs, ec);
            ShellExecuteW(nullptr, L"open", logs.c_str(), nullptr, nullptr, SW_SHOW);
            return 0;
        }
        if (id == ID_AUDIO_TEST) {
            MessageBeep(MB_ICONASTERISK);
            return 0;
        }


        if(id==ID_ONLINE_FRIENDS){animate_friends(true);return 0;}

        if (id == ID_BASE_BROWSE) browse(g.base_edit, tr(L"Base game folder (CUSA14876)"));
        else if (id == ID_PATCH_BROWSE) browse(g.patch_edit, tr(L"Update folder (v01.21)"));
        else if (id >= ID_PILL && id < ID_PILL + 5) select_resolution(id - ID_PILL);
        else {
            const auto found = g.controls.find(reinterpret_cast<HWND>(lparam));
            if (found != g.controls.end() && found->second.kind == Kind::Toggle) {
                *found->second.value = !*found->second.value;
                if (id == ID_ONLINE_TOGGLE) {
                    host_toggled(true);
                }
                InvalidateRect(found->first, nullptr, FALSE);
                validate();
            }
        }
        return 0;
    }
    case WM_TIMER:
        if(wparam==kNitroTimer) {nitro_tick();return 0;}
        if(wparam==31) poll_launcher_update();
        if(wparam==20) {
            reload_friends();
            if(g.friends_open) InvalidateRect(g.friends_drawer,nullptr,FALSE);
            poll_fonts();
        }
        return 0;
    case WM_APP_FINISHED:
        game_finished(static_cast<DWORD>(wparam));
        return 0;
    case WM_APP_HOST_READY:
        host_ready(wparam != 0);
        return 0;
    case WM_APP_XBOX_READY:
        xbox_prompts_ready(reinterpret_cast<std::wstring*>(lparam));
        return 0;
    case WM_APP_SHADERS_PROGRESS:
        g.shaders_percent = static_cast<int>(wparam);
        InvalidateRect(g.launch, nullptr, FALSE);
        InvalidateRect(g.window, nullptr, FALSE);
        return 0;
    case WM_APP_SHADERS_READY:
        d3d_shaders_ready(static_cast<DWORD>(wparam), reinterpret_cast<std::wstring*>(lparam));
        return 0;
    case WM_APP_GB_CATALOG:
        gamebanana_catalog_ready(reinterpret_cast<ModsJob*>(lparam));
        return 0;
    case WM_APP_GB_PROGRESS:
        g.mods_busy_percent = static_cast<int>(wparam);
        InvalidateRect(g.window, nullptr, FALSE);
        return 0;
    case WM_APP_GB_DONE:
        mods_install_done(reinterpret_cast<ModsJob*>(lparam));
        return 0;
    case WM_APP_TROPHY_WIKI:
        trophy_wiki_ready(reinterpret_cast<std::wstring*>(lparam));
        return 0;
    case WM_CLOSE:
        if (g.process || g.host_busy || g.xbox_busy || g.shaders_busy || g.mods_busy || update_preparing) return 0;
        nitro_finish();
        save_config();
        KillTimer(window,20);
        KillTimer(window,31);
        g.host.stop(); // closes the router ports opened for hosting
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

fs::path find_root(const fs::path& start) {
    std::error_code ec;
    for (fs::path folder = start; !folder.empty(); folder = folder.parent_path()) {
        if (fs::exists(folder / "ctr_gpu.dll", ec) || fs::exists(folder / "build/CTRPort-gpu/ctr_gpu.dll", ec)) return folder;
        if (folder == folder.parent_path()) break;
    }
    return {};
}
}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&common);
    GdiplusStartupInput gdiplus_input;
    ULONG_PTR gdiplus = 0;
    GdiplusStartup(&gdiplus, &gdiplus_input, nullptr);

    wchar_t module[MAX_PATH * 4];
    GetModuleFileNameW(nullptr, module, static_cast<DWORD>(std::size(module)));
    g.exe_dir = fs::path(module).parent_path();
    g.root = find_root(g.exe_dir);
    if (ctr_update::portable(g.root, g.exe_dir)) {
        try {
            update_install_lock = ctr_update::acquire_install_lock(g.root);
            if (update_install_lock == INVALID_HANDLE_VALUE)
                throw std::runtime_error("Another launcher or updater is using this installation.");
            const auto pending = g.root / "runtime/user/updates/pending.json";
            if (fs::exists(pending)) {
                const fs::path txn = ctr_update::wide(ctr_update::read(pending).at("transaction").text);
                const auto name = txn.filename().wstring();
                if (txn.parent_path() != pending.parent_path() || name.size() != 36 || name.substr(0,4) != L"txn-" ||
                    name.substr(4).find_first_not_of(L"0123456789abcdef") != std::wstring::npos)
                    throw std::runtime_error("Invalid update recovery path.");
                ctr_update::assert_plain_path(txn / "apply.ps1");
                ctr_update::launch_apply(g.root, txn / "apply.ps1", txn, true);
                CloseHandle(update_install_lock);
                return 0;
            }
        } catch (const std::exception& e) {
            MessageBoxW(nullptr, ctr_update::wide(e.what()).c_str(), L"CTRNativePorted update", MB_ICONERROR);
            if (update_install_lock != INVALID_HANDLE_VALUE) CloseHandle(update_install_lock);
            return 1;
        }
    }
    std::error_code ec;
    if (fs::exists(g.exe_dir / "ctrport.exe", ec)) {
        g.runtime_exe = g.exe_dir / "ctrport.exe";
    } else if (fs::exists(g.root / "build/CTRPort-gpu/ctrport.exe", ec)) {
        g.runtime_exe = g.root / "build/CTRPort-gpu/ctrport.exe";
    } else {
        g.runtime_exe = g.root / "build/CTRPort-dev/ctrport.exe";
    }
    { std::ifstream prefs(g.root/"runtime/user/nitro-ui.txt");
      std::string data((std::istreambuf_iterator<char>(prefs)),{});
      const auto appearance=ctrport::launcher_appearance::parse(data);
      classic_skin=appearance.classic;nitro_motion=appearance.motion; }
    apply_launcher_palette();
    g.scale = static_cast<float>(GetDpiForSystem()) / 96.f;
    {RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);g.scale=std::min(g.scale,std::min((work.right-work.left-32)/kWidth,(work.bottom-work.top-40)/kHeight));}
    load_config();
    g.logo = load_launcher_logo();

    WNDCLASSEXW window_class{sizeof(window_class)};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = L"CTRNativePortedLauncher";
    RegisterClassExW(&window_class);
    const DWORD style = WS_POPUP | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT frame{0, 0, px(kWidth), px(kHeight)};
    AdjustWindowRectEx(&frame, style, FALSE, 0);
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int fw = frame.right - frame.left, fh = frame.bottom - frame.top;
    HWND window = CreateWindowExW(0, window_class.lpszClassName, kTitle, style, work.left + (work.right - work.left - fw) / 2,
                                  work.top + std::max(0L, (work.bottom - work.top - fh) / 2), fw, fh, nullptr, nullptr, instance, nullptr);

    SetWindowRgn(window, CreateRoundRectRgn(0, 0, px(kWidth) + 1, px(kHeight) + 1, px(24), px(24)), TRUE);
    const BOOL dark = TRUE;
    const COLORREF caption = RGB(18, 12, 48), caption_text = RGB(255, 248, 236), border = RGB(18, 12, 48);
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(window, 36, &caption_text, sizeof(caption_text));
    DwmSetWindowAttribute(window, 34, &border, sizeof(border));
    SetWindowTextW(window,classic_skin?L"CTR Native Ported — Classic":L"CTR Native Ported — Nitro Cinema");
    ShowWindow(window, show);
    UpdateWindow(window);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    nitro_finish();
    nitro_clear_surfaces();
    g.logo.reset();
    g.art.reset();
    g.icon.reset();
    g.trophy_icons.clear();
    if (font_load.valid()) { auto pending = font_load.get(); }
    retail_fonts = {};
    for (auto& cached : family_cache) cached.reset();
    GdiplusShutdown(gdiplus);
    CoUninitialize();
    if (update_job.valid()) update_job.wait();
    if (update_install_lock != INVALID_HANDLE_VALUE) CloseHandle(update_install_lock);
    return 0;
}
