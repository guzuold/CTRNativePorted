// SPDX-License-Identifier: GPL-2.0-only
// Advanced tab, KEYBOARD section: the key of each of the 24 pad inputs when the keyboard plays
// (keyboard_keys.hpp). Click an input's key, then press the new one (Esc cancels); a key taken
// from another input leaves that one without a key. Saved at once to
// runtime/user/keyboard_map.txt, which the launcher passes to the game as CTR_KEYBOARD_MAP: the
// runtime reads the keyboard itself, so the map works with every game version and renderer, from
// the next start of the game. Reset keys deletes the file. The tab scrolls with the mouse wheel.
// Coordinates are the Classic page's (x 296..990); Nitro maps them with page_rect.
#pragma once
#include "keyboard_keys.hpp"

namespace keyboard_section {
namespace keys = ctrport::keyboard_keys;

constexpr int kFirstId = 1400, kResetId = 1430;
constexpr float kTop = 600, kRows = 74, kRow = 32, kColumn[2] = {312, 652}, kLabel = 170, kButton = 150;
inline const wchar_t* const kLabels[keys::count] = {L"Cross",
                                                    L"Circle",
                                                    L"Square",
                                                    L"Triangle",
                                                    L"L1",
                                                    L"R1",
                                                    L"L2",
                                                    L"R2",
                                                    L"L3",
                                                    L"R3",
                                                    L"Options",
                                                    L"Touch pad",
                                                    L"D-pad up",
                                                    L"D-pad down",
                                                    L"D-pad left",
                                                    L"D-pad right",
                                                    L"Left stick up",
                                                    L"Left stick down",
                                                    L"Left stick left",
                                                    L"Left stick right",
                                                    L"Right stick up",
                                                    L"Right stick down",
                                                    L"Right stick left",
                                                    L"Right stick right"};

inline std::array<std::vector<int>, keys::count> map;
inline std::array<std::wstring, keys::count> captions; // the buttons' labels (Control::label points here)
inline std::array<HWND, keys::count> buttons{};
inline int capturing = -1; // the input waiting for its new key
inline WPARAM swallow = 0; // the key that ended a capture: its release must not click the button
struct Child {
    HWND hwnd;
    RECT base; // client position at scroll 0
};
inline std::vector<Child> children; // every Advanced-tab child window, moved when the tab scrolls
inline float bottom = 0;

inline fs::path file() {
    return g.root / "runtime/user/keyboard_map.txt";
}
inline bool owns(int id) {
    return (id >= kFirstId && id < kFirstId + keys::count) || id == kResetId;
}
inline RectF box(float x, float y, float w, float h) {
    return classic_skin ? rect(x, y, w, h) : page_rect(x, y, w, h);
}
inline float view_top() {
    return classic_skin ? 56.f : nitro::header - 40.f;
}
inline float view_bottom() {
    return classic_skin ? 616.f : nitro::footer - 40.f;
}
inline float max_scroll() {
    return std::max(0.f, bottom + 20.f - view_bottom());
}

// The defaults, then the lines of the saved file (as the runtime reads them).
inline void load() {
    for (int i = 0; i < keys::count; ++i) map[size_t(i)] = {keys::defaults[i]};
    std::ifstream in(file());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        for (int i = 0; i < keys::count; ++i) {
            if (line.compare(0, eq, keys::names[i]) != 0 || std::strlen(keys::names[i]) != eq) continue;
            std::vector<int> codes;
            std::stringstream list(line.substr(eq + 1));
            std::string key;
            bool bad = false;
            while (std::getline(list, key, ',')) {
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                if (key.empty()) continue;
                if (const int code = keys::key_code(key))
                    codes.push_back(code);
                else
                    bad = true;
            }
            if (!bad) map[size_t(i)] = codes;
        }
    }
}
inline bool save() {
    std::error_code ec;
    fs::create_directories(file().parent_path(), ec);
    std::ofstream out(file(), std::ios::binary | std::ios::trunc);
    out << "# Keyboard keys of the pad inputs, written by the launcher (Advanced tab, KEYBOARD).\n";
    for (int i = 0; i < keys::count; ++i) {
        out << keys::names[i] << '=';
        for (size_t k = 0; k < map[size_t(i)].size(); ++k) out << (k ? "," : "") << keys::key_name(map[size_t(i)][k]);
        out << '\n';
    }
    out.close();
    return static_cast<bool>(out);
}

// What the button shows for a key: the character of the user's layout for letters, digits and
// punctuation, a name for the others.
inline std::wstring key_label(int code) {
    if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9'))
        return std::wstring(1, static_cast<wchar_t>(code));
    if (code >= VK_F1 && code <= VK_F24) return L"F" + std::to_wstring(code - VK_F1 + 1);
    if (code >= VK_NUMPAD0 && code <= VK_NUMPAD9) return L"Num " + std::to_wstring(code - VK_NUMPAD0);
    static const std::pair<int, const wchar_t*> names[] = {
        {VK_SPACE, L"Space"},         {VK_RETURN, L"Enter"},      {VK_TAB, L"Tab"},
        {VK_BACK, L"Backspace"},      {VK_SHIFT, L"Shift"},       {VK_LSHIFT, L"Left Shift"},
        {VK_RSHIFT, L"Right Shift"},  {VK_CONTROL, L"Ctrl"},      {VK_LCONTROL, L"Left Ctrl"},
        {VK_RCONTROL, L"Right Ctrl"}, {VK_MENU, L"Alt"},          {VK_LMENU, L"Left Alt"},
        {VK_RMENU, L"Right Alt"},     {VK_UP, L"Up arrow"},       {VK_DOWN, L"Down arrow"},
        {VK_LEFT, L"Left arrow"},     {VK_RIGHT, L"Right arrow"}, {VK_INSERT, L"Insert"},
        {VK_DELETE, L"Delete"},       {VK_HOME, L"Home"},         {VK_END, L"End"},
        {VK_PRIOR, L"Page Up"},       {VK_NEXT, L"Page Down"},    {VK_CAPITAL, L"Caps Lock"},
        {VK_MULTIPLY, L"Num *"},      {VK_ADD, L"Num +"},         {VK_SUBTRACT, L"Num -"},
        {VK_DECIMAL, L"Num ."},       {VK_DIVIDE, L"Num /"}};
    for (const auto& [c, name] : names)
        if (c == code) return tr(name);
    if (const auto ch = static_cast<wchar_t>(MapVirtualKeyW(static_cast<UINT>(code), MAPVK_VK_TO_CHAR) & 0xffff))
        return std::wstring(1, ch);
    return widen(keys::key_name(code));
}
inline void refresh() {
    for (int i = 0; i < keys::count; ++i) {
        auto& caption = captions[size_t(i)];
        if (capturing == i)
            caption = tr(L"Press a key (Esc cancels)");
        else {
            caption.clear();
            for (int code : map[size_t(i)]) caption += (caption.empty() ? L"" : L" / ") + key_label(code);
            if (caption.empty()) caption = tr(L"(none)");
        }
        if (const auto found = g.controls.find(buttons[size_t(i)]); found != g.controls.end())
            found->second.label = caption.c_str();
        InvalidateRect(buttons[size_t(i)], nullptr, FALSE);
    }
}

// A key pressed while an input waits for one. Left and right Shift, Ctrl and Alt are told apart.
inline void key(WPARAM raw, LPARAM lparam) {
    swallow = raw;
    if (raw == VK_ESCAPE) {
        capturing = -1;
        refresh();
        return;
    }
    int code = static_cast<int>(raw);
    if (raw == VK_SHIFT)
        code = static_cast<int>(MapVirtualKeyW((lparam >> 16) & 0xff, MAPVK_VSC_TO_VK_EX));
    else if (raw == VK_CONTROL)
        code = (lparam & (1 << 24)) ? VK_RCONTROL : VK_LCONTROL;
    else if (raw == VK_MENU)
        code = (lparam & (1 << 24)) ? VK_RMENU : VK_LMENU;
    if (keys::key_name(code).empty()) {
        swallow = 0;
        return;
    } // a key the map cannot name: keep waiting
    for (auto& other : map) other.erase(std::remove(other.begin(), other.end(), code), other.end());
    map[size_t(capturing)] = {code};
    capturing = -1;
    if (!save()) MessageBoxW(g.window, file().wstring().c_str(), tr(L"Keyboard keys could not be saved"), MB_ICONERROR);
    refresh();
}

// While an input waits, its button takes every key (the dialog manager would use Tab, Enter
// and the arrows), and loses the wait with the focus.
inline LRESULT CALLBACK key_proc(HWND h, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR index) {
    if (capturing == int(index)) {
        if (msg == WM_GETDLGCODE) return DLGC_WANTALLKEYS | DLGC_WANTMESSAGE;
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            key(wparam, lparam);
            return 0;
        }
        if (msg == WM_CHAR || msg == WM_SYSCHAR || msg == WM_KEYUP || msg == WM_SYSKEYUP) return 0;
        if (msg == WM_KILLFOCUS) {
            capturing = -1;
            refresh();
        }
    }
    // the release of the key that ended a capture (Space would click the button again)
    if ((msg == WM_KEYUP || msg == WM_SYSKEYUP) && swallow && wparam == swallow) {
        swallow = 0;
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) swallow = 0;
    if ((msg == WM_CHAR || msg == WM_SYSCHAR) && swallow) return 0;
    return DefSubclassProc(h, msg, wparam, lparam);
}
} // namespace keyboard_section

// Moves the Advanced-tab children with the scroll position; those outside the visible band are
// hidden (and all of them outside the Advanced tab).
inline void keyboard_section_apply_scroll() {
    using namespace keyboard_section;
    const bool shown = g.current_tab == TAB_ADVANCED;
    if (shown) settings_scroll = std::clamp(settings_scroll, 0.f, max_scroll());
    const int shift = shown ? px(settings_scroll) : 0, top = px(classic_skin ? view_top() : view_top() + 40.f),
              end = px(classic_skin ? view_bottom() : view_bottom() + 40.f);
    for (const auto& c : children) {
        const int y = c.base.top - shift;
        SetWindowPos(c.hwnd, nullptr, c.base.left, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        const bool inside = y >= top && y + (c.base.bottom - c.base.top) <= end;
        ShowWindow(c.hwnd, shown && inside ? SW_SHOW : SW_HIDE);
    }
}

// After the tab's other controls exist (create_controls of either skin).
inline void keyboard_section_create() {
    using namespace keyboard_section;
    children.clear();
    capturing = -1;
    swallow = 0;
    load();
    for (int i = 0; i < keys::count; ++i) {
        const float x = kColumn[i / 12], y = kTop + kRows + float(i % 12) * kRow;
        buttons[size_t(i)] =
            add_button(kFirstId + i, Kind::ActionBtn, box(x + kLabel, y, kButton, 26), L"", nullptr, -1, TAB_ADVANCED);
        SetWindowSubclass(buttons[size_t(i)], key_proc, 6, DWORD_PTR(i));
    }
    add_button(kResetId, Kind::ActionBtn, box(830, kTop + 12, 144, 30), L"Reset keys", nullptr, -1, TAB_ADVANCED);
    bottom = kTop + kRows + 12 * kRow + 6;
    refresh();
    std::vector<HWND> tab = {g.base_edit, g.patch_edit};
    for (const auto& [hwnd, control] : g.controls)
        if (control.tab == TAB_ADVANCED) tab.push_back(hwnd);
    for (HWND hwnd : tab) {
        if (!hwnd) continue;
        RECT r{};
        GetWindowRect(hwnd, &r);
        MapWindowPoints(nullptr, g.window, reinterpret_cast<POINT*>(&r), 2);
        children.push_back({hwnd, r});
    }
}

// Inside the Advanced page painting, after the session log (the caller has applied the scroll).
inline void keyboard_section_paint(Graphics& gr) {
    using namespace keyboard_section;
    const RectF card = box(296, kTop, 694, bottom - kTop + 12);
    fill_round(gr, card, 18, SolidBrush(Color(kArcadeCard)));
    stroke_round(gr, card, 18, kArcadeDark, 3.0f);
    text(gr, tr(L"KEYBOARD"), 0, 18.0f, FontStyleBold, box(312, kTop + 14, 150, 24), kArcadeCyan);
    text(gr, tr(L"Click a key, then press the new one. Used from the next game start."), 1, 10.5f, FontStyleRegular,
         box(312, kTop + 44, 660, 20), kTextMuted);
    for (int i = 0; i < keys::count; ++i)
        text(gr, tr(kLabels[i]), 1, 10.5f, FontStyleBold,
             box(kColumn[i / 12], kTop + kRows + float(i % 12) * kRow + 5, kLabel - 6, 18), kTextLight);
}

// A thin bar at the right edge of the page while the tab can scroll.
inline void keyboard_section_paint_scrollbar(Graphics& gr) {
    using namespace keyboard_section;
    const float range = max_scroll();
    if (range <= 0) return;
    const float top = view_top() + 8, height = view_bottom() - view_top() - 16;
    const float thumb = std::max(40.f, height * height / (height + range));
    const float y = top + (height - thumb) * (std::clamp(settings_scroll, 0.f, range) / range);
    fill_round(gr, box(994, top, 4, height), 2, SolidBrush(Color(0x40FFFFFF)));
    fill_round(gr, box(994, y, 4, thumb), 2, SolidBrush(Color(kArcadeCyan)));
}

// WM_COMMAND of a section button. true when handled.
inline bool keyboard_section_command(int id, int code) {
    using namespace keyboard_section;
    if (!owns(id)) return false;
    if (code != BN_CLICKED) return true;
    if (id == kResetId) {
        capturing = -1;
        std::error_code ec;
        fs::remove(file(), ec);
        load();
    } else {
        capturing = id - kFirstId;
        SetFocus(buttons[size_t(capturing)]);
    }
    refresh();
    return true;
}

// WM_MOUSEWHEEL of the launcher window. true when the Advanced tab scrolled.
inline bool keyboard_section_wheel(int delta) {
    if (g.current_tab != TAB_ADVANCED) return false;
    settings_scroll -= delta / float(WHEEL_DELTA) * 56.f;
    keyboard_section_apply_scroll();
    InvalidateRect(g.window, nullptr, FALSE);
    return true;
}
