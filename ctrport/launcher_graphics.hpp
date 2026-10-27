// SPDX-License-Identifier: GPL-2.0-only
// Settings tab, GRAPHICS section: every PC Settings control of the game menu, built from the same
// catalog (settings/menu_catalog.hpp, settings/control_schema.hpp) with the same pages, names and
// translations, below the three cards of the tab. A choice is saved at once to
// runtime/user/launcher_config.json, as the game menu saves it; a choice the selected renderer does
// not provide is not listed (as the game menu hides it). The tab scrolls with the mouse wheel.
// Coordinates are the Classic page's (x 296..990); Nitro maps them with page_rect.
#pragma once

namespace graphics_section {
namespace catalog = ctrport::settings::menu_catalog;
namespace schema = ctrport::settings::control_schema;

constexpr int kFirstId = 1200;
constexpr float kTop = 592, kRow = 32, kHeader = 30, kColumn[2] = {312, 652}, kLabel = 162, kCombo = 156;

struct Row {
    const catalog::Control* control;
    schema::Option option;
    HWND combo = nullptr;
    std::vector<unsigned> indices; // combo position -> domain index
    float x = 0, y = 0;
};
struct Header {
    std::string category;
    float x, y;
};
struct Child {
    HWND hwnd;
    RECT base; // client position at scroll 0
};
inline std::vector<Row> rows;
inline std::vector<Header> headers;
inline std::vector<Child> children; // every settings-tab child window, moved when the tab scrolls
inline float bottom = 0;            // end of the section (page coordinates)

// pages: Display, Upscaling, Quality presets on the left; Geometry and textures, Effects and
// lighting on the right
inline const std::vector<std::vector<std::string>>& columns() {
    static const std::vector<std::vector<std::string>> c = {{"display", "rendering", "presets"},
                                                            {"quality", "graphics"}};
    return c;
}
// launcher language (en, fr, it, de, es) -> catalog locale (en, it, fr, de, es, ar)
inline size_t locale() {
    static const size_t map[] = {0, 2, 1, 3, 4};
    const int l = launcher_text::language;
    return l >= 0 && l < 5 ? map[l] : 0;
}
inline RectF box(float x, float y, float w, float h) {
    return classic_skin ? rect(x, y, w, h) : page_rect(x, y, w, h);
}
// the visible band of the page, in page coordinates
inline float view_top() {
    return classic_skin ? 56.f : nitro::header - 40.f;
}
inline float view_bottom() {
    return classic_skin ? 616.f : nitro::footer - 40.f;
}
inline float max_scroll() {
    return std::max(0.f, bottom + 20.f - view_bottom());
}

// The renderer chosen in the launcher decides what is offered, as the game menu's hooks do.
inline bool available(const std::string& key, unsigned index) {
    const bool d3d11 = g.cfg.renderer == L"d3d11";
    if (key == "upscaler") return !d3d11 || index < 2;      // Direct3D 11: Off, FSR 1
    if (key == "shadow_quality") return index < 4 || d3d11; // Ultra: Direct3D 11 shadow maps
    return true;
}
inline std::wstring label(const catalog::Control& c, unsigned index) {
    const auto& localized = c.value_labels[locale()];
    return widen(index < localized.size() ? localized[index] : c.values[index]);
}
inline ctr_online::Json current() {
    ctr_online::Json doc = ctr_online::Json::object();
    try {
        doc = ctrport::settings::effective(ctrport::settings::Store(config_path()).read());
    } catch (const std::exception&) {}
    // the launcher's own state for the controls it also keeps
    doc.set("fps_60", ctr_online::Json::of(g.cfg.fps_60));
    doc.set("upscaler", ctr_online::Json::of(narrow(g.cfg.upscaler)));
    doc.set("upscaling_mode", ctr_online::Json::of(narrow(g.cfg.upscaling_mode)));
    return doc;
}
} // namespace graphics_section

// Lists and selections from the saved document (after a change, a preset, a renderer switch).
inline void graphics_section_refresh() {
    using namespace graphics_section;
    const auto doc = current();
    for (auto& row : rows) {
        const auto* value = doc.find(row.control->key);
        unsigned selected = 0;
        for (unsigned i = 0; i < schema::count(row.option); ++i)
            if (value && *value == schema::value(row.option, i)) selected = i;
        SendMessageW(row.combo, CB_RESETCONTENT, 0, 0);
        row.indices.clear();
        for (unsigned i = 0; i < schema::count(row.option); ++i) {
            if (!available(row.control->key, i) && i != selected) continue;
            SendMessageW(row.combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label(*row.control, i).c_str()));
            if (i == selected) SendMessageW(row.combo, CB_SETCURSEL, row.indices.size(), 0);
            row.indices.push_back(i);
        }
        // the 60 FPS mode exists in v01.21 only; the upscaling mode needs a method
        if (row.control->key == "fps_60") EnableWindow(row.combo, g.cfg.use_patch);
        if (row.control->key == "upscaling_mode") EnableWindow(row.combo, g.cfg.upscaler != L"off");
    }
}

// Moves the settings-tab children with the scroll position; those outside the visible band are
// hidden (and all of them outside the Settings tab).
inline void graphics_section_apply_scroll() {
    using namespace graphics_section;
    // the scroll position is the current tab's (the Advanced tab scrolls too)
    const bool shown = g.current_tab == TAB_SETTINGS;
    if (shown) settings_scroll = std::clamp(settings_scroll, 0.f, max_scroll());
    const int shift = shown ? px(settings_scroll) : 0, top = px(classic_skin ? view_top() : view_top() + 40.f),
              end = px(classic_skin ? view_bottom() : view_bottom() + 40.f);
    for (const auto& c : children) {
        const int y = c.base.top - shift;
        SetWindowPos(c.hwnd, nullptr, c.base.left, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        const bool inside = y >= top && y + (c.base.bottom - c.base.top) <= end;
        // the hidden launcher-only combos stay hidden
        if (c.hwnd == g.scene_combo || c.hwnd == g.upscaler_combo) continue;
        if (c.hwnd == g.width_edit || c.hwnd == g.height_edit) {
            ShowWindow(c.hwnd, shown && inside && g.resolution == 4 && !g.cfg.fullscreen ? SW_SHOW : SW_HIDE);
            continue;
        }
        ShowWindow(c.hwnd, shown && inside ? SW_SHOW : SW_HIDE);
    }
}

// Wheel over a closed combo scrolls the page instead of changing the choice.
inline LRESULT CALLBACK graphics_combo_proc(HWND h, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEWHEEL && !SendMessageW(h, CB_GETDROPPEDSTATE, 0, 0))
        return SendMessageW(GetParent(h), WM_MOUSEWHEEL, wparam, lparam);
    return DefSubclassProc(h, msg, wparam, lparam);
}

// After the tab's other controls exist (create_controls of either skin).
inline void graphics_section_create() {
    using namespace graphics_section;
    rows.clear();
    headers.clear();
    children.clear();
    settings_scroll = 0;
    for (int c = 0; c < 2; ++c) {
        float y = kTop + 48;
        for (const auto& key : columns()[c]) {
            headers.push_back({key, kColumn[c], y});
            y += kHeader;
            for (const auto& ctl : catalog::controls()) {
                if (ctl.category != key) continue;
                Row row{&ctl, schema::Option(ctl.id)};
                row.x = kColumn[c];
                row.y = y;
                const RectF r = box(row.x + kLabel + 4, row.y, kCombo, 26);
                row.combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                            px(r.X), px(r.Y), px(r.Width), px(300), g.window,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kFirstId + rows.size())),
                                            nullptr, nullptr);
                SendMessageW(row.combo, WM_SETFONT, reinterpret_cast<WPARAM>(g.edit_font), FALSE);
                SendMessageW(row.combo, CB_SETDROPPEDWIDTH, px(classic_skin ? 230.f : nitro::page_w(160)), 0);
                rows.push_back(row);
                y += kRow;
            }
            y += 6;
        }
        bottom = std::max(bottom, y);
    }
    graphics_section_refresh();
    // every child of the Settings tab, for scrolling
    std::vector<HWND> tab = {g.width_edit,     g.height_edit, g.online_id_edit, g.language_combo,
                             g.renderer_combo, g.scene_combo, g.upscaler_combo};
    for (HWND combo : g.player_combo) tab.push_back(combo);
    for (const auto& [hwnd, control] : g.controls)
        if (control.tab == TAB_SETTINGS) tab.push_back(hwnd);
    for (const auto& row : rows) tab.push_back(row.combo);
    for (HWND hwnd : tab) {
        if (!hwnd) continue;
        RECT r{};
        GetWindowRect(hwnd, &r);
        MapWindowPoints(nullptr, g.window, reinterpret_cast<POINT*>(&r), 2);
        children.push_back({hwnd, r});
        wchar_t cls[16]{};
        GetClassNameW(hwnd, cls, 16);
        if (!_wcsicmp(cls, L"ComboBox")) SetWindowSubclass(hwnd, graphics_combo_proc, 3, 0);
    }
}

// Inside the Settings page painting, after the cards (the caller has applied the scroll).
inline void graphics_section_paint(Graphics& gr) {
    using namespace graphics_section;
    const RectF card = box(296, kTop, 694, bottom - kTop + 12);
    fill_round(gr, card, 18, SolidBrush(Color(kArcadeCard)));
    stroke_round(gr, card, 18, kArcadeDark, 3.0f);
    text(gr, tr(L"GRAPHICS"), 0, 18.0f, FontStyleBold, box(312, kTop + 14, 300, 24), kArcadeCyan);
    text(gr, tr(L"Saved at once; restart settings apply at the next start of the game."), 1, 10.5f, FontStyleRegular,
         box(560, kTop + 18, 414, 20), kTextMuted);
    for (const auto& h : headers)
        for (const auto& category : catalog::categories())
            if (category.key == h.category)
                text(gr, widen(category.labels[locale()]), 0, 13.0f, FontStyleBold, box(h.x, h.y + 4, 322, 22),
                     kArcadeGold);
    for (const auto& row : rows)
        text(gr, widen(row.control->labels[locale()]), 1, 10.5f, FontStyleBold, box(row.x, row.y + 5, kLabel, 18),
             IsWindowEnabled(row.combo) ? kTextLight : kTextMuted);
}

// A thin bar at the right edge of the page while the tab can scroll.
inline void graphics_section_paint_scrollbar(Graphics& gr) {
    using namespace graphics_section;
    const float range = max_scroll();
    if (range <= 0) return;
    const float top = view_top() + 8, height = view_bottom() - view_top() - 16;
    const float thumb = std::max(40.f, height * height / (height + range));
    const float y = top + (height - thumb) * (settings_scroll / range);
    fill_round(gr, box(994, top, 4, height), 2, SolidBrush(Color(0x40FFFFFF)));
    fill_round(gr, box(994, y, 4, thumb), 2, SolidBrush(Color(kArcadeCyan)));
}

// WM_COMMAND from a section combo: saves the choice (a quality preset writes its bundle) and
// refreshes the section and the launcher's own copies. true when handled.
inline bool graphics_section_command(int id, int code) {
    using namespace graphics_section;
    if (id < kFirstId || id >= kFirstId + int(rows.size())) return false;
    if (code != CBN_SELCHANGE) return true;
    const Row& row = rows[size_t(id - kFirstId)];
    const auto sel = SendMessageW(row.combo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || size_t(sel) >= row.indices.size()) return true;
    const auto value = schema::value(row.option, row.indices[size_t(sel)]);
    const std::string key = row.control->key;
    try {
        auto changes = ctr_online::Json::object();
        changes.set(key, value);
        ctrport::settings::Store(config_path()).update(changes);
    } catch (const std::exception& error) {
        MessageBoxW(g.window, widen(error.what()).c_str(), tr(L"Settings were not saved"), MB_ICONERROR);
    }
    if (key == "upscaler") g.cfg.upscaler = widen(value.text);
    if (key == "upscaling_mode") g.cfg.upscaling_mode = widen(value.text);
    if (key == "fps_60") g.cfg.fps_60 = value.boolean;
    graphics_section_refresh();
    validate();
    InvalidateRect(g.window, nullptr, FALSE);
    return true;
}

// WM_MOUSEWHEEL of the launcher window. true when the Settings tab scrolled.
inline bool graphics_section_wheel(int delta) {
    if (g.current_tab != TAB_SETTINGS) return false;
    settings_scroll -= delta / float(WHEEL_DELTA) * 56.f;
    graphics_section_apply_scroll();
    InvalidateRect(g.window, nullptr, FALSE);
    return true;
}
