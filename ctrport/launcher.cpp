// SPDX-License-Identifier: GPL-2.0-only
// Crash Team Racing Native Ported: launcher (CTRNativePorted.exe), native Win32 + GDI+,
// so players need no Python. It replaces ctrport/launcher.py and keeps its settings file
// (runtime/user/launcher_config.json, same keys), then starts ctrport.exe exactly as
// ctrport/play_gpu.py does: same command line, environment, logs in runtime/user/logs,
// save copy before an unlock-all run, shared lane lock in development checkouts.
// No game artwork is part of this file or of the repository: the cover (sce_sys/pic1.png)
// and icon (sce_sys/icon0.png) are read at run time from the dump the user selects.
#ifndef UNICODE
#define UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
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
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;
using namespace Gdiplus;

namespace {
constexpr wchar_t kTitle[] = L"Crash Team Racing Native Ported";
constexpr float kWidth = 920, kHeight = 680, kHero = 258;
constexpr UINT WM_APP_FINISHED = WM_APP + 1;

// Palette taken from the game's logo colours: gold letters, red ribbon, night-blue lettering.
constexpr ARGB kNight = 0xFF0B1530, kNightTop = 0xFF10204A, kCard = 0xEE152853, kCardLine = 0xFF2C4A8F;
constexpr ARGB kField = 0xFF0C1735, kFieldOff = 0xFF0A1228, kGold = 0xFFFFC21A, kGoldLight = 0xFFFFE45C;
constexpr ARGB kOrange = 0xFFFF8A00, kRed = 0xFFE3262C, kRedDark = 0xFFA70F18, kText = 0xFFF5F7FF;
constexpr ARGB kMuted = 0xFF9FB0D6, kOk = 0xFF59E08B, kError = 0xFFFF6B6B;

const wchar_t* const kResolutions[] = {L"1280x720 (720p)", L"1920x1080 (1080p)", L"2560x1440 (1440p)", L"3840x2160 (4K)", L"Custom"};
const wchar_t* const kPills[] = {L"720p", L"1080p", L"1440p", L"4K", L"Custom"};
const int kPresetSize[][2] = {{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}};

enum : int {
    ID_BASE_EDIT = 101, ID_BASE_BROWSE, ID_PATCH_TOGGLE, ID_PATCH_EDIT, ID_PATCH_BROWSE,
    ID_PILL = 110, ID_WIDTH = 120, ID_HEIGHT, ID_FULLSCREEN, ID_FPS60, ID_PRO, ID_NATIVE_4K, ID_UNLOCK, ID_LAUNCH
};

struct Config {
    std::wstring base_dir, patch_dir, resolution = L"1920x1080 (1080p)", custom_w = L"1920", custom_h = L"1080";
    bool use_patch = false, fps_60 = true, ps4_pro = false, native_4k = false, unlock_all = false, fullscreen = false;
};

// ---- settings file: flat JSON object, compatible with the Python launcher's json.dump ----
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

// ---- process helpers ----
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

// Same exclusive lock file as tools/lane_lock.py ('shared', lane 'claude'), only in a
// development checkout (a coordination/ folder next to runtime/).
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
                MessageBoxW(owner, (L"Unable to create lock file " + path.wstring()).c_str(), kTitle, MB_ICONERROR);
                return false;
            }
            const std::wstring text = L"The runtime is already in use by another process (shared lane lock):\n\n" +
                widen(read_tail(path, 4096)) + L"\n\nRetry?";
            if (MessageBoxW(owner, text.c_str(), kTitle, MB_RETRYCANCEL | MB_ICONWARNING) != IDRETRY) return false;
        }
    }
    void release() {
        if (!held) return;
        held = false;
        if (read_tail(path, 4096).find(token) != std::string::npos) DeleteFileW(path.c_str());
    }
};

// ---- application state ----
enum class Kind { Browse, Toggle, Pill, Launch };
struct Control { Kind kind; const wchar_t* label; bool* value; int pill; RectF box; };
struct Status { std::wstring text; int kind = 0; };  // 0 neutral, 1 ok, 2 error

struct App {
    HWND window = nullptr, base_edit = nullptr, patch_edit = nullptr, width_edit = nullptr, height_edit = nullptr;
    HWND patch_browse = nullptr, fps_toggle = nullptr, pro_toggle = nullptr, native_4k_toggle = nullptr, launch = nullptr, hot = nullptr;
    std::vector<HWND> pills;
    std::map<HWND, Control> controls;
    float scale = 1;
    fs::path exe_dir, root, runtime_exe;
    Config cfg;
    int resolution = 1;
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

// Layout in logical pixels (96 DPI); scaled at creation and drawing time.
const RectF kFilesCard = rect(24, 286, 476, 262), kVideoCard = rect(516, 286, 380, 186), kTestCard = rect(516, 482, 380, 66);
const RectF kBaseField = rect(44, 350, 332, 34), kPatchField = rect(44, 450, 332, 34);
const RectF kWidthField = rect(536, 370, 84, 30), kHeightField = rect(644, 370, 84, 30);
const RectF kLaunch = rect(24, 566, 872, 56);

fs::path config_path() { return (g.root.empty() ? g.exe_dir : g.root / "runtime/user") / "launcher_config.json"; }

// Relative dump folders are resolved against the launcher's root, so a portable copy with
// dump\CUSA14876 and dump\CUSA14876-patch beside it works wherever it is moved.
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
    // Default locations: the dump folder next to the launcher. Saved settings override them.
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
            flag(L"use_patch", g.cfg.use_patch); flag(L"fps_60", g.cfg.fps_60); flag(L"ps4_pro", g.cfg.ps4_pro);
            flag(L"native_4k", g.cfg.native_4k); flag(L"unlock_all", g.cfg.unlock_all); flag(L"fullscreen", g.cfg.fullscreen);
        }
    }
    if (g.cfg.native_4k) g.cfg.ps4_pro = true;
    for (std::wstring* folder : {&g.cfg.base_dir, &g.cfg.patch_dir})
        if (!folder->empty()) *folder = fs::path(*folder).make_preferred().wstring();
    g.resolution = 1;
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
    const auto flag = [](bool v) { return std::string(v ? "true" : "false"); };
    const std::string json = "{\n  \"base_dir\": " + json_string(g.cfg.base_dir) + ",\n  \"patch_dir\": " + json_string(g.cfg.patch_dir) +
        ",\n  \"use_patch\": " + flag(g.cfg.use_patch) + ",\n  \"fps_60\": " + flag(g.cfg.fps_60) + ",\n  \"ps4_pro\": " + flag(g.cfg.ps4_pro) +
        ",\n  \"native_4k\": " + flag(g.cfg.native_4k) + ",\n  \"unlock_all\": " + flag(g.cfg.unlock_all) + ",\n  \"fullscreen\": " + flag(g.cfg.fullscreen) +
        ",\n  \"resolution\": " + json_string(g.cfg.resolution) + ",\n  \"custom_w\": " + json_string(g.cfg.custom_w) +
        ",\n  \"custom_h\": " + json_string(g.cfg.custom_h) + "\n}";
    std::error_code ec;
    fs::create_directories(config_path().parent_path(), ec);
    std::ofstream(config_path(), std::ios::binary | std::ios::trunc) << json;
}

// ---- drawing helpers ----
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
std::unique_ptr<FontFamily> family_cache[4];
const FontFamily& family(int which) {
    // 0 headline (heavy), 1 text, 2 text semibold, 3 condensed labels
    static const wchar_t* const names[4][3] = {{L"Segoe UI Black", L"Arial Black", L"Segoe UI"},
                                               {L"Segoe UI", L"Tahoma", L"Arial"},
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
// Heavy families such as Segoe UI Black have no bold face; fall back to an existing style.
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
// Logo-style lettering: dark outline, then a vertical gold gradient fill.
void logo_text(Graphics& gr, const wchar_t* value, float size, PointF origin, ARGB top, ARGB bottom, ARGB outline, float outline_width) {
    GraphicsPath path;
    StringFormat format;
    const int style = usable_style(family(0), FontStyleBoldItalic);
    path.AddString(value, -1, &family(0), style, size, origin, &format);
    RectF bounds;
    path.GetBounds(&bounds);
    GraphicsPath shadow;
    shadow.AddString(value, -1, &family(0), style, size, PointF(origin.X + 3, origin.Y + 4), &format);
    SolidBrush shadow_brush{Color(110, 0, 0, 0)};
    gr.FillPath(&shadow_brush, &shadow);
    Pen pen(Color(outline), outline_width);
    pen.SetLineJoin(LineJoinRound);
    gr.DrawPath(&pen, &path);
    LinearGradientBrush fill(PointF(0, bounds.Y - 1), PointF(0, bounds.Y + bounds.Height + 1), Color(top), Color(bottom));
    fill.SetWrapMode(WrapModeTileFlipXY);
    gr.FillPath(&fill, &path);
}
void draw_cover(Graphics& gr, Bitmap& image, const RectF& dest) {
    const float iw = static_cast<float>(image.GetWidth()), ih = static_cast<float>(image.GetHeight());
    const float ratio = std::max(dest.Width / iw, dest.Height / ih);
    const float sw = dest.Width / ratio, sh = dest.Height / ratio;
    ImageAttributes attributes;
    attributes.SetWrapMode(WrapModeTileFlipXY);
    gr.DrawImage(&image, dest, (iw - sw) / 2, 0, sw, sh, UnitPixel, &attributes);
}
void checkered(Graphics& gr, float y, float cell, int rows) {
    SolidBrush white{Color(0xFFF2F4FA)}, dark{Color(0xFF080F24)};
    for (int row = 0; row < rows; ++row)
        for (int column = 0; column * cell < kWidth; ++column)
            gr.FillRectangle(((row + column) & 1) ? &dark : &white, column * cell, y + row * cell, cell, cell);
}
void card(Graphics& gr, const RectF& r, const wchar_t* title) {
    fill_round(gr, r, 14, SolidBrush(Color(kCard)));
    stroke_round(gr, r, 14, fade(kCardLine, 0.8f), 1);
    if (!title) return;
    fill_round(gr, rect(r.X + 20, r.Y + 16, 4, 16), 2, SolidBrush(Color(kGold)));
    text(gr, title, 3, 14, FontStyleRegular, rect(r.X + 30, r.Y + 13, r.Width - 50, 22), kGold);
}

// Static layer: artwork, title, cards and labels. Owner-drawn controls copy their
// background from it, so it is rebuilt only when the artwork or the DPI changes.
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
        LinearGradientBrush body(PointF(0, kHero), PointF(0, kHeight), Color(kNightTop), Color(kNight));
        body.SetWrapMode(WrapModeTileFlipXY);
        gr.FillRectangle(&body, 0.f, kHero - 1, kWidth, kHeight - kHero + 1);
        // Hero: the user's own cover art, or a sky gradient when no dump is selected.
        const RectF hero = rect(0, 0, kWidth, kHero);
        if (g.art) {
            draw_cover(gr, *g.art, hero);
        } else {
            LinearGradientBrush sky(PointF(0, 0), PointF(0, kHero), Color(0xFF3A8EE6), Color(0xFF13306E));
            sky.SetWrapMode(WrapModeTileFlipXY);
            gr.FillRectangle(&sky, hero);
            for (int i = 0; i < 6; ++i) {
                const float x = 470.f + i * 70.f;
                PointF stripe[] = {PointF(x, 0), PointF(x + 34, 0), PointF(x - 96, kHero), PointF(x - 130, kHero)};
                SolidBrush light{Color(static_cast<BYTE>(18 + i * 4), 255, 214, 90)};
                gr.FillPolygon(&light, stripe, 4);
            }
        }
        LinearGradientBrush fade_down(PointF(0, 110), PointF(0, kHero), Color(0, 11, 21, 48), Color(250, 11, 21, 48));
        fade_down.SetWrapMode(WrapModeTileFlipXY);
        gr.FillRectangle(&fade_down, 0.f, 110.f, kWidth, kHero - 110);
        LinearGradientBrush fade_left(PointF(0, 0), PointF(640, 0), Color(190, 11, 21, 48), Color(0, 11, 21, 48));
        fade_left.SetWrapMode(WrapModeTileFlipXY);
        gr.FillRectangle(&fade_left, 0.f, 120.f, 640.f, kHero - 120);
        // Title: red ribbon like the game's logo banner, then gold lettering.
        PointF ribbon[] = {PointF(34, 136), PointF(290, 136), PointF(282, 164), PointF(26, 164)};
        LinearGradientBrush ribbon_fill(PointF(0, 136), PointF(0, 164), Color(kRed), Color(kRedDark));
        ribbon_fill.SetWrapMode(WrapModeTileFlipXY);
        gr.FillPolygon(&ribbon_fill, ribbon, 4);
        Pen ribbon_edge(Color(0xFFFFFFFF), 1.5f);
        gr.DrawPolygon(&ribbon_edge, ribbon, 4);
        text(gr, L"CRASH TEAM RACING", 0, 17, FontStyleBoldItalic, rect(26, 136, 264, 28), kText, StringAlignmentCenter);
        logo_text(gr, L"NATIVE PORTED", 52, PointF(18, 158), kGoldLight, kOrange, 0xFF4A1000, 7);
        text(gr, L"Native Windows runtime · Play with your own CTR Nitro-Fueled PS4 dump", 2, 13.5f, FontStyleRegular,
             rect(30, 226, 620, 22), 0xF0FFFFFF);
        LinearGradientBrush accent(PointF(0, 0), PointF(kWidth, 0), Color(kGold), Color(kRed));
        accent.SetWrapMode(WrapModeTileFlipXY);
        gr.FillRectangle(&accent, 0.f, kHero - 3, kWidth, 3.f);
        checkered(gr, kHero, 5, 2);
        // Cards and fixed labels.
        card(gr, kFilesCard, L"GAME FILES");
        card(gr, kVideoCard, L"VIDEO");
        card(gr, kTestCard, nullptr);
        LinearGradientBrush tag(PointF(0, 501), PointF(0, 529), Color(kGoldLight), Color(kGold));
        tag.SetWrapMode(WrapModeTileFlipXY);
        fill_round(gr, rect(536, 502, 66, 26), 13, tag);
        text(gr, L"TEST", 3, 12.5f, FontStyleRegular, rect(536, 502, 66, 26), kNight, StringAlignmentCenter);
        text(gr, L"Base game · v01.00 (CUSA14876)", 2, 13.5f, FontStyleRegular, rect(44, 326, 436, 20), kText);
        text(gr, L"Your own dump is required: eboot.bin & sce_module. The update overlays the base game.", 1, 11.5f,
             FontStyleRegular, rect(44, 516, 436, 18), kMuted);
    }
    SelectObject(dc, previous);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

void draw_check(Graphics& gr, float x, float y, int kind) {
    const ARGB color = kind == 1 ? kOk : kError;
    SolidBrush circle{Color(fade(color, 0.2f))};
    gr.FillEllipse(&circle, x, y, 16.f, 16.f);
    Pen pen(Color(color), 2);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    if (kind == 1) {
        PointF tick[] = {PointF(x + 4.5f, y + 8.5f), PointF(x + 7, y + 11), PointF(x + 11.5f, y + 5.5f)};
        gr.DrawLines(&pen, tick, 3);
    } else {
        gr.DrawLine(&pen, x + 5, y + 5, x + 11, y + 11);
        gr.DrawLine(&pen, x + 11, y + 5, x + 5, y + 11);
    }
}
void draw_status(Graphics& gr, const Status& status, float x, float y, float w) {
    if (status.kind) draw_check(gr, x, y + 2, status.kind);
    const float offset = status.kind ? 22.f : 0.f;
    text(gr, status.text, 1, 12.5f, FontStyleRegular, rect(x + offset, y, w - offset, 20),
         status.kind == 1 ? kOk : status.kind == 2 ? kError : kMuted);
}
void draw_field(Graphics& gr, const RectF& r, HWND edit) {
    const bool enabled = IsWindowEnabled(edit) != FALSE, focused = GetFocus() == edit;
    fill_round(gr, r, 8, SolidBrush(Color(enabled ? kField : kFieldOff)));
    stroke_round(gr, r, 8, focused ? kGold : enabled ? kCardLine : fade(kCardLine, 0.45f), focused ? 1.6f : 1.f);
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
        draw_field(gr, kBaseField, g.base_edit);
        draw_field(gr, kPatchField, g.patch_edit);
        draw_field(gr, kWidthField, g.width_edit);
        draw_field(gr, kHeightField, g.height_edit);
        const bool size_on = IsWindowEnabled(g.width_edit) != FALSE;
        text(gr, L"×", 2, 16, FontStyleRegular, rect(620, 370, 24, 30), size_on ? kText : fade(kMuted, 0.6f), StringAlignmentCenter);
        text(gr, L"window pixels", 1, 12, FontStyleRegular, rect(738, 370, 150, 30), size_on ? kMuted : fade(kMuted, 0.6f));
        draw_status(gr, g.base_status, 44, 390, 436);
        draw_status(gr, g.patch_status, 44, 490, 436);
        const float status_width = std::min(860.f, 30.f + 7.2f * static_cast<float>(g.global.text.size()));
        draw_status(gr, g.global, (kWidth - status_width) / 2, 630, status_width);
        text(gr, L"60 FPS: '60 FPS Unlock' patch by illusion (GoldHEN Patch Repository) · Crash Team Racing Native Ported is GPL-2.0 software",
             1, 11, FontStyleRegular, rect(24, 654, 872, 18), fade(kMuted, 0.85f), StringAlignmentCenter);
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
        const RectF full = rect(0.5f, 0.5f + (pressed ? 1.f : 0.f), lw - 1, lh - 1);
        switch (control.kind) {
        case Kind::Browse:
            fill_round(gr, full, 8, SolidBrush(Color(fade(hot ? 0xFF2D4C93 : 0xFF213A75, alpha))));
            stroke_round(gr, full, 8, fade(kGold, hot ? alpha : 0.55f * alpha), 1.2f);
            text(gr, control.label, 3, 13, FontStyleRegular, full, fade(kText, alpha), StringAlignmentCenter);
            break;
        case Kind::Toggle: {
            const bool on = *control.value;
            const RectF track = rect(1, (lh - 20) / 2, 38, 20);
            if (on) {
                LinearGradientBrush fill(PointF(0, track.Y), PointF(0, track.Y + 20), Color(fade(kGoldLight, alpha)), Color(fade(kOrange, alpha)));
                fill.SetWrapMode(WrapModeTileFlipXY);
                fill_round(gr, track, 10, fill);
            } else {
                fill_round(gr, track, 10, SolidBrush(Color(fade(kField, alpha))));
                stroke_round(gr, track, 10, fade(hot ? kGold : 0xFF3A5597, alpha), 1.2f);
            }
            SolidBrush knob{Color(fade(on ? 0xFFFFFFFF : 0xFF8EA2CF, alpha))};
            gr.FillEllipse(&knob, on ? track.X + 20 : track.X + 3, track.Y + 3, 14.f, 14.f);
            text(gr, control.label, 2, 13.5f, FontStyleRegular, rect(48, 0, lw - 48, lh), fade(hot ? 0xFFFFFFFF : kText, alpha));
            break;
        }
        case Kind::Pill: {
            const bool selected = g.resolution == control.pill;
            if (selected) {
                LinearGradientBrush fill(PointF(0, 0), PointF(0, lh), Color(fade(kGoldLight, alpha)), Color(fade(kGold, alpha)));
                fill.SetWrapMode(WrapModeTileFlipXY);
                fill_round(gr, full, lh / 2 - 1, fill);
            } else {
                fill_round(gr, full, lh / 2 - 1, SolidBrush(Color(fade(kField, alpha))));
                stroke_round(gr, full, lh / 2 - 1, fade(hot ? kGold : kCardLine, alpha), 1.2f);
            }
            text(gr, control.label, 3, 13, FontStyleRegular, full, selected ? fade(kNight, alpha) : fade(0xFFCFD8F0, alpha), StringAlignmentCenter);
            break;
        }
        case Kind::Launch: {
            if (disabled) {
                LinearGradientBrush fill(PointF(0, 0), PointF(0, lh), Color(0xFF34436A), Color(0xFF26324F));
                fill.SetWrapMode(WrapModeTileFlipXY);
                fill_round(gr, full, 12, fill);
            } else {
                LinearGradientBrush fill(PointF(0, 0), PointF(0, lh), Color(hot ? 0xFFFFF07A : kGoldLight), Color(hot ? 0xFFFF9A1A : kOrange));
                fill.SetWrapMode(WrapModeTileFlipXY);
                const Color stops_colors[] = {Color(hot ? 0xFFFFF07A : kGoldLight), Color(hot ? 0xFFFFC93A : kGold), Color(hot ? 0xFFFF9A1A : kOrange)};
                const REAL stops[] = {0.f, 0.45f, 1.f};
                fill.SetInterpolationColors(stops_colors, stops, 3);
                fill_round(gr, full, 12, fill);
                stroke_round(gr, full, 12, 0xFF7A2A00, 1.5f);
            }
            const ARGB ink = disabled ? 0xFF7F8CB0 : kNight;
            const float cx = lw / 2 - 120;
            PointF arrow[] = {PointF(cx, lh / 2 - 11), PointF(cx + 18, lh / 2), PointF(cx, lh / 2 + 11)};
            SolidBrush arrow_brush{Color(ink)};
            gr.FillPolygon(&arrow_brush, arrow, 3);
            text(gr, L"LAUNCH GAME", 0, 26, FontStyleBoldItalic, rect(cx + 26, 0, 260, lh), ink);
            break;
        }
        }
        if ((item.itemState & ODS_FOCUS) && !(item.itemState & ODS_NOFOCUSRECT))
            stroke_round(gr, rect(1, 1, lw - 2, lh - 2), control.kind == Kind::Launch ? 12.f : 8.f, 0xB0FFFFFF, 1.5f);
    }
    BitBlt(item.hDC, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, previous);
    DeleteObject(buffer);
    DeleteDC(dc);
}

// ---- artwork from the selected dump ----
std::unique_ptr<Bitmap> load_png(const fs::path& path) {
    std::unique_ptr<Bitmap> file(Bitmap::FromFile(path.c_str()));
    if (!file || file->GetLastStatus() != Ok) return nullptr;
    // Copy so the file is not kept open.
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

// ---- validation ----
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
    g.base_status = !base_eboot ? Status{L"eboot.bin not found in this folder", 2}
                  : !base_modules ? Status{L"Missing sce_module folder for base game", 2}
                                  : Status{L"Base game found (eboot.bin & sce_module)", 1};
    if (!g.cfg.use_patch) g.patch_status = {L"Update disabled: v01.00 will be launched", 0};
    else g.patch_status = patch_ok ? Status{L"Update v01.21 found (eboot.bin)", 1} : Status{L"Update folder or eboot.bin missing", 2};
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
    if (!runtime_ok) g.global = {L"Runtime not found: ctrport.exe and build/CTRPort-gpu/ctr_gpu.dll required", 2};
    else if (!base_modules || (g.cfg.use_patch && !patch_ok)) g.global = {L"Select valid dump folders to launch the game", 2};
    else if (!size_ok) g.global = {L"Invalid resolution (from 320×240 to 7680×4320)", 2};
    else {
        std::wstring summary = L"Ready · ";
        summary += g.cfg.use_patch ? L"v01.21" : L"v01.00";
        if (g.cfg.use_patch && g.cfg.fps_60) summary += L" · 60 FPS";
        summary += g.cfg.fullscreen ? L" · fullscreen" : L" · " + std::to_wstring(width) + L"×" + std::to_wstring(height);
        if (g.cfg.native_4k) summary += L" · PS4 Pro (Native 4K 3D)";
        else if (g.cfg.ps4_pro) summary += L" · PS4 Pro (1440p 3D)";
        if (g.cfg.unlock_all) summary += L" · unlock all (saves backed up)";
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

// ---- launching ctrport.exe (mirrors ctrport/play_gpu.py) ----
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

void launch_game() {
    validate();
    if (!g.can_launch || g.process) return;
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
            MessageBoxW(g.window, (L"Save backup failed, launch aborted:\n" + widen(ec.message())).c_str(), kTitle, MB_ICONERROR);
            g.lock.release();
            return;
        }
    }
    const fs::path base = resolve(g.cfg.base_dir), patch = resolve(g.cfg.patch_dir);
    const fs::path eboot = (g.cfg.use_patch ? patch : base) / "eboot.bin";
    std::vector<fs::path> modules;
    for (const auto& entry : fs::directory_iterator(base / "sce_module", ec))
        if (lower(entry.path().extension().wstring()) == L".prx") modules.push_back(entry.path());
    // Python's sorted() on Windows paths compares them lower-cased.
    std::sort(modules.begin(), modules.end(), [](const fs::path& a, const fs::path& b) { return lower(a.wstring()) < lower(b.wstring()); });
    std::wstring command = quote_argument(g.runtime_exe.wstring()) + L" --entry-after-libc-init-images " + quote_argument(eboot.wstring());
    for (const auto& module : modules) command += L" " + quote_argument(module.wstring());

    std::map<std::wstring, std::wstring> set{{L"CTR_GPU_LIBRARY", L"1"}, {L"CTR_TLS_PATCH", L"1"}, {L"CTR_GPU_LOG", L"error"},
                                             {L"CTR_NO_FRAME_OBSERVATION", L"1"}, {L"CTR_BASE_DIR", base.wstring()}};
    if (g.cfg.use_patch) set[L"CTR_PATCH_DIR"] = patch.wstring();
    if (g.cfg.use_patch && g.cfg.fps_60) set[L"CTR_60FPS_PATCH"] = L"1";
    if (g.cfg.ps4_pro || g.cfg.native_4k) set[L"CTR_NEO_MODE"] = L"1";
    if (g.cfg.native_4k) set[L"CTR_NATIVE_4K"] = L"1";
    if (g.cfg.unlock_all) set[L"CTR_UNLOCK_ALL"] = L"1";
    if (g.cfg.fullscreen) set[L"CTR_GPU_FULLSCREEN"] = L"1";
    else { set[L"CTR_GPU_WIDTH"] = window_text(g.width_edit); set[L"CTR_GPU_HEIGHT"] = window_text(g.height_edit); }
    auto environment = environment_block(set, {L"CTR_STARTUP_WRITE_DLL_DIR", L"CTR_STARTUP_MUTEX_DLL_DIR", L"CTR_PARK_MISSING_NID",
        L"CTR_PARK_MISSING_SERVICE", L"CTR_TILING_TIMING", L"CTR_60FPS_PATCH", L"CTR_NEO_MODE", L"CTR_NATIVE_4K", L"CTR_UNLOCK_ALL",
        L"CTR_GPU_FULLSCREEN", L"CTR_GPU_WIDTH", L"CTR_GPU_HEIGHT", L"CTR_PATCH_DIR", L"CTR_VERSION"});

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
        MessageBoxW(g.window, (L"Failed to launch ctrport.exe (error " + std::to_wstring(error) + L").").c_str(), kTitle, MB_ICONERROR);
        return;
    }
    CloseHandle(info.hThread);
    g.process = info.hProcess;
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
    // Same stop-reason scan as play_gpu.py.
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
    const bool closed_by_user = !reasons.empty() && reasons.back().find("window closed") != std::string::npos;
    if (code == 0 && (reasons.empty() || closed_by_user)) return;
    std::wstring message = L"The game has stopped (exit code " + std::to_wstring(code) + L").\n\n";
    if (reasons.empty()) message += L"No specific error found in logs.\n";
    for (std::size_t i = reasons.size() > 5 ? reasons.size() - 5 : 0; i < reasons.size(); ++i) message += widen(reasons[i]) + L"\n";
    message += L"\nLog: " + g.log_stem.wstring() + L".stdout.log";
    MessageBoxW(g.window, message.c_str(), kTitle, MB_ICONWARNING);
}

// ---- controls ----
LRESULT CALLBACK control_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    switch (message) {
    case WM_MOUSEMOVE:
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
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefSubclassProc(window, message, wparam, lparam);
}
HWND add_button(int id, Kind kind, const RectF& box, const wchar_t* label, bool* value = nullptr, int pill = -1) {
    HWND button = CreateWindowExW(0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, px(box.X), px(box.Y),
                                  px(box.Width), px(box.Height), g.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SetWindowSubclass(button, control_proc, 1, 0);
    g.controls[button] = Control{kind, label, value, pill, box};
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
    g.base_edit = add_edit(ID_BASE_EDIT, kBaseField, g.cfg.base_dir);
    add_button(ID_BASE_BROWSE, Kind::Browse, rect(384, 350, 96, 34), L"BROWSE");
    add_button(ID_PATCH_TOGGLE, Kind::Toggle, rect(44, 418, 436, 26), L"Title Update v01.21 (new tracks, karts, Grand Prix)", &g.cfg.use_patch);
    g.patch_edit = add_edit(ID_PATCH_EDIT, kPatchField, g.cfg.patch_dir);
    g.patch_browse = add_button(ID_PATCH_BROWSE, Kind::Browse, rect(384, 450, 96, 34), L"BROWSE");
    for (int i = 0; i < 5; ++i)
        g.pills.push_back(add_button(ID_PILL + i, Kind::Pill, rect(536 + i * 69.f, 330, 62, 30), kPills[i], nullptr, i));
    g.width_edit = add_edit(ID_WIDTH, kWidthField, g.cfg.custom_w, ES_NUMBER | ES_CENTER);
    g.height_edit = add_edit(ID_HEIGHT, kHeightField, g.cfg.custom_h, ES_NUMBER | ES_CENTER);
    add_button(ID_FULLSCREEN, Kind::Toggle, rect(536, 410, 150, 26), L"Fullscreen", &g.cfg.fullscreen);
    g.fps_toggle = add_button(ID_FPS60, Kind::Toggle, rect(692, 410, 194, 26), L"60 FPS (v01.21)", &g.cfg.fps_60);
    g.pro_toggle = add_button(ID_PRO, Kind::Toggle, rect(536, 438, 172, 26), L"PS4 Pro (1440p 3D)", &g.cfg.ps4_pro);
    g.native_4k_toggle = add_button(ID_NATIVE_4K, Kind::Toggle, rect(712, 438, 174, 26), L"Native 4K 3D", &g.cfg.native_4k);
    add_button(ID_UNLOCK, Kind::Toggle, rect(616, 502, 270, 26), L"Unlock all items & tracks", &g.cfg.unlock_all);
    g.launch = add_button(ID_LAUNCH, Kind::Launch, kLaunch, L"LAUNCH GAME");
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        g.window = window;
        g.field_brush = CreateSolidBrush(RGB(12, 23, 53));
        g.field_off_brush = CreateSolidBrush(RGB(10, 18, 40));
        render_base_layer();
        // Edits report EN_CHANGE while being created; validate only once all controls exist.
        g.updating = true;
        create_controls();
        g.updating = false;
        select_resolution(g.resolution);
        SetFocus(IsWindowEnabled(g.launch) ? g.launch : g.base_edit);
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
        SetTextColor(dc, enabled ? RGB(245, 247, 255) : RGB(110, 124, 160));
        SetBkColor(dc, enabled ? RGB(12, 23, 53) : RGB(10, 18, 40));
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
            validate();
            return 0;
        }
        if (code == EN_SETFOCUS || code == EN_KILLFOCUS) { InvalidateRect(window, nullptr, FALSE); return 0; }
        if (code != BN_CLICKED && code != BN_DOUBLECLICKED) break;
        if (id == ID_BASE_BROWSE) browse(g.base_edit, L"Base game dump folder (CUSA14876, with eboot.bin)");
        else if (id == ID_PATCH_BROWSE) browse(g.patch_edit, L"Title Update v01.21 folder (e.g. CUSA14876-patch)");
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
    case WM_CLOSE:
        if (g.process) return 0;
        save_config();
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
        if (fs::exists(folder / "build/CTRPort-gpu/ctr_gpu.dll", ec)) return folder;
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
    g.runtime_exe = fs::exists(g.exe_dir / "ctrport.exe", ec) ? g.exe_dir / "ctrport.exe" : g.root / "build/CTRPort-claude/ctrport.exe";
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
    // Night-blue title bar and gold border on Windows 11 (ignored where unsupported).
    const BOOL dark = TRUE;
    const COLORREF caption = RGB(11, 21, 48), caption_text = RGB(245, 247, 255), border = RGB(255, 194, 26);
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
