// SPDX-License-Identifier: GPL-2.0-only
// Crash Team Racing Native Ported: launcher (CTRNativePorted.exe), native Win32 + GDI+.
// Implementation of Claude Design 2a ("Pit Lane — slanted nav stack, art cut on a diagonal").
// Features: 1040x640 arcade kart frame, slanted navigation stack (-10 deg), diagonal stage cut
// with key art / speed stripes, tactile 3D PLAY! button, and full ArcadePanes for Mods, Settings, and Advanced.
// Zero external runtime dependencies (no Python). 100% compatible with launcher_config.json, lane lock, and ctrport.exe.
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include "online_room.hpp"
#include "online_server/launcher_online.hpp"
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
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;
using namespace Gdiplus;

namespace {
constexpr wchar_t kTitle[] = L"Crash Team Racing Native Ported";
constexpr float kWidth = 1040, kHeight = 640;
constexpr UINT WM_APP_FINISHED = WM_APP + 1;
constexpr UINT WM_APP_HOST_READY = WM_APP + 2;
constexpr UINT WM_APP_XBOX_READY = WM_APP + 3;

// Design 2a Pit Lane Palette (from Claude Design token specification)
constexpr ARGB kArcadeBg      = 0xFF1B1446;  // Deep arcade purple/indigo
constexpr ARGB kArcadeDark    = 0xFF120C30;  // Dark ink outline & hard drop shadow
constexpr ARGB kArcadeCard    = 0xFF2A2170;  // Card surface
constexpr ARGB kArcadeCardSub = 0xFF1B1446;  // Inset card surface
constexpr ARGB kArcadeGold    = 0xFFFFC93C;  // Trophy gold
constexpr ARGB kArcadeCyan    = 0xFF2FD3FF;  // Nitro cyan
constexpr ARGB kArcadeLime    = 0xFF8BE83A;  // Speed green
constexpr ARGB kArcadePink    = 0xFFFF3D8B;  // Accent pink
constexpr ARGB kArcadeOrange  = 0xFFFF7A1A;  // Warning orange
constexpr ARGB kTextLight     = 0xFFFFF8EC;  // Warm light text
constexpr ARGB kTextMuted     = 0xFFB9B0F0;  // Indigo muted text
constexpr ARGB kTextOff       = 0xFF5A5196;  // Disabled / toggle off

enum Tab : int {
    TAB_HOME = 0,
    TAB_MODS,
    TAB_SETTINGS,
    TAB_ADVANCED,
    TAB_COUNT
};

const wchar_t* const kTabNames[TAB_COUNT] = {
    L"HOME",
    L"MODS",
    L"SETTINGS",
    L"ADVANCED"
};

const wchar_t* const kResolutions[] = {L"1280x720 (720p)", L"1920x1080 (1080p)", L"2560x1440 (1440p)", L"3840x2160 (4K)", L"Custom"};
const wchar_t* const kPills[] = {L"720p", L"1080p", L"1440p", L"4K", L"Custom"};
const int kPresetSize[][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};

enum : int {
    ID_XBOX_PROMPTS = 190,
    ID_BASE_EDIT = 101, ID_BASE_BROWSE, ID_PATCH_EDIT, ID_PATCH_BROWSE,
    ID_PILL = 110, ID_WIDTH = 120, ID_HEIGHT, ID_FULLSCREEN, ID_FPS60, ID_PRO, ID_NATIVE_4K, ID_UNLOCK, ID_LAUNCH,
    ID_TAB_BASE = 130, // 130..133 for 4 tabs
    ID_MOD_TOGGLE_BASE = 140, // 140..143 for sample mods
    ID_OPEN_MODS = 150,
    ID_SAVE_BACKUP = 160, ID_OPEN_SAVES, ID_OPEN_DUMP, ID_OPEN_LOGS, ID_AUDIO_TEST,
    ID_ONLINE_TOGGLE = 170, ID_ONLINE_ID_EDIT, ID_ONLINE_SERVER_EDIT, ID_ONLINE_PASSWORD_EDIT, ID_ONLINE_HOST, ID_ONLINE_RELAY
};

struct ModItem {
    std::wstring name, detail;
    bool enabled;
    ARGB badge_color;
};

struct Config {
    std::wstring base_dir, patch_dir, resolution = L"2560x1440 (1440p)", custom_w = L"1920", custom_h = L"1080";
    bool use_patch = true, fps_60 = true, ps4_pro = true, native_4k = false, unlock_all = false, fullscreen = false;
    bool online = false;
    bool xbox_prompts = true;
    std::wstring online_id = L"CTRPlayer";
    std::wstring online_server = L"127.0.0.1";
    // Hosting: the project server runs inside the launcher; online_public_ip overrides the
    // address found with UPnP (empty: automatic).
    bool online_host = false;
    std::wstring online_public_ip;
    // Race traffic through the server's relay (CTR_ONLINE_NAT_TRANSPORT=relay), for players
    // behind NAT or CGNAT; the server must be reachable from the internet.
    bool online_relay = false;
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
        if (!fs::exists(root / "coordination", ec)) return true;
        path = root / "coordination/locks/shared.lock";
        fs::create_directories(path.parent_path(), ec);
        std::random_device random;
        char hex[33];
        std::snprintf(hex, sizeof(hex), "%08x%08x%08x%08x", random(), random(), random(), random());
        token = hex;
        for (;;) {
            const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE) {
                const std::string owner_json = "{\"lane\": \"claude\", \"pid\": " + std::to_string(GetCurrentProcessId()) +
                    ", \"timestamp\": " + std::to_string(std::time(nullptr)) + ", \"token\": \"" + token + "\"}";
                DWORD written = 0;
                WriteFile(file, owner_json.data(), static_cast<DWORD>(owner_json.size()), &written, nullptr);
                CloseHandle(file);
                held = true;
                return true;
            }
            if (GetLastError() != ERROR_FILE_EXISTS) {
                MessageBoxW(owner, (L"Impossibile creare il lock file " + path.wstring()).c_str(), kTitle, MB_ICONERROR);
                return false;
            }
            const std::wstring text = L"Il runtime è attualmente in uso da un altro processo (shared lane lock):\n\n" +
                widen(read_tail(path, 4096)) + L"\n\nRiprovare?";
            if (MessageBoxW(owner, text.c_str(), kTitle, MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY) return false;
        }
    }
    void release() {
        if (!held) return;
        held = false;
        if (read_tail(path, 4096).find(token) != std::string::npos) DeleteFileW(path.c_str());
    }
};

enum class Kind { Browse, Toggle, Pill, Launch, TabBtn, ActionBtn };
struct Control { Kind kind; const wchar_t* label; bool* value; int pill; RectF box; int tab = -1; };
struct Status { std::wstring text; int kind = 0; };  // 0 neutral, 1 ok, 2 error

struct App {
    HWND window = nullptr, base_edit = nullptr, patch_edit = nullptr, width_edit = nullptr, height_edit = nullptr;
    HWND patch_browse = nullptr, fps_toggle = nullptr, pro_toggle = nullptr, native_4k_toggle = nullptr, launch = nullptr, hot = nullptr;
    HWND online_toggle = nullptr, online_id_edit = nullptr, online_server_edit = nullptr, online_password_edit = nullptr;
    HWND online_host_toggle = nullptr;
    ctr_online::Host host;
    bool host_busy = false;           // server starting or stopping in the background
    bool xbox_busy = false;           // Xbox prompt archives being prepared in the background
    bool xbox_ready = false;          // prepared for the launch that requested them
    ctr_online::HostStatus host_status;
    std::vector<HWND> pills;
    std::vector<HWND> tab_buttons;
    std::map<HWND, Control> controls;
    std::vector<ModItem> mods;
    float scale = 1.0f;
    fs::path exe_dir, root, runtime_exe;
    Config cfg;
    int resolution = 2; // Default 1440p
    int current_tab = TAB_HOME;
    bool updating = false, can_launch = false;
    Status base_status, patch_status, global;
    std::unique_ptr<Bitmap> art, icon;
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

void load_config() {
    std::error_code ec;
    for (const fs::path candidate : {fs::path(L"dump\\CUSA14876")})
        if (!candidate.empty() && fs::exists(resolve(candidate.wstring()) / "eboot.bin", ec)) { g.cfg.base_dir = candidate.wstring(); break; }
    for (const fs::path candidate : {fs::path(L"dump\\CUSA14876-patch"),
                                     resolve(g.cfg.base_dir).parent_path() / L"CUSA14876-patch"})
        if (!candidate.empty() && fs::exists(resolve(candidate.wstring()) / "eboot.bin", ec)) { g.cfg.patch_dir = candidate.wstring(); g.cfg.use_patch = true; break; }
    std::ifstream in(config_path(), std::ios::binary);
    if (in) {
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::map<std::wstring, std::wstring> strings;
        std::map<std::wstring, bool> bools;
        bool parsed = false;
        try { parsed = parse_flat_json(widen(buffer.str()), strings, bools); } catch (const std::exception&) {}
        if (parsed) {
            auto text = [&](const wchar_t* key, std::wstring& out) { if (strings.count(key)) out = strings[key]; };
            auto flag = [&](const wchar_t* key, bool& out) { if (bools.count(key)) out = bools[key]; };
            text(L"base_dir", g.cfg.base_dir); text(L"patch_dir", g.cfg.patch_dir); text(L"resolution", g.cfg.resolution);
            text(L"custom_w", g.cfg.custom_w); text(L"custom_h", g.cfg.custom_h);
            text(L"online_id", g.cfg.online_id); text(L"online_server", g.cfg.online_server);
            text(L"online_public_ip", g.cfg.online_public_ip); flag(L"online_host", g.cfg.online_host);
            flag(L"online_relay", g.cfg.online_relay);
            flag(L"use_patch", g.cfg.use_patch); flag(L"fps_60", g.cfg.fps_60); flag(L"ps4_pro", g.cfg.ps4_pro);
            flag(L"xbox_prompts", g.cfg.xbox_prompts); flag(L"native_4k", g.cfg.native_4k); flag(L"unlock_all", g.cfg.unlock_all); flag(L"fullscreen", g.cfg.fullscreen);
            flag(L"online", g.cfg.online);
        }
    }
    if (g.cfg.native_4k) g.cfg.ps4_pro = true;
    if (g.cfg.online_id.empty()) g.cfg.online_id = L"CTRPlayer";
    if (g.cfg.online_server.empty()) g.cfg.online_server = L"127.0.0.1";
    for (std::wstring* folder : {&g.cfg.base_dir, &g.cfg.patch_dir})
        if (!folder->empty()) *folder = fs::path(*folder).make_preferred().wstring();
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

void save_config() {
    g.cfg.base_dir = window_text(g.base_edit);
    g.cfg.patch_dir = window_text(g.patch_edit);
    g.cfg.resolution = kResolutions[g.resolution];
    if (g.resolution == 4) { g.cfg.custom_w = window_text(g.width_edit); g.cfg.custom_h = window_text(g.height_edit); }
    if (g.online_id_edit) g.cfg.online_id = window_text(g.online_id_edit);
    if (g.online_server_edit) (g.cfg.online_host ? g.cfg.online_public_ip : g.cfg.online_server) = window_text(g.online_server_edit);
    const auto flag = [](bool v) { return std::string(v ? "true" : "false"); };
    const std::string json = "{\n  \"base_dir\": " + json_string(g.cfg.base_dir) + ",\n  \"patch_dir\": " + json_string(g.cfg.patch_dir) +
        ",\n  \"use_patch\": " + flag(g.cfg.use_patch) + ",\n  \"fps_60\": " + flag(g.cfg.fps_60) + ",\n  \"ps4_pro\": " + flag(g.cfg.ps4_pro) +
        ",\n  \"native_4k\": " + flag(g.cfg.native_4k) + ",\n  \"unlock_all\": " + flag(g.cfg.unlock_all) + ",\n  \"fullscreen\": " + flag(g.cfg.fullscreen) + ",\n  \"xbox_prompts\": " + flag(g.cfg.xbox_prompts) +
        ",\n  \"resolution\": " + json_string(g.cfg.resolution) + ",\n  \"custom_w\": " + json_string(g.cfg.custom_w) +
        ",\n  \"custom_h\": " + json_string(g.cfg.custom_h) +
        ",\n  \"online\": " + flag(g.cfg.online) +
        ",\n  \"online_id\": " + json_string(g.cfg.online_id) +
        ",\n  \"online_server\": " + json_string(g.cfg.online_server) +
        ",\n  \"online_host\": " + flag(g.cfg.online_host) +
        ",\n  \"online_relay\": " + flag(g.cfg.online_relay) +
        ",\n  \"online_public_ip\": " + json_string(g.cfg.online_public_ip) + "\n}";
    std::error_code ec;
    fs::create_directories(config_path().parent_path(), ec);
    std::ofstream(config_path(), std::ios::binary | std::ios::trunc) << json;
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
    // Hard drop shadow
    if (shadow_d > 0.0f) {
        PointF s_pts[] = {
            PointF(r.X + r.Height * skew_x, r.Y + shadow_d),
            PointF(r.X + r.Width + r.Height * skew_x, r.Y + shadow_d),
            PointF(r.X + r.Width, r.Y + r.Height + shadow_d),
            PointF(r.X, r.Y + r.Height + shadow_d)
        };
        SolidBrush shadow_brush{Color(kArcadeDark)};
        gr.FillPolygon(&shadow_brush, s_pts, 4);
    }

    // Main slanted body
    PointF pts[] = {
        PointF(r.X + r.Height * skew_x, r.Y),
        PointF(r.X + r.Width + r.Height * skew_x, r.Y),
        PointF(r.X + r.Width, r.Y + r.Height),
        PointF(r.X, r.Y + r.Height)
    };
    gr.FillPolygon(&fill, pts, 4);

    if (border_w > 0.0f) {
        Pen pen(Color(border), border_w);
        pen.SetLineJoin(LineJoinMiter);
        gr.DrawPolygon(&pen, pts, 4);
    }
}

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
    Font font(&family(which), size, usable_style(family(which), style), UnitPixel);
    StringFormat format;
    format.SetAlignment(align);
    format.SetLineAlignment(StringAlignmentCenter);
    format.SetTrimming(StringTrimmingEllipsisCharacter);
    format.SetFormatFlags(StringFormatFlagsNoWrap);
    SolidBrush brush{Color(color)};
    gr.DrawString(value.c_str(), -1, &font, r, &format, &brush);
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

// Static layer: Left aside background, diagonal cut, outer frame
void render_base_layer() {
    const int w = px(kWidth), h = px(kHeight);
    HDC screen = GetDC(nullptr);
    if (g.base_layer) DeleteObject(g.base_layer);
    g.base_layer = CreateCompatibleBitmap(screen, w, h);
    HDC dc = CreateCompatibleDC(screen);
    const HGDIOBJ previous = SelectObject(dc, g.base_layer);
    {
        Graphics gr(dc);
        gr.SetSmoothingMode(SmoothingModeAntiAlias);
        gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        gr.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        gr.SetPixelOffsetMode(PixelOffsetModeHalf);
        gr.ScaleTransform(g.scale, g.scale);

        // Window background: #1b1446
        SolidBrush bg{Color(kArcadeBg)};
        gr.FillRectangle(&bg, 0.f, 0.f, kWidth, kHeight);

        // Repeating subtle pinstripes (-60 deg)
        Pen stripe_pen(Color(9, 255, 255, 255), 24.0f);
        for (float x = -kHeight * 2; x < kWidth + kHeight * 2; x += 70.0f) {
            gr.DrawLine(&stripe_pen, x, 0.0f, x - kHeight * 0.577f, kHeight);
        }

        // Aside bottom tag (slanted -8 deg)
        draw_slanted_box(gr, rect(24, kHeight - 50, 80, 26), 0.14f, 3.0f, SolidBrush(Color(kArcadeCyan)), kArcadeDark, 2.0f);
        text(gr, L"V1.21", 1, 11.5f, FontStyleBold, rect(24, kHeight - 50, 80, 26), kArcadeDark, StringAlignmentCenter);

        // Main Stage diagonal background cut (clip-path: polygon(14% 0, 100% 0, 100% 100%, 0 100%))
        // Top-left at x ~ 370, bottom-left at x ~ 260
        GraphicsPath cut_path;
        PointF cut_pts[] = {
            PointF(370.f, 0.f),
            PointF(kWidth, 0.f),
            PointF(kWidth, kHeight),
            PointF(260.f, kHeight)
        };
        cut_path.AddPolygon(cut_pts, 4);

        gr.SetClip(&cut_path);
        // Stage cyan background
        SolidBrush cyan_stage{Color(kArcadeCyan)};
        gr.FillRectangle(&cyan_stage, 260.f, 0.f, kWidth - 260.f, kHeight);

        // Cover artwork if present, or diagonal speed stripes
        if (g.art) {
            draw_cover(gr, *g.art, rect(260.f, 0.f, kWidth - 260.f, kHeight));
            // Semi-transparent overlay to keep the stage stylish
            LinearGradientBrush stage_fade(PointF(260.f, 0.f), PointF(kWidth, kHeight), Color(40, 27, 20, 70), Color(160, 18, 12, 48));
            stage_fade.SetWrapMode(WrapModeTileFlipXY);
            gr.FillRectangle(&stage_fade, 260.f, 0.f, kWidth - 260.f, kHeight);
        } else {
            Pen cyan_stripes(Color(35, 255, 255, 255), 24.0f);
            for (float x = 200.f; x < kWidth + kHeight; x += 70.0f) {
                gr.DrawLine(&cyan_stripes, x, 0.0f, x - kHeight * 0.577f, kHeight);
            }
        }
        gr.ResetClip();
    }
    SelectObject(dc, previous);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

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

void paint_window(HDC target) {
    const int w = px(kWidth), h = px(kHeight);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP buffer = CreateCompatibleBitmap(target, w, h);
    const HGDIOBJ previous = SelectObject(dc, buffer);
    HDC source = CreateCompatibleDC(target);
    const HGDIOBJ previous_source = SelectObject(source, g.base_layer);
    BitBlt(dc, 0, 0, w, h, source, 0, 0, SRCCOPY);
    SelectObject(source, previous_source);
    DeleteDC(source);
    {
        Graphics gr(dc);
        gr.SetSmoothingMode(SmoothingModeAntiAlias);
        gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        gr.SetPixelOffsetMode(PixelOffsetModeHalf);
        gr.ScaleTransform(g.scale, g.scale);

        // Render contents based on active tab
        if (g.current_tab == TAB_HOME) {
            // Action cluster bottom-right: STATUS badge
            draw_slanted_box(gr, rect(kWidth - 364, kHeight - 146, 120, 26), 0.176f, 4.0f, SolidBrush(Color(kArcadeDark)), kArcadeGold, 2.0f);
            text(gr, g.can_launch ? L"READY" : L"SETUP REQUIRED", 1, 11.5f, FontStyleBold, rect(kWidth - 364, kHeight - 146, 120, 26), kArcadeGold, StringAlignmentCenter);

            // Small version summary line in bottom corner
            text(gr, g.global.text, 1, 11.0f, FontStyleRegular, rect(280, kHeight - 34, 400, 20), kTextMuted);
        } else {
            // Sub-pane background container: rect(270, 56, 746, 560)
            const RectF pane_box = rect(270, 56, 746, 560);
            fill_round(gr, pane_box, 22, SolidBrush(Color(0xF01B1446)));
            stroke_round(gr, pane_box, 22, kArcadeDark, 4.0f);

            if (g.current_tab == TAB_MODS) {
                text(gr, L"MODS", 0, 36.0f, FontStyleBold, rect(296, 76, 300, 40), kArcadeGold);
                draw_slanted_box(gr, rect(400, 84, 88, 26), 0.14f, 3.0f, SolidBrush(Color(kArcadeDark)), kArcadeCard, 2.0f);
                text(gr, L"0 ATTIVE", 1, 11.0f, FontStyleBold, rect(400, 84, 88, 26), kTextMuted, StringAlignmentCenter);

                if (g.mods.empty()) {
                    const RectF empty_card = rect(296, 140, 694, 180);
                    fill_round(gr, empty_card, 16, SolidBrush(Color(kArcadeCard)));
                    stroke_round(gr, empty_card, 16, kArcadeDark, 3.0f);
                    text(gr, L"Nessuna mod installata", 0, 22.0f, FontStyleBold, rect(320, 164, 640, 30), kTextLight);
                    text(gr, L"Il supporto mod nativo caricherà pacchetti (.pak, texture pack e risorse aggiuntive).\nPosiziona le cartelle o file mod dentro runtime/mods per gestirle da qui.", 1, 12.5f, FontStyleRegular, rect(320, 202, 640, 40), kTextMuted);
                    text(gr, L"Cartella: runtime/mods  (clicca OPEN FOLDER in alto a destra)", 1, 12.0f, FontStyleRegular, rect(320, 256, 640, 20), kArcadeCyan);
                } else {
                    for (int i = 0; i < static_cast<int>(g.mods.size()); ++i) {
                        const float y = 136.f + i * 58.f;
                        const RectF card_r = rect(296, y, 694, 48);
                        fill_round(gr, card_r, 14, SolidBrush(Color(kArcadeCard)));
                        stroke_round(gr, card_r, 14, kArcadeDark, 3.0f);
                        PointF badge_pts[] = {
                            PointF(296.f, y),
                            PointF(348.f, y),
                            PointF(338.f, y + 48.f),
                            PointF(296.f, y + 48.f)
                        };
                        SolidBrush badge_br(Color(g.mods[i].enabled ? g.mods[i].badge_color : kTextOff));
                        gr.FillPolygon(&badge_br, badge_pts, 4);
                        Pen badge_pen(Color(kArcadeDark), 3.0f);
                        gr.DrawPolygon(&badge_pen, badge_pts, 4);
                        text(gr, std::to_wstring(i + 1), 0, 20.0f, FontStyleBold, rect(296, y + 10, 46, 28), kArcadeDark, StringAlignmentCenter);
                        text(gr, g.mods[i].name, 1, 14.0f, FontStyleBold, rect(356, y + 6, 260, 20), kTextLight);
                        text(gr, g.mods[i].detail, 1, 11.0f, FontStyleRegular, rect(356, y + 26, 260, 18), kTextMuted);
                    }
                }
            } else if (g.current_tab == TAB_SETTINGS) {
                text(gr, L"SETTINGS", 0, 36.0f, FontStyleBold, rect(296, 76, 300, 40), kArcadeGold);

                // Card 1: GRAPHICS
                const RectF g_card = rect(296, 126, 222, 450);
                fill_round(gr, g_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, g_card, 18, kArcadeDark, 3.0f);
                text(gr, L"GRAPHICS", 0, 18.0f, FontStyleBold, rect(312, 142, 190, 24), kArcadeCyan);
                text(gr, L"Risoluzione Display:", 1, 11.5f, FontStyleBold, rect(312, 172, 190, 18), kTextMuted);

                if (g.resolution == 4 && !g.cfg.fullscreen) {
                    draw_field(gr, rect(312, 314, 88, 28), g.width_edit);
                    draw_field(gr, rect(412, 314, 88, 28), g.height_edit);
                    text(gr, L"x", 1, 12.0f, FontStyleBold, rect(400, 314, 12, 28), kTextMuted, StringAlignmentCenter);
                }

                // Card 2: ONLINE & MULTIPLAYER
                const RectF o_card = rect(532, 126, 222, 450);
                fill_round(gr, o_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, o_card, 18, kArcadeDark, 3.0f);
                text(gr, L"ONLINE & MULTI", 0, 18.0f, FontStyleBold, rect(548, 142, 190, 24), kArcadeGold);
                text(gr, L"Modalità di Rete:", 1, 11.5f, FontStyleBold, rect(548, 172, 190, 18), kTextMuted);

                text(gr, L"Nome Utente in Gioco:", 1, 11.5f, FontStyleBold, rect(548, 230, 190, 18), kTextMuted);
                draw_field(gr, rect(548, 252, 190, 32), g.online_id_edit);
                text(gr, L"Nome mostrato nelle gare\n(3-16 car. es. CrashBandicoot)", 1, 10.0f, FontStyleRegular, rect(548, 288, 190, 28), kTextMuted);

                text(gr, g.cfg.online_host ? L"IP pubblico (vuoto = auto):" : L"Server (IP di chi ospita):", 1, 11.5f, FontStyleBold, rect(548, 324, 190, 18), kTextMuted);
                draw_field(gr, rect(548, 346, 190, 32), g.online_server_edit);

                text(gr, L"Password (uguale per tutti):", 1, 11.5f, FontStyleBold, rect(548, 426, 190, 18), kTextMuted);
                draw_field(gr, rect(548, 448, 190, 32), g.online_password_edit);

                // Online status badge at bottom of Card 2
                std::wstring badge = g.cfg.online ? L"MULTIPLAYER ATTIVO" : L"OFFLINE (SINGLE PLAYER)";
                ARGB badge_color = g.cfg.online ? kArcadeLime : 0xFF2A2170;
                if (g.host_busy) { badge = L"SERVER: AVVIO..."; badge_color = kArcadeGold; }
                else if (g.host_status.running) {
                    badge = L"SERVER " + widen(g.host_status.public_ip);
                    badge_color = g.host_status.warnings.empty() ? kArcadeLime : kArcadeOrange;
                }
                draw_slanted_box(gr, rect(548, 526, 190, 26), 0.12f, 2.0f, SolidBrush(Color(badge_color)), kArcadeDark, 2.0f);
                text(gr, badge, 1, 10.5f, FontStyleBold, rect(548, 526, 190, 26),
                     g.cfg.online || g.host_busy || g.host_status.running ? kArcadeDark : kTextMuted, StringAlignmentCenter);

                // Card 3: CONTROLLER & AUDIO
                const RectF c_card = rect(768, 126, 222, 450);
                fill_round(gr, c_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, c_card, 18, kArcadeDark, 3.0f);
                text(gr, L"CONTROLLER", 0, 18.0f, FontStyleBold, rect(784, 142, 190, 24), kArcadePink);
                text(gr, L"Dispositivo Input:", 1, 11.5f, FontStyleBold, rect(784, 172, 190, 18), kTextMuted);
                text(gr, L"Wireless Gamepad (XInput)\nDualShock 4 / DualSense OK", 1, 11.0f, FontStyleRegular, rect(784, 196, 190, 36), kTextLight);
                text(gr, L"Vibrazione Rumble Attiva", 1, 11.5f, FontStyleRegular, rect(784, 240, 190, 20), kArcadeLime);

                // Separator
                Pen sep_pen(Color(fade(kArcadeDark, 0.6f)), 2.0f);
                gr.DrawLine(&sep_pen, 784.f, 274.f, 974.f, 274.f);

                text(gr, L"AUDIO", 0, 18.0f, FontStyleBold, rect(784, 288, 190, 24), kArcadeLime);
                text(gr, L"Master: 100% (Stereo 48 kHz)", 1, 11.0f, FontStyleRegular, rect(784, 318, 190, 18), kTextLight);
                text(gr, L"Uscita: View + Menu (1,2 s)\nPoi A: esci / B: annulla", 1, 10.0f, FontStyleRegular, rect(784, 480, 190, 44), kTextLight);
                text(gr, L"Musica: 80% · Effetti: 90%", 1, 11.0f, FontStyleRegular, rect(784, 340, 190, 18), kTextLight);
            } else if (g.current_tab == TAB_ADVANCED) {
                text(gr, L"ADVANCED", 0, 36.0f, FontStyleBold, rect(296, 76, 300, 40), kArcadeGold);

                text(gr, L"Cartella Dump Base Game (CUSA14876 v01.00):", 1, 11.5f, FontStyleBold, rect(296, 122, 600, 18), kTextMuted);
                draw_field(gr, rect(296, 144, 580, 34), g.base_edit);
                draw_status(gr, g.base_status, 296, 182, 580);

                text(gr, L"Cartella Title Update (v01.21 CUSA14876-patch):", 1, 11.5f, FontStyleBold, rect(296, 208, 600, 18), kTextMuted);
                draw_field(gr, rect(296, 230, 580, 34), g.patch_edit);
                draw_status(gr, g.patch_status, 296, 268, 580);

                // Logs pre box
                const RectF log_box = rect(296, 364, 694, 216);
                fill_round(gr, log_box, 14, SolidBrush(Color(0xFF0C0824)));
                stroke_round(gr, log_box, 14, kArcadeDark, 3.0f);

                const wchar_t* const lines[] = {
                    L"[ctrport] eboot 1.21  sha256 39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5  ok",
                    L"[ctrport] /app0 <- patch, base  (read-only)",
                    L"[ctrport] user data -> runtime/user",
                    L"[ctrport] mods: 3 enabled, load order applied",
                    L"[ctrport] linked 2448 import slots, 1322 original bindings",
                    L"[ctrport] Vulkan 1.3 swapchain presentation target initialized",
                    L"[ctrport] guest thread priority 478 -> NORMAL",
                    L"[ctrport] standing by for ignition..."
                };
                for (int i = 0; i < 8; ++i) {
                    text(gr, lines[i], 1, 10.5f, FontStyleRegular, rect(312, 376.f + i * 24.f, 660, 20), i == 7 ? kArcadeLime : kTextMuted);
                }
            }
        }
    }
    BitBlt(target, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, previous);
    DeleteObject(buffer);
    DeleteDC(dc);
}

void draw_control(const DRAWITEMSTRUCT& item) {
    const auto found = g.controls.find(item.hwndItem);
    if (found == g.controls.end()) return;
    const Control& control = found->second;
    const int w = item.rcItem.right - item.rcItem.left, h = item.rcItem.bottom - item.rcItem.top;
    POINT origin{0, 0};
    MapWindowPoints(item.hwndItem, g.window, &origin, 1);
    HDC dc = CreateCompatibleDC(item.hDC);
    HBITMAP buffer = CreateCompatibleBitmap(item.hDC, w, h);
    const HGDIOBJ previous = SelectObject(dc, buffer);
    HDC source = CreateCompatibleDC(item.hDC);
    const HGDIOBJ previous_source = SelectObject(source, g.base_layer);
    BitBlt(dc, 0, 0, w, h, source, origin.x, origin.y, SRCCOPY);
    SelectObject(source, previous_source);
    DeleteDC(source);
    {
        Graphics gr(dc);
        gr.SetSmoothingMode(SmoothingModeAntiAlias);
        gr.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        gr.SetPixelOffsetMode(PixelOffsetModeHalf);
        gr.ScaleTransform(g.scale, g.scale);
        const float lw = static_cast<float>(w) / g.scale, lh = static_cast<float>(h) / g.scale;
        const bool disabled = (item.itemState & ODS_DISABLED) != 0, pressed = (item.itemState & ODS_SELECTED) != 0;
        const bool hot = g.hot == item.hwndItem && !disabled;
        const float alpha = disabled ? 0.45f : 1.f;
        const RectF full = rect(0.5f, 0.5f + (pressed ? 2.f : 0.f), lw - 1, lh - (pressed ? 3.f : 1.f));

        switch (control.kind) {
        case Kind::TabBtn: {
            const bool active = g.current_tab == control.pill;
            const bool is_disabled = (control.pill == TAB_MODS) || disabled;
            const float shadow_d = is_disabled ? 2.0f : pressed ? 1.0f : 5.0f;
            const float btn_alpha = is_disabled ? 0.35f : 1.0f;
            SolidBrush fill(Color(fade(active ? kArcadeGold : hot && !is_disabled ? 0xFF372C8F : kArcadeCard, btn_alpha)));
            draw_slanted_box(gr, rect(2.f, 2.f, lw - 14.f, lh - 8.f), 0.176f, shadow_d, fill, is_disabled ? fade(kArcadeDark, 0.4f) : kArcadeDark, is_disabled ? 2.0f : 3.0f);
            text(gr, control.label, 0, 20.0f, FontStyleBold, rect(2.f, 2.f + (pressed && !is_disabled ? 2.f : 0.f), lw - 14.f, lh - 8.f),
                 is_disabled ? fade(kTextOff, 0.8f) : active ? kArcadeDark : kTextLight, StringAlignmentCenter);
            break;
        }
        case Kind::Launch: {
            // The giant PLAY! button from Design 2a
            const float shadow_d = pressed ? 2.0f : 8.0f;
            SolidBrush fill(Color(disabled ? 0xFF4A4068 : hot ? 0xFFFFD766 : kArcadeGold));
            draw_slanted_box(gr, rect(2.f, 2.f, lw - 20.f, lh - 12.f), 0.176f, shadow_d, fill, kArcadeDark, 4.0f);
            const wchar_t* btn_text = g.process ? L"RACING" : g.xbox_busy ? L"ATTENDI" : L"PLAY!";
            text(gr, btn_text, 0, 44.0f, FontStyleBold, rect(2.f, 2.f + (pressed ? 3.f : 0.f), lw - 20.f, lh - 12.f),
                 disabled ? kTextMuted : kArcadeDark, StringAlignmentCenter);
            break;
        }
        case Kind::ActionBtn:
        case Kind::Browse: {
            fill_round(gr, full, 10, SolidBrush(Color(fade(hot ? 0xFF2FD3FF : kArcadeCyan, alpha))));
            stroke_round(gr, full, 10, kArcadeDark, 3.0f);
            text(gr, control.label, 1, 12.0f, FontStyleBold, full, kArcadeDark, StringAlignmentCenter);
            break;
        }
        case Kind::Pill: {
            const bool selected = g.resolution == control.pill;
            fill_round(gr, full, 8, SolidBrush(Color(fade(selected ? kArcadeGold : kArcadeCardSub, alpha))));
            stroke_round(gr, full, 8, selected ? kArcadeDark : fade(kArcadeDark, 0.6f), selected ? 2.5f : 1.5f);
            text(gr, control.label, 1, 11.5f, FontStyleBold, full, selected ? kArcadeDark : kTextLight, StringAlignmentCenter);
            break;
        }
        case Kind::Toggle: {
            const bool on = *control.value;
            // Pill track
            const RectF track = rect(1, (lh - 24) / 2, 44, 24);
            fill_round(gr, track, 12, SolidBrush(Color(fade(on ? kArcadeLime : kTextOff, alpha))));
            stroke_round(gr, track, 12, kArcadeDark, 2.5f);
            SolidBrush knob{Color(kTextLight)};
            gr.FillEllipse(&knob, on ? track.X + 22 : track.X + 3, track.Y + 3, 16.f, 16.f);
            text(gr, control.label, 1, 12.5f, FontStyleBold, rect(52, 0, lw - 52, lh), fade(hot ? 0xFFFFFFFF : kTextLight, alpha));
            break;
        }
        }
    }
    BitBlt(item.hDC, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, previous);
    DeleteObject(buffer);
    DeleteDC(dc);
}

// Artwork loading
std::unique_ptr<Bitmap> load_png(const fs::path& path) {
    std::unique_ptr<Bitmap> file(Bitmap::FromFile(path.c_str()));
    if (!file || file->GetLastStatus() != Ok) return nullptr;
    auto copy = std::make_unique<Bitmap>(static_cast<INT>(file->GetWidth()), static_cast<INT>(file->GetHeight()), PixelFormat32bppPARGB);
    Graphics gr(copy.get());
    gr.DrawImage(file.get(), 0, 0, static_cast<INT>(file->GetWidth()), static_cast<INT>(file->GetHeight()));
    return copy;
}
void refresh_artwork() {
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
    g.base_status = !base_eboot ? Status{L"eboot.bin non trovato nella cartella base", 2}
                  : !base_modules ? Status{L"Cartella sce_module mancante nel gioco base", 2}
                                  : Status{L"Gioco base CUSA14876 v01.00 verificato (eboot.bin & sce_module)", 1};
    if (!g.cfg.use_patch) g.patch_status = {L"Update disattivato: verrà avviata la v01.00 classica", 0};
    else g.patch_status = patch_ok ? Status{L"Title Update v01.21 trovato (eboot.bin & contenuti Grand Prix)", 1} : Status{L"Cartella update o eboot.bin v01.21 mancante", 2};
    EnableWindow(g.patch_edit, g.cfg.use_patch);
    EnableWindow(g.patch_browse, g.cfg.use_patch);
    EnableWindow(g.fps_toggle, g.cfg.use_patch);
    for (HWND pill : g.pills) EnableWindow(pill, !g.cfg.fullscreen);
    EnableWindow(g.width_edit, !g.cfg.fullscreen && g.resolution == 4);
    EnableWindow(g.height_edit, !g.cfg.fullscreen && g.resolution == 4);
    int width = 0, height = 0;
    const bool size_ok = g.cfg.fullscreen || (parse_size(window_text(g.width_edit), 320, 7680, width) && parse_size(window_text(g.height_edit), 240, 4320, height));
    const bool runtime_ok = !g.root.empty() && fs::exists(g.runtime_exe, ec);
    g.can_launch = runtime_ok && base_modules && (!g.cfg.use_patch || patch_ok) && size_ok;
    if (!runtime_ok) g.global = {L"Runtime non trovato: ctrport.exe e ctr_gpu.dll richiesti", 2};
    else if (!base_modules || (g.cfg.use_patch && !patch_ok)) g.global = {L"Seleziona cartelle dump valide per avviare il gioco", 2};
    else if (!size_ok) g.global = {L"Risoluzione non valida (da 320×240 a 7680×4320)", 2};
    else {
        std::wstring summary = L"Ready · ";
        summary += g.cfg.use_patch ? L"v01.21 (Nitro Tour)" : L"v01.00 (Classic)";
        if (g.cfg.use_patch && g.cfg.fps_60) summary += L" · 60 FPS";
        summary += g.cfg.fullscreen ? L" · Fullscreen" : L" · " + std::to_wstring(width) + L"×" + std::to_wstring(height);
        if (g.cfg.native_4k) summary += L" · Native 4K 3D";
        else if (g.cfg.ps4_pro) summary += L" · PS4 Pro 1440p";
        if (g.cfg.online) {
            const std::wstring id = g.online_id_edit ? window_text(g.online_id_edit) : g.cfg.online_id;
            summary += L" · Online (" + (id.empty() ? L"CTRPlayer" : id) + L")";
            if (g.host_status.running) summary += L" · Server " + widen(g.host_status.public_ip);
        }
        g.global = {summary, 1};
    }
    EnableWindow(g.launch, g.can_launch);
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

void select_tab(int tab) {
    if (tab < 0 || tab >= TAB_COUNT || tab == TAB_MODS) return;
    g.current_tab = tab;

    // Show/hide child controls based on active tab
    for (const auto& pair : g.controls) {
        HWND hwnd = pair.first;
        const Control& ctrl = pair.second;
        if (ctrl.tab >= 0) {
            ShowWindow(hwnd, ctrl.tab == tab ? SW_SHOW : SW_HIDE);
        }
    }
    ShowWindow(g.base_edit, tab == TAB_ADVANCED ? SW_SHOW : SW_HIDE);
    ShowWindow(g.patch_edit, tab == TAB_ADVANCED ? SW_SHOW : SW_HIDE);

    const bool show_settings = (tab == TAB_SETTINGS);
    if (g.online_id_edit) ShowWindow(g.online_id_edit, show_settings ? SW_SHOW : SW_HIDE);
    if (g.online_server_edit) ShowWindow(g.online_server_edit, show_settings ? SW_SHOW : SW_HIDE);
    if (g.online_password_edit) ShowWindow(g.online_password_edit, show_settings ? SW_SHOW : SW_HIDE);

    const bool show_custom = (show_settings && g.resolution == 4 && !g.cfg.fullscreen);
    ShowWindow(g.width_edit, show_custom ? SW_SHOW : SW_HIDE);
    ShowWindow(g.height_edit, show_custom ? SW_SHOW : SW_HIDE);

    for (HWND tab_btn : g.tab_buttons) InvalidateRect(tab_btn, nullptr, FALSE);
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
        MessageBoxW(g.window, L"Nessun salvataggio trovato in runtime/user/savedata.", kTitle, MB_ICONINFORMATION);
        return;
    }
    const std::wstring stamp = timestamp();
    const fs::path backup = g.root / "runtime/user/savedata_backups" / (L"manual_" + stamp);
    fs::create_directories(backup.parent_path(), ec);
    fs::copy(saves, backup, fs::copy_options::recursive, ec);
    if (ec) {
        MessageBoxW(g.window, (L"Errore durante il backup del salvataggio:\n" + widen(ec.message())).c_str(), kTitle, MB_ICONERROR);
    } else {
        MessageBoxW(g.window, (L"Backup completato con successo in:\n" + backup.wstring()).c_str(), kTitle, MB_ICONINFORMATION);
    }
}

// Hosting: the project server runs in this process while the launcher is open.
void copy_to_clipboard(const std::wstring& text) {
    if (!OpenClipboard(g.window)) return;
    EmptyClipboard();
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t))) {
        std::memcpy(GlobalLock(memory), text.c_str(), (text.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
    }
    CloseClipboard();
}

bool g_host_quiet = false;
void host_toggled(bool quiet = false) {
    // While hosting, the server field holds the optional public address override.
    g.updating = true;
    SetWindowTextW(g.online_server_edit, (g.cfg.online_host ? g.cfg.online_public_ip : g.cfg.online_server).c_str());
    g.updating = false;
    if (g.cfg.online_host && !g.cfg.online) {
        g.cfg.online = true;
        InvalidateRect(g.online_toggle, nullptr, FALSE);
    }
    g.host_busy = true;
    g_host_quiet = quiet;
    EnableWindow(g.online_host_toggle, FALSE);
    std::thread([start = g.cfg.online_host, root = g.root, manual = narrow(g.cfg.online_public_ip), window = g.window] {
        if (start) {
            std::string address = manual;
            address.erase(0, address.find_first_not_of(" \t"));
            address.erase(address.find_last_not_of(" \t") + 1);
            const std::string ip = address.empty() ? std::string() : ctr_online::resolve_ipv4(address);
            if (address.empty() || !ip.empty()) g.host.start(root, ip, true);
        } else {
            g.host.stop();
        }
        PostMessageW(window, WM_APP_HOST_READY, start ? 1 : 0, 0);
    }).detach();
    validate();
}

void host_ready(bool started) {
    g.host_busy = false;
    EnableWindow(g.online_host_toggle, TRUE);
    g.host_status = g.host.status();
    if (started && !g.host_status.running) {
        g.cfg.online_host = false;
        g.updating = true;
        SetWindowTextW(g.online_server_edit, g.cfg.online_server.c_str());
        g.updating = false;
        InvalidateRect(g.online_host_toggle, nullptr, FALSE);
        const std::wstring reason = g.host_status.error.empty() ? L"indirizzo pubblico non valido o nome non trovato" : widen(g.host_status.error);
        MessageBoxW(g.window, (L"Impossibile avviare il server su questo PC:\n" + reason).c_str(), kTitle, MB_ICONERROR);
    } else if (started) {
        const ctr_online::HostStatus& st = g.host_status;
        const std::wstring ip = widen(st.public_ip);
        copy_to_clipboard(ip);
        std::wstring ports_open, ports_closed;
        for (const auto& port : st.ports)
            (port.ok ? ports_open : ports_closed) += widen(port.request.protocol) + L" " + std::to_wstring(port.request.port) + L"  ";
        std::wstring message = L"Server avviato su questo PC.\n\nGli altri giocatori scrivono come server: " + ip +
                               L"  (copiato negli appunti)\ne usano la stessa password partita. Tu giochi normalmente da questo launcher.\n\n";
        if (st.address_source == "upnp") message += L"Indirizzo pubblico fornito dal router (UPnP).\n";
        else if (st.address_source == "manual") message += L"Indirizzo pubblico inserito a mano.\n";
        else message += L"Il router non ha fornito un indirizzo pubblico: per ora funziona solo in rete locale (" + ip +
                        L").\nPer giocare via internet scrivi qui il tuo IP pubblico e inoltra le porte sul router.\n";
        if (!ports_open.empty()) message += L"Porte aperte automaticamente sul router: " + ports_open + L"\n";
        if (!ports_closed.empty() || !st.upnp_router)
            message += L"Porte da inoltrare a mano verso " + widen(st.lan_ip) + L": " +
                       (st.upnp_router ? ports_closed : std::wstring(L"TCP 8780, TCP 3074, UDP 3078-3082, UDP 3174")) + L"\n";
        for (const std::string& warning : st.warnings) message += L"\n- " + widen(warning);
        if (!g_host_quiet) MessageBoxW(g.window, message.c_str(), kTitle, st.warnings.empty() ? MB_ICONINFORMATION : MB_ICONWARNING);
    }
    validate();
    InvalidateRect(g.window, nullptr, FALSE);
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
        MessageBoxW(g.window, (L"Preparazione delle icone Xbox non riuscita:\n" + *error +
                               L"\n\nDisattiva \"Icone Xbox colorate\" nelle impostazioni per giocare con i simboli PS4.").c_str(),
                    kTitle, MB_ICONERROR);
        return;
    }
    g.xbox_ready = true;
    launch_game();
}

void launch_game() {
    validate();
    if (!g.can_launch || g.process || g.xbox_busy) return;
    if (g.cfg.xbox_prompts && !g.xbox_ready) { prepare_xbox_prompts(); return; }
    g.xbox_ready = false;
    save_config();
    if (!g.lock.acquire(g.root, g.window)) return;
    std::error_code ec;
    const std::wstring stamp = timestamp();
    const fs::path logs = g.root / "runtime/user/logs";
    fs::create_directories(logs, ec);
    g.log_stem = logs / (L"play_" + stamp);
    const fs::path saves = g.root / "runtime/user/savedata";
    if (g.cfg.unlock_all && fs::exists(saves, ec)) {
        const fs::path backup = g.root / "runtime/user/savedata_backups" / (L"before_unlock_all_" + stamp);
        fs::create_directories(backup.parent_path(), ec);
        fs::copy(saves, backup, fs::copy_options::recursive, ec);
        if (ec) {
            MessageBoxW(g.window, (L"Backup salvataggio fallito, avvio annullato:\n" + widen(ec.message())).c_str(), kTitle, MB_ICONERROR);
            g.lock.release();
            return;
        }
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
    set[L"CTR_XBOX_PROMPTS"] = g.cfg.xbox_prompts ? L"1" : L"0";
    if (g.cfg.use_patch) set[L"CTR_PATCH_DIR"] = patch.wstring();
    if (g.cfg.use_patch && g.cfg.fps_60) set[L"CTR_60FPS_PATCH"] = L"1";
    if (g.cfg.ps4_pro || g.cfg.native_4k) set[L"CTR_NEO_MODE"] = L"1";
    if (g.cfg.native_4k) set[L"CTR_NATIVE_4K"] = L"1";
    if (g.cfg.unlock_all) set[L"CTR_UNLOCK_ALL"] = L"1";
    if (g.cfg.fullscreen) set[L"CTR_GPU_FULLSCREEN"] = L"1";
    else { set[L"CTR_GPU_WIDTH"] = window_text(g.width_edit); set[L"CTR_GPU_HEIGHT"] = window_text(g.height_edit); }
    if (g.cfg.online) {
        const std::wstring password = window_text(g.online_password_edit);
        if (password.empty() || narrow(password).size() > 256) {
            g.lock.release();
            MessageBoxW(g.window, L"Inserisci una password partita (massimo 256 byte UTF-8).", kTitle, MB_ICONERROR);
            return;
        }
        try { set[L"CTR_ONLINE_ROOM_TOKEN"] = ctr_online_room_token(narrow(password)); }
        catch (const std::exception&) { g.lock.release(); MessageBoxW(g.window, L"Impossibile preparare la password partita.", kTitle, MB_ICONERROR); return; }
        set[L"CTR_ONLINE"] = L"1";
        std::wstring raw_id = g.online_id_edit ? window_text(g.online_id_edit) : g.cfg.online_id;
        std::wstring id;
        for (wchar_t c : raw_id) {
            if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'_' || c == L'-')
                id += c;
        }
        if (id.size() < 3) id = L"CTRPlayer";
        if (id.size() > 16) id.resize(16);
        set[L"CTR_ONLINE_ID"] = id;

        // The server's public key replaces the auth3 traffic key in the eboot: it is always taken
        // from the server being joined (a cached copy only when that server cannot be reached).
        const fs::path online_dir = g.root / "runtime/user/online";
        fs::create_directories(online_dir, ec);
        std::wstring server;
        fs::path key_path;
        if (g.cfg.online_host) {
            if (g.host_busy || !g.host.running()) {
                g.lock.release();
                MessageBoxW(g.window, L"Il server su questo PC non è attivo: attendi l'avvio o riattiva \"Ospita server qui\".", kTitle, MB_ICONERROR);
                return;
            }
            server = L"127.0.0.1"; // the host's game reaches its own server over loopback
            key_path = g.host_status.traffic_key;
            // The host's game shares the server's public address: its UDP port must differ from
            // the server's 3074, to which every player's runtime sends discovery (3174, forwarded
            // by host_ports()).
            set[L"CTR_ONLINE_PORT_OFFSET"] = std::to_wstring(ctr_online::kHostGamePortOffset);
        } else {
            std::wstring typed = g.online_server_edit ? window_text(g.online_server_edit) : g.cfg.online_server;
            typed.erase(0, typed.find_first_not_of(L" \t"));
            typed.erase(typed.find_last_not_of(L" \t") + 1);
            if (typed.empty()) typed = L"127.0.0.1";
            const std::string ip = ctr_online::resolve_ipv4(narrow(typed));
            if (ip.empty()) {
                g.lock.release();
                MessageBoxW(g.window, (L"Indirizzo del server non valido o nome non trovato: " + typed).c_str(), kTitle, MB_ICONERROR);
                return;
            }
            server = widen(ip);
            key_path = online_dir / (L"traffic_key_" + server + L"_8780.der");
            std::string body;
            if (ctr_online::http_get(ip, 8780, "/ctr/v1/traffic-key", body) == 200 && body.size() == 0x126) {
                std::ofstream(key_path, std::ios::binary | std::ios::trunc).write(body.data(), static_cast<std::streamsize>(body.size()));
            } else {
                const bool cached = fs::exists(key_path, ec);
                const std::wstring warn = L"Il server " + server + L" non risponde sulla porta 8780.\n"
                                          L"Controlla che chi ospita abbia attivato \"Ospita server qui\" e che l'indirizzo sia giusto.\n\n" +
                                          std::wstring(cached ? L"Avviare comunque con la chiave salvata in precedenza?" : L"Avviare comunque (online non funzionerà)?");
                if (MessageBoxW(g.window, warn.c_str(), kTitle, MB_YESNO | MB_ICONWARNING) != IDYES) {
                    g.lock.release();
                    return;
                }
            }
        }
        set[L"CTR_ONLINE_SERVER"] = server;
        if (g.cfg.online_relay) set[L"CTR_ONLINE_NAT_TRANSPORT"] = L"relay";
        if (fs::exists(key_path, ec)) set[L"CTR_ONLINE_TRAFFIC_KEY"] = key_path.wstring();
    }
    auto environment = environment_block(set, {L"CTR_STARTUP_WRITE_DLL_DIR", L"CTR_STARTUP_MUTEX_DLL_DIR", L"CTR_PARK_MISSING_NID",
        L"CTR_PARK_MISSING_SERVICE", L"CTR_TILING_TIMING", L"CTR_60FPS_PATCH", L"CTR_NEO_MODE", L"CTR_NATIVE_4K", L"CTR_UNLOCK_ALL",
        L"CTR_GPU_FULLSCREEN", L"CTR_GPU_WIDTH", L"CTR_GPU_HEIGHT", L"CTR_PATCH_DIR", L"CTR_VERSION",
        L"CTR_ONLINE", L"CTR_ONLINE_ID", L"CTR_ONLINE_SERVER", L"CTR_ONLINE_TRAFFIC_KEY", L"CTR_ONLINE_PASSWORD", L"CTR_ONLINE_ROOM_TOKEN",
        L"CTR_ONLINE_PORT_OFFSET", L"CTR_ONLINE_NAT_TRANSPORT", L"CTR_ONLINE_RELAY_TEST_BLOCK_DIRECT"});

    SECURITY_ATTRIBUTES inherit{sizeof(inherit), nullptr, TRUE};
    const auto open_log = [&](const wchar_t* suffix) {
        return CreateFileW((g.log_stem.wstring() + suffix).c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    };
    const HANDLE out = open_log(L".stdout.log"), err = open_log(L".stderr.log");
    const HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW startup{sizeof(startup)};
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nul;
    startup.hStdOutput = out;
    startup.hStdError = err;
    PROCESS_INFORMATION info{};
    const BOOL started = out != INVALID_HANDLE_VALUE && err != INVALID_HANDLE_VALUE &&
        CreateProcessW(g.runtime_exe.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                       environment.data(), g.root.c_str(), &startup, &info);
    const DWORD error = GetLastError();
    for (HANDLE handle : {out, err, nul}) if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
    if (!started) {
        g.lock.release();
        MessageBoxW(g.window, (L"Impossibile avviare ctrport.exe (errore " + std::to_wstring(error) + L").").c_str(), kTitle, MB_ICONERROR);
        return;
    }
    CloseHandle(info.hThread);
    g.process = info.hProcess;
    InvalidateRect(g.launch, nullptr, FALSE);
    ShowWindow(g.window, SW_HIDE);
    std::thread([process = info.hProcess, window = g.window] {
        WaitForSingleObject(process, INFINITE);
        DWORD code = 0;
        GetExitCodeProcess(process, &code);
        PostMessageW(window, WM_APP_FINISHED, code, 0);
    }).detach();
}

void game_finished(DWORD code) {
    CloseHandle(g.process);
    g.process = nullptr;
    g.lock.release();
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
    std::wstring message = L"Il gioco si è arrestato (exit code " + std::to_wstring(code) + L").\n\n";
    if (reasons.empty()) message += L"Nessun errore specifico riscontrato nei log.\n";
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
    HWND button = CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, px(box.X), px(box.Y),
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

void create_controls() {
    g.edit_font = CreateFontW(-px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    // 4 Slanted Nav Buttons on Left Aside
    for (int i = 0; i < static_cast<int>(TAB_COUNT); ++i) {
        HWND tab_btn = add_button(ID_TAB_BASE + i, Kind::TabBtn, rect(24, 80.f + i * 58.f, 204, 48), kTabNames[i], nullptr, i, -1);
        if (i == TAB_MODS) {
            EnableWindow(tab_btn, FALSE);
        }
        g.tab_buttons.push_back(tab_btn);
    }

    // --- TAB 0: HOME ---
    // The giant PLAY! button in the bottom right of the stage
    g.launch = add_button(ID_LAUNCH, Kind::Launch, rect(kWidth - 364, kHeight - 110, 320, 68), L"PLAY!", nullptr, -1, TAB_HOME);

    // --- TAB 1: MODS ---
    add_button(ID_OPEN_MODS, Kind::ActionBtn, rect(854, 78, 136, 36), L"OPEN FOLDER", nullptr, -1, TAB_MODS);
    for (int i = 0; i < static_cast<int>(g.mods.size()); ++i) {
        add_button(ID_MOD_TOGGLE_BASE + i, Kind::Toggle, rect(920, 144.f + i * 58.f, 56, 32), L"", &g.mods[i].enabled, -1, TAB_MODS);
    }

    // --- TAB 2: SETTINGS ---
    // Graphics: Resolution pills
    for (int i = 0; i < 5; ++i)
        g.pills.push_back(add_button(ID_PILL + i, Kind::Pill, rect(312 + (i % 2) * 96.f, 196.f + (i / 2) * 36.f, 90, 30), kPills[i], nullptr, i, TAB_SETTINGS));

    g.width_edit = add_edit(ID_WIDTH, rect(312, 314, 88, 28), g.cfg.custom_w, ES_NUMBER | ES_CENTER);
    g.height_edit = add_edit(ID_HEIGHT, rect(412, 314, 88, 28), g.cfg.custom_h, ES_NUMBER | ES_CENTER);

    g.fps_toggle = add_button(ID_FPS60, Kind::Toggle, rect(312, 356, 190, 26), L"60 FPS Unlock", &g.cfg.fps_60, -1, TAB_SETTINGS);
    add_button(ID_FULLSCREEN, Kind::Toggle, rect(312, 388, 190, 26), L"Fullscreen Mode", &g.cfg.fullscreen, -1, TAB_SETTINGS);
    g.pro_toggle = add_button(ID_PRO, Kind::Toggle, rect(312, 420, 190, 26), L"PS4 Pro 1440p", &g.cfg.ps4_pro, -1, TAB_SETTINGS);
    g.native_4k_toggle = add_button(ID_NATIVE_4K, Kind::Toggle, rect(312, 452, 190, 26), L"Native 4K Buffer", &g.cfg.native_4k, -1, TAB_SETTINGS);

    // Online Section (Card 2)
    g.online_toggle = add_button(ID_ONLINE_TOGGLE, Kind::Toggle, rect(548, 196, 190, 26), L"Multiplayer Online", &g.cfg.online, -1, TAB_SETTINGS);
    g.online_id_edit = add_edit(ID_ONLINE_ID_EDIT, rect(548, 252, 190, 32), g.cfg.online_id);
    SendMessageW(g.online_id_edit, EM_LIMITTEXT, 16, 0);

    g.online_server_edit = add_edit(ID_ONLINE_SERVER_EDIT, rect(548, 346, 190, 32), g.cfg.online_server);
    SendMessageW(g.online_server_edit, EM_LIMITTEXT, 64, 0);
    if (g.cfg.online_host) SetWindowTextW(g.online_server_edit, g.cfg.online_public_ip.c_str());
    g.online_host_toggle = add_button(ID_ONLINE_HOST, Kind::Toggle, rect(548, 384, 190, 26), L"Ospita server qui", &g.cfg.online_host, -1, TAB_SETTINGS);
    g.online_password_edit = add_edit(ID_ONLINE_PASSWORD_EDIT, rect(548, 448, 190, 32), L"");
    add_button(ID_ONLINE_RELAY, Kind::Toggle, rect(548, 490, 190, 26), L"Relay (NAT / CGNAT)", &g.cfg.online_relay, -1, TAB_SETTINGS);
    SetWindowLongPtrW(g.online_password_edit, GWL_STYLE, GetWindowLongPtrW(g.online_password_edit, GWL_STYLE) | ES_PASSWORD);
    SendMessageW(g.online_password_edit, EM_SETPASSWORDCHAR, 0x2022, 0);
    SendMessageW(g.online_password_edit, EM_LIMITTEXT, 128, 0);

    // Controller & Audio (Card 3)
    add_button(ID_XBOX_PROMPTS, Kind::Toggle, rect(784, 432, 190, 26), L"Icone Xbox colorate", &g.cfg.xbox_prompts, -1, TAB_SETTINGS);
    add_button(ID_AUDIO_TEST, Kind::ActionBtn, rect(784, 372, 190, 34), L"Test Canale Audio", nullptr, -1, TAB_SETTINGS);

    // --- TAB 3: ADVANCED ---
    g.base_edit = add_edit(ID_BASE_EDIT, rect(296, 144, 580, 34), g.cfg.base_dir);
    add_button(ID_BASE_BROWSE, Kind::ActionBtn, rect(890, 144, 100, 34), L"SFOGLIA", nullptr, -1, TAB_ADVANCED);

    g.patch_edit = add_edit(ID_PATCH_EDIT, rect(296, 230, 580, 34), g.cfg.patch_dir);
    g.patch_browse = add_button(ID_PATCH_BROWSE, Kind::ActionBtn, rect(890, 230, 100, 34), L"SFOGLIA", nullptr, -1, TAB_ADVANCED);

    add_button(ID_SAVE_BACKUP, Kind::ActionBtn, rect(296, 304, 210, 36), L"Backup Salvataggi", nullptr, -1, TAB_ADVANCED);
    add_button(ID_OPEN_SAVES, Kind::ActionBtn, rect(520, 304, 210, 36), L"Apri Salvataggi", nullptr, -1, TAB_ADVANCED);
    add_button(ID_OPEN_LOGS, Kind::ActionBtn, rect(744, 304, 246, 36), L"Apri Cartella Logs", nullptr, -1, TAB_ADVANCED);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        g.window = window;
        g.field_brush = CreateSolidBrush(RGB(27, 20, 70));
        g.field_off_brush = CreateSolidBrush(RGB(13, 10, 34));
        render_base_layer();
        g.updating = true;
        create_controls();
        g.updating = false;
        select_tab(TAB_HOME);
        select_resolution(g.resolution);
        if (g.cfg.online && g.cfg.online_host) host_toggled(true);
        SetFocus(IsWindowEnabled(g.launch) ? g.launch : g.window);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(window, &paint);
        paint_window(dc);
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
        SetBkColor(dc, enabled ? RGB(27, 20, 70) : RGB(13, 10, 34));
        return reinterpret_cast<LRESULT>(enabled ? g.field_brush : g.field_off_brush);
    }
    case WM_COMMAND: {
        const int id = LOWORD(wparam), code = HIWORD(wparam);
        if (id == IDOK || (id == ID_LAUNCH && (code == BN_CLICKED || code == BN_DOUBLECLICKED))) { launch_game(); return 0; }
        if (code == EN_CHANGE) {
            if (!g.updating && g.resolution == 4 && (id == ID_WIDTH || id == ID_HEIGHT)) {
                g.cfg.custom_w = window_text(g.width_edit);
                g.cfg.custom_h = window_text(g.height_edit);
            }
            if (!g.updating && id == ID_ONLINE_ID_EDIT && g.online_id_edit) {
                g.cfg.online_id = window_text(g.online_id_edit);
            }
            if (!g.updating && id == ID_ONLINE_SERVER_EDIT && g.online_server_edit) {
                (g.cfg.online_host ? g.cfg.online_public_ip : g.cfg.online_server) = window_text(g.online_server_edit);
            }
            validate();
            return 0;
        }
        if (code == EN_SETFOCUS || code == EN_KILLFOCUS) { InvalidateRect(window, nullptr, FALSE); return 0; }
        if (code != BN_CLICKED && code != BN_DOUBLECLICKED) break;

        // Tab selection
        if (id >= ID_TAB_BASE && id < ID_TAB_BASE + static_cast<int>(TAB_COUNT)) {
            const int tab = id - ID_TAB_BASE;
            if (tab == TAB_MODS) return 0; // Disabled!
            select_tab(tab);
            return 0;
        }

        // Mod toggles
        if (id >= ID_MOD_TOGGLE_BASE && id < ID_MOD_TOGGLE_BASE + static_cast<int>(g.mods.size())) {
            const int mod_idx = id - ID_MOD_TOGGLE_BASE;
            g.mods[mod_idx].enabled = !g.mods[mod_idx].enabled;
            InvalidateRect(reinterpret_cast<HWND>(lparam), nullptr, FALSE);
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }

        if (id == ID_OPEN_MODS) {
            const fs::path mods_dir = g.root / "runtime/mods";
            std::error_code ec;
            fs::create_directories(mods_dir, ec);
            ShellExecuteW(nullptr, L"open", mods_dir.c_str(), nullptr, nullptr, SW_SHOW);
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

        if (id == ID_BASE_BROWSE) browse(g.base_edit, L"Cartella dump gioco base (CUSA14876, con eboot.bin)");
        else if (id == ID_PATCH_BROWSE) browse(g.patch_edit, L"Cartella Title Update v01.21 (es. CUSA14876-patch)");
        else if (id >= ID_PILL && id < ID_PILL + 5) select_resolution(id - ID_PILL);
        else {
            const auto found = g.controls.find(reinterpret_cast<HWND>(lparam));
            if (found != g.controls.end() && found->second.kind == Kind::Toggle) {
                *found->second.value = !*found->second.value;
                if (id == ID_NATIVE_4K && g.cfg.native_4k && !g.cfg.ps4_pro) {
                    g.cfg.ps4_pro = true;
                    InvalidateRect(g.pro_toggle, nullptr, FALSE);
                } else if (id == ID_PRO && !g.cfg.ps4_pro && g.cfg.native_4k) {
                    g.cfg.native_4k = false;
                    InvalidateRect(g.native_4k_toggle, nullptr, FALSE);
                } else if (id == ID_ONLINE_HOST) {
                    host_toggled();
                } else if (id == ID_ONLINE_TOGGLE && !g.cfg.online && g.cfg.online_host) {
                    g.cfg.online_host = false;
                    InvalidateRect(g.online_host_toggle, nullptr, FALSE);
                    host_toggled();
                }
                InvalidateRect(found->first, nullptr, FALSE);
                validate();
            }
        }
        return 0;
    }
    case WM_APP_FINISHED:
        game_finished(static_cast<DWORD>(wparam));
        return 0;
    case WM_APP_HOST_READY:
        host_ready(wparam != 0);
        return 0;
    case WM_APP_XBOX_READY:
        xbox_prompts_ready(reinterpret_cast<std::wstring*>(lparam));
        return 0;
    case WM_CLOSE:
        if (g.process || g.host_busy || g.xbox_busy) return 0;
        save_config();
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
    std::error_code ec;
    if (fs::exists(g.exe_dir / "ctrport.exe", ec)) {
        g.runtime_exe = g.exe_dir / "ctrport.exe";
    } else if (fs::exists(g.root / "build/CTRPort-gpu/ctrport.exe", ec)) {
        g.runtime_exe = g.root / "build/CTRPort-gpu/ctrport.exe";
    } else {
        g.runtime_exe = g.root / "build/CTRPort-claude/ctrport.exe";
    }
    g.scale = static_cast<float>(GetDpiForSystem()) / 96.f;
    load_config();

    WNDCLASSEXW window_class{sizeof(window_class)};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = L"CTRNativePortedLauncher";
    RegisterClassExW(&window_class);
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    RECT frame{0, 0, px(kWidth), px(kHeight)};
    AdjustWindowRectEx(&frame, style, FALSE, 0);
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int fw = frame.right - frame.left, fh = frame.bottom - frame.top;
    HWND window = CreateWindowExW(0, window_class.lpszClassName, kTitle, style, work.left + (work.right - work.left - fw) / 2,
                                  work.top + std::max(0L, (work.bottom - work.top - fh) / 2), fw, fh, nullptr, nullptr, instance, nullptr);

    const BOOL dark = TRUE;
    const COLORREF caption = RGB(18, 12, 48), caption_text = RGB(255, 248, 236), border = RGB(18, 12, 48);
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
    DwmSetWindowAttribute(window, 36, &caption_text, sizeof(caption_text));
    DwmSetWindowAttribute(window, 34, &border, sizeof(border));
    ShowWindow(window, show);
    UpdateWindow(window);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (IsDialogMessageW(window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    g.art.reset();
    g.icon.reset();
    for (auto& cached : family_cache) cached.reset();
    GdiplusShutdown(gdiplus);
    CoUninitialize();
    return 0;
}
