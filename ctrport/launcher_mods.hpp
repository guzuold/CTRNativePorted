// SPDX-License-Identifier: GPL-2.0-only
// MODS tab list, shared by both skins. FILE MODS shows a drop zone for archives and folders
// downloaded by hand, the installed mods, then the GameBanana catalog (gamebanana.hpp);
// PATCHES shows the patch files. The list scrolls with the mouse wheel: it is drawn clipped to
// its band, and a pool of kModRows row slots carries the buttons of the rows wholly inside it.
// Coordinates are the Classic page's (x 296..990); Nitro maps them with page_rect (box()).
#pragma once

namespace mods_list {
enum ItemKind { Drop, Heading, Empty, Mod, Patch, Online };
struct Item {
    ItemKind kind;
    int index;  // in mod_list, patch_list or gb_list; Heading: 0 installed, 1 GameBanana
    float y, h; // from the top of the list
};
constexpr float kTop = 172, kBottom = 574, kStep = 52, kRowH = 44;

inline RectF box(float x, float y, float w, float h) {
    return classic_skin ? rect(x, y, w, h) : page_rect(x, y, w, h);
}

inline std::vector<Item> items() {
    std::vector<Item> list;
    float y = 0;
    const auto add = [&](ItemKind kind, int index, float h, float gap) {
        list.push_back({kind, index, y, h});
        y += h + gap;
    };
    if (g.mods_view == 1) {
        if (g.patch_list.empty()) add(Empty, 1, 100, 8);
        for (int i = 0; i < static_cast<int>(g.patch_list.size()); ++i) add(Patch, i, kRowH, kStep - kRowH);
        return list;
    }
    add(Drop, 0, 66, 14);
    add(Heading, 0, 26, 4);
    if (g.mod_list.empty()) add(Empty, 0, kRowH, kStep - kRowH);
    for (int i = 0; i < static_cast<int>(g.mod_list.size()); ++i) add(Mod, i, kRowH, kStep - kRowH);
    y += 10;
    add(Heading, 1, 26, 4);
    if (g.gb_list.empty()) add(Empty, 2, kRowH, kStep - kRowH);
    for (int i = 0; i < static_cast<int>(g.gb_list.size()); ++i) add(Online, i, kRowH, kStep - kRowH);
    return list;
}
inline float max_scroll(const std::vector<Item>& list) {
    return list.empty() ? 0.f : std::max(0.f, list.back().y + list.back().h - (kBottom - kTop));
}
inline bool installed(const ctrport::gamebanana::Entry& entry) {
    return g.gb_installed.count(entry.id) != 0;
}

inline std::wstring online_detail(const ctrport::gamebanana::Entry& entry) {
    namespace gb = ctrport::gamebanana;
    if (g.mods_busy && g.mods_busy_id == entry.id)
        return g.mods_busy_percent >= 0 ? trf(L"Downloading... {0}%", std::to_wstring(g.mods_busy_percent))
                                        : std::wstring(tr(L"Installing..."));
    std::wstring detail = entry.model == "Sound" ? std::wstring(tr(L"Sound")) : std::wstring(tr(L"Mod"));
    if (!entry.author.empty()) detail += L"  ·  " + trf(L"by {0}", gb::wide(entry.author));
    detail += L"  ·  " + (entry.files.size() > 1 ? trf(L"{0} files", std::to_wstring(entry.files.size()))
                                                 : gb::size_text(entry.files[0].size));
    if (const auto found = g.gb_installed.find(entry.id); found != g.gb_installed.end())
        detail = trf(L"Installed: {0}", gb::wide(found->second.file)) + L"  ·  " + detail;
    return detail;
}

inline void place(HWND hwnd, float x, float y, float w, float h, bool show) {
    const RectF r = box(x, y, w, h);
    SetWindowPos(hwnd, nullptr, px(r.X), px(r.Y), 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
}
} // namespace mods_list

// Puts the row slots on the visible rows and refreshes their states.
void sync_mod_rows() {
    using namespace mods_list;
    if (!g.mod_row_toggle[0]) return;
    const auto list = items();
    g.mods_scroll = std::clamp(g.mods_scroll, 0.f, max_scroll(list));
    const bool shown = g.current_tab == TAB_MODS;
    int slot = 0;
    for (int& item : g.mod_slot_item) item = -1;
    for (int i = 0; i < static_cast<int>(list.size()) && slot < kModRows; ++i) {
        const Item& item = list[i];
        if (item.kind != Mod && item.kind != Patch && item.kind != Online) continue;
        const float y = kTop + item.y - g.mods_scroll;
        if (y >= kTop && y + kRowH <= kBottom) g.mod_slot_item[slot++] = i;
    }
    for (int row = 0; row < kModRows; ++row) {
        const Item* item = g.mod_slot_item[row] >= 0 ? &list[g.mod_slot_item[row]] : nullptr;
        const ItemKind kind = item ? item->kind : Drop;
        const int i = item ? item->index : 0;
        const float y = item ? kTop + item->y - g.mods_scroll : kTop;
        if (kind == Mod) g.mod_row_on[row] = g.mod_list[i].enabled;
        if (kind == Patch) g.mod_row_on[row] = ctrport::guest_patches::selected(g.patch_list[i], g.patch_selection);
        place(g.mod_row_toggle[row], 930, y + 6, 54, 32, shown && (kind == Mod || kind == Patch));
        place(g.mod_row_up[row], 836, y + 8, 36, 28, shown && kind == Mod);
        place(g.mod_row_down[row], 878, y + 8, 36, 28, shown && kind == Mod);
        place(g.gb_row_page[row], 790, y + 8, 84, 28, shown && kind == Online);
        place(g.gb_row_get[row], 882, y + 8, 102, 28, shown && kind == Online);
        EnableWindow(g.mod_row_up[row], kind == Mod && i > 0);
        EnableWindow(g.mod_row_down[row], kind == Mod && i + 1 < static_cast<int>(g.mod_list.size()));
        EnableWindow(g.gb_row_get[row], kind == Online && !g.mods_busy);
        if (kind == Online) g.controls.at(g.gb_row_get[row]).label = installed(g.gb_list[i]) ? L"REMOVE" : L"INSTALL";
        for (HWND control :
             {g.mod_row_toggle[row], g.mod_row_up[row], g.mod_row_down[row], g.gb_row_page[row], g.gb_row_get[row]})
            InvalidateRect(control, nullptr, FALSE);
    }
    for (const auto& entry : g.controls)
        if (entry.second.kind == Kind::Segment) InvalidateRect(entry.first, nullptr, FALSE);
    InvalidateRect(g.window, nullptr, FALSE);
}

// WM_MOUSEWHEEL of the launcher window. true when the MODS list scrolled.
bool mods_list_wheel(int delta) {
    if (g.current_tab != TAB_MODS) return false;
    g.mods_scroll -= delta / float(WHEEL_DELTA) * mods_list::kStep;
    sync_mod_rows();
    return true;
}

// The MODS page, inside either skin's painting.
void mods_list_paint(Graphics& gr) {
    using namespace mods_list;
    namespace gb = ctrport::gamebanana;
    text(gr, L"MODS", 0, 36.0f, FontStyleBold, box(296, 76, 300, 40), kArcadeGold);
    int active = 0;
    for (const auto& mod : g.mod_list) active += mod.enabled;
    for (std::size_t i = 0; i < g.patch_list.size(); ++i)
        active += !g.patch_warn[i] && ctrport::guest_patches::selected(g.patch_list[i], g.patch_selection);
    draw_slanted_box(gr, box(410, 84, 120, 26), 0.14f, 3.0f, SolidBrush(Color(kArcadeDark)), kArcadeCard, 2.0f);
    text(gr, g.cfg.mods_enabled ? trf(L"{0} ENABLED", std::to_wstring(active)) : std::wstring(tr(L"ALL OFF")), 1, 11.0f,
         FontStyleBold, box(410, 84, 120, 26), g.cfg.mods_enabled && active ? kArcadeLime : kTextMuted,
         StringAlignmentCenter);

    const auto list = items();
    const float alpha = g.cfg.mods_enabled ? 1.f : 0.5f;
    const RectF band = box(296, kTop, 704, kBottom - kTop);
    const GraphicsState unclipped = gr.Save();
    gr.SetClip(RectF(band.X - 4, band.Y, band.Width + 8, band.Height));
    for (const Item& item : list) {
        const float y = kTop + item.y - g.mods_scroll;
        if (y + item.h < kTop || y > kBottom) continue;
        switch (item.kind) {
        case Drop: {
            const RectF zone = box(296, y, 694, item.h);
            fill_round(gr, zone, 14, SolidBrush(Color(kArcadeCardSub)));
            GraphicsPath outline;
            round_rect(outline, zone, 14);
            Pen dashed(Color(g.mods_busy && !g.mods_busy_id ? kArcadeGold : kArcadeCyan), 2.0f);
            dashed.SetDashStyle(DashStyleDash);
            gr.DrawPath(&dashed, &outline);
            const bool installing = g.mods_busy && !g.mods_busy_id;
            text(gr,
                 installing ? trf(L"Installing {0}...", g.mods_busy_name)
                            : std::wstring(tr(L"Drop a downloaded mod here to install it")),
                 0, 16.0f, FontStyleBold, box(312, y + 12, 662, 24), installing ? kArcadeGold : kTextLight,
                 StringAlignmentCenter);
            text(gr, tr(L"A .zip, .7z or .rar archive, or its extracted folder"), 1, 11.0f, FontStyleRegular,
                 box(312, y + 38, 662, 18), kTextMuted, StringAlignmentCenter);
            break;
        }
        case Heading: {
            const bool online = item.index == 1;
            text(gr, online ? L"GAMEBANANA" : tr(L"INSTALLED"), 0, 15.0f, FontStyleBold, box(300, y + 2, 300, 22),
                 online ? kArcadeCyan : kArcadeGold);
            const std::wstring note = online ? (g.gb_loading ? std::wstring(tr(L"Loading..."))
                                                             : trf(L"{0} mods for Crash Team Racing Nitro-Fueled (PC)",
                                                                   std::to_wstring(g.gb_list.size())))
                                             : trf(L"Folder: {0}", L"runtime/mods");
            text(gr, note, 1, 10.5f, FontStyleRegular, box(560, y + 6, 426, 18), kTextMuted, StringAlignmentFar);
            break;
        }
        case Empty: {
            const RectF card = box(296, y, 694, item.h);
            fill_round(gr, card, 14, SolidBrush(Color(kArcadeCard)));
            stroke_round(gr, card, 14, kArcadeDark, 3.0f);
            if (item.index == 1) {
                text(gr, tr(L"No patch files yet"), 0, 22.0f, FontStyleBold, box(320, y + 20, 640, 30), kTextLight);
                text(gr, trf(L"Folder: {0}", L"runtime/patches"), 1, 12.0f, FontStyleRegular, box(320, y + 60, 640, 20),
                     kArcadeCyan);
            } else {
                const std::wstring line = item.index == 0 ? std::wstring(tr(L"No mods yet"))
                                          : g.gb_loading  ? std::wstring(tr(L"Loading the GameBanana catalog..."))
                                          : !g.gb_error.empty()
                                              ? trf(L"GameBanana cannot be reached: {0}", g.gb_error)
                                              : std::wstring(tr(L"Press REFRESH to load the GameBanana catalog."));
                text(gr, line, 1, 12.5f, FontStyleBold, box(320, y + 12, 650, 20),
                     !g.gb_error.empty() && item.index == 2 ? kArcadePink : kTextMuted);
            }
            break;
        }
        case Mod:
        case Patch:
        case Online: {
            const int i = item.index;
            const bool online = item.kind == Online;
            const bool on = online             ? installed(g.gb_list[i])
                            : item.kind == Mod ? g.mod_list[i].enabled
                                               : ctrport::guest_patches::selected(g.patch_list[i], g.patch_selection);
            const bool warn = online ? false : item.kind == Mod ? g.mod_bad[i] : g.patch_warn[i];
            const float row_alpha = online ? 1.f : alpha;
            fill_round(gr, box(296, y, 694, kRowH), 14, SolidBrush(Color(kArcadeCard)));
            stroke_round(gr, box(296, y, 694, kRowH), 14, kArcadeDark, 3.0f);
            const auto at = [&](float px_, float py) {
                const RectF r = box(px_, py, 0, 0);
                return PointF(r.X, r.Y);
            };
            PointF badge_pts[] = {at(296, y), at(344, y), at(335, y + kRowH), at(296, y + kRowH)};
            const ARGB badge = online                     ? (on ? kArcadeLime : kArcadeCyan)
                               : on && g.cfg.mods_enabled ? (warn               ? kArcadeOrange
                                                             : item.kind == Mod ? kArcadeLime
                                                                                : kArcadeCyan)
                                                          : kTextOff;
            SolidBrush badge_br(Color(fade(badge, row_alpha)));
            gr.FillPolygon(&badge_br, badge_pts, 4);
            Pen badge_pen(Color(kArcadeDark), 3.0f);
            gr.DrawPolygon(&badge_pen, badge_pts, 4);
            text(gr, online ? std::wstring(L"GB") : std::to_wstring(i + 1), 0, online ? 14.0f : 18.0f, FontStyleBold,
                 box(296, y + 8, 42, 28), kArcadeDark, StringAlignmentCenter);
            const std::wstring name = online             ? gb::wide(g.gb_list[i].name)
                                      : item.kind == Mod ? g.mod_list[i].name
                                                         : widen(g.patch_list[i].name);
            const std::wstring detail = online             ? online_detail(g.gb_list[i])
                                        : item.kind == Mod ? g.mod_details[i]
                                                           : g.patch_details[i];
            const float width = online ? 426.f : item.kind == Mod ? 470.f : 560.f;
            const bool busy = online && g.mods_busy && g.mods_busy_id == g.gb_list[i].id;
            text(gr, name, 1, 13.5f, FontStyleBold, box(354, y + 4, width, 20), fade(kTextLight, row_alpha));
            text(gr, detail, 1, 10.5f, FontStyleRegular, box(354, y + 24, width, 18),
                 fade(busy   ? kArcadeGold
                      : warn ? kArcadeOrange
                             : kTextMuted,
                      row_alpha));
            break;
        }
        }
    }
    gr.Restore(unclipped);
    // A thin bar at the right edge while the list can scroll.
    if (const float range = max_scroll(list); range > 0) {
        const float height = kBottom - kTop, thumb = std::max(40.f, height * height / (height + range));
        fill_round(gr, box(994, kTop, 4, height), 2, SolidBrush(Color(0x40FFFFFF)));
        fill_round(gr, box(994, kTop + (height - thumb) * (g.mods_scroll / range), 4, thumb), 2,
                   SolidBrush(Color(kArcadeCyan)));
    }
    const std::wstring notice = !g.mods_notice.empty() ? g.mods_notice
                                : g.mods_game.empty()  ? std::wstring(tr(L"Game version unknown"))
                                                       : trf(L"Game: {0}", g.mods_game);
    text(gr, notice, 1, 11.0f, FontStyleRegular, box(296, 582, 694, 22), g.mods_notice_error ? kArcadePink : kTextMuted,
         StringAlignmentCenter);
}
