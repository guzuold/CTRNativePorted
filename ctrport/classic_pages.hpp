// SPDX-License-Identifier: GPL-2.0-only
// Original v0.6.0 presentation, retained as the Classic skin.
void classic_paint_window(HDC target) {
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
            draw_slanted_box(gr, rect(kWidth - 184, kHeight - 150, 140, 30), 0.176f, 4.0f, SolidBrush(Color(kArcadeDark)), kArcadeGold, 2.0f);
            text(gr, tr(g.can_launch ? L"READY" : L"SETUP REQUIRED"), 1, 11.5f, FontStyleBold, rect(kWidth - 184, kHeight - 150, 140, 30), kArcadeGold, StringAlignmentCenter);

            fill_round(gr, rect(280, kHeight - 37, 720, 26), 8, SolidBrush(Color(kArcadeDark)));
            // Small version summary line in bottom corner
            text(gr, g.global.text, 1, 11.0f, FontStyleRegular, rect(292, kHeight - 34, 692, 20), kTextMuted);
        } else {
            // Sub-pane background container: rect(270, 56, 746, 560)
            // 3a/3b/3c share the uninterrupted purple Pit Lane shell.

            if (g.current_tab == TAB_MODS) {
                text(gr, L"MODS", 0, 36.0f, FontStyleBold, rect(296, 76, 300, 40), kArcadeGold);
                int active = 0;
                for (const auto& mod : g.mod_list) active += mod.enabled;
                for (std::size_t i = 0; i < g.patch_list.size(); ++i)
                    active += !g.patch_warn[i] && ctrport::guest_patches::selected(g.patch_list[i], g.patch_selection);
                draw_slanted_box(gr, rect(410, 84, 120, 26), 0.14f, 3.0f, SolidBrush(Color(kArcadeDark)), kArcadeCard, 2.0f);
                text(gr, g.cfg.mods_enabled ? trf(L"{0} ENABLED", std::to_wstring(active)) : std::wstring(tr(L"ALL OFF")), 1, 11.0f, FontStyleBold,
                     rect(410, 84, 120, 26), g.cfg.mods_enabled && active ? kArcadeLime : kTextMuted, StringAlignmentCenter);

                const bool files = g.mods_view == 0;
                const int count = files ? static_cast<int>(g.mod_list.size()) : static_cast<int>(g.patch_list.size());
                const float alpha = g.cfg.mods_enabled ? 1.f : 0.5f;
                if (count == 0) {
                    const RectF empty_card = rect(296, 176, 694, 100);
                    fill_round(gr, empty_card, 16, SolidBrush(Color(kArcadeCard)));
                    stroke_round(gr, empty_card, 16, kArcadeDark, 3.0f);
                    text(gr, tr(files ? L"No mods yet" : L"No patch files yet"), 0, 22.0f, FontStyleBold, rect(320, 196, 640, 30), kTextLight);
                    text(gr, trf(L"Folder: {0}", files ? L"runtime/mods" : L"runtime/patches"), 1, 12.0f, FontStyleRegular, rect(320, 236, 640, 20), kArcadeCyan);
                }
                for (int row = 0; row < kModRows; ++row) {
                    const int i = g.mods_page * kModRows + row;
                    if (i >= count) break;
                    const float y = 176.f + row * 52.f;
                    const bool on = g.mod_row_on[row];
                    const bool warn = files ? g.mod_bad[i] : g.patch_warn[i];
                    fill_round(gr, rect(296, y, 694, 44), 14, SolidBrush(Color(kArcadeCard)));
                    stroke_round(gr, rect(296, y, 694, 44), 14, kArcadeDark, 3.0f);
                    PointF badge_pts[] = {PointF(296.f, y), PointF(344.f, y), PointF(335.f, y + 44.f), PointF(296.f, y + 44.f)};
                    SolidBrush badge_br(Color(fade(on && g.cfg.mods_enabled ? (warn ? kArcadeOrange : files ? kArcadeLime : kArcadeCyan) : kTextOff, alpha)));
                    gr.FillPolygon(&badge_br, badge_pts, 4);
                    Pen badge_pen(Color(kArcadeDark), 3.0f);
                    gr.DrawPolygon(&badge_pen, badge_pts, 4);
                    text(gr, std::to_wstring(i + 1), 0, 18.0f, FontStyleBold, rect(296, y + 8, 42, 28), kArcadeDark, StringAlignmentCenter);
                    const std::wstring name = files ? g.mod_list[i].name : widen(g.patch_list[i].name);
                    const std::wstring& detail = files ? g.mod_details[i] : g.patch_details[i];
                    text(gr, name, 1, 13.5f, FontStyleBold, rect(354, y + 4, files ? 470.f : 560.f, 20), fade(kTextLight, alpha));
                    text(gr, detail, 1, 10.5f, FontStyleRegular, rect(354, y + 24, files ? 470.f : 560.f, 18), fade(warn ? kArcadeOrange : kTextMuted, alpha));
                }
                const int pages = std::max(1, (count + kModRows - 1) / kModRows);
                const std::wstring footer = trf(L"Page {0} / {1}", std::to_wstring(g.mods_page + 1), std::to_wstring(pages));
                text(gr, footer, 1, 11.5f, FontStyleBold, rect(396, 544, 494, 30), kTextMuted, StringAlignmentCenter);
                const std::wstring notice = !g.mods_notice.empty() ? g.mods_notice
                    : g.mods_game.empty() ? std::wstring(tr(L"Game version unknown")) : trf(L"Game: {0}", g.mods_game);
                text(gr, notice, 1, 11.0f, FontStyleRegular, rect(296, 582, 694, 22), g.mods_notice_error ? kArcadePink : kTextMuted, StringAlignmentCenter);
            } else if (g.current_tab == TAB_TROPHIES) {
                text(gr, tr(L"TROPHIES"), 0, 36.0f, FontStyleBold, rect(296, 76, 300, 40), kArcadeGold);
                const auto total = g.trophy_set.trophies.size();
                draw_slanted_box(gr, rect(870, 84, 120, 26), 0.14f, 3.0f, SolidBrush(Color(kArcadeDark)), kArcadeCard, 2.0f);
                text(gr, std::to_wstring(g.trophy_unlocked.size()) + L" / " + std::to_wstring(total), 1, 11.0f, FontStyleBold,
                     rect(870, 84, 120, 26), g.trophy_unlocked.empty() ? kTextMuted : kArcadeLime, StringAlignmentCenter);
                ColorMatrix gray = {{{0.2f, 0.2f, 0.2f, 0, 0}, {0.4f, 0.4f, 0.4f, 0, 0}, {0.1f, 0.1f, 0.1f, 0, 0}, {0, 0, 0, 0.35f, 0}, {0, 0, 0, 0, 1}}};
                ImageAttributes locked;
                locked.SetColorMatrix(&gray);
                for (std::size_t i = 0; i < total && i < g.trophy_icons.size(); ++i) {
                    const float x = kTrophyLeft + (i % kTrophyColumns) * kTrophyCell, y = kTrophyTop + (i / kTrophyColumns) * kTrophyCell;
                    const bool unlocked = g.trophy_unlocked.count(g.trophy_set.trophies[i].id) > 0;
                    if (static_cast<int>(i) == g.trophy_hot) fill_round(gr, rect(x - 3, y - 3, kTrophyIcon + 6, kTrophyIcon + 6), 8, SolidBrush(Color(kArcadeGold)));
                    if (Bitmap* icon = g.trophy_icons[i].get()) {
                        const RectF r = rect(x, y, kTrophyIcon, kTrophyIcon);
                        if (unlocked) gr.DrawImage(icon, r);
                        else gr.DrawImage(icon, r, 0, 0, static_cast<REAL>(icon->GetWidth()), static_cast<REAL>(icon->GetHeight()), UnitPixel, &locked);
                    } else {
                        fill_round(gr, rect(x, y, kTrophyIcon, kTrophyIcon), 8, SolidBrush(Color(kArcadeCard)));
                    }
                }
                if (total == 0) {
                    text(gr, tr(L"No trophy file in the dump"), 1, 13.0f, FontStyleRegular, rect(296, 140, 694, 24), kTextMuted);
                } else {
                    const int shown = g.trophy_hot >= 0 && g.trophy_hot < static_cast<int>(total) ? g.trophy_hot : 0;
                    const auto& t = g.trophy_set.trophies[static_cast<std::size_t>(shown)];
                    const auto when = g.trophy_unlocked.find(t.id);
                    const RectF card = rect(296, 362, 694, 100);
                    fill_round(gr, card, 14, SolidBrush(Color(kArcadeCard)));
                    stroke_round(gr, card, 14, kArcadeDark, 3.0f);
                    if (static_cast<std::size_t>(shown) < g.trophy_icons.size() && g.trophy_icons[static_cast<std::size_t>(shown)])
                        gr.DrawImage(g.trophy_icons[static_cast<std::size_t>(shown)].get(), rect(310, 372, 80, 80));
                    std::wstring name = t.name.empty() ? trf(L"Trophy {0}", std::to_wstring(t.id)) : widen(t.name);
                    std::wstring detail = widen(t.detail);
                    if (t.hidden && when == g.trophy_unlocked.end() && g.trophy_set.texts) { name = tr(L"Hidden trophy"); detail.clear(); }
                    const wchar_t* grade = t.grade == 'P' ? L"Platinum" : t.grade == 'G' ? L"Gold" : t.grade == 'S' ? L"Silver" : t.grade == 'B' ? L"Bronze" : nullptr;
                    text(gr, name, 1, 15.0f, FontStyleBold, rect(404, 370, 440, 22), kTextLight);
                    if (grade) text(gr, tr(grade), 1, 11.0f, FontStyleBold, rect(850, 372, 130, 18), kArcadeGold, StringAlignmentFar);
                    text(gr, detail, 1, 11.5f, FontStyleRegular, rect(404, 394, 576, 34), kTextMuted);
                    std::wstring state = tr(L"Locked");
                    if (when != g.trophy_unlocked.end()) {
                        const std::time_t at = static_cast<std::time_t>(when->second);
                        std::tm local{};
                        wchar_t date[32]{};
                        if (!localtime_s(&local, &at)) std::wcsftime(date, 32, L"%Y-%m-%d %H:%M", &local);
                        state = trf(L"Unlocked {0}", date);
                    }
                    text(gr, state, 1, 11.0f, FontStyleBold, rect(404, 432, 576, 20), when != g.trophy_unlocked.end() ? kArcadeLime : kTextMuted);
                }
                text(gr, tr(L"Trophy key (optional):"), 1, 11.5f, FontStyleBold, rect(296, 478, 520, 18), kTextMuted);
                draw_field(gr, rect(296, 500, 520, 34), g.trophy_key_edit);
                const std::wstring note = !g.trophy_notice.empty() ? g.trophy_notice
                    : g.trophy_set.texts ? std::wstring() : std::wstring(tr(L"Names and descriptions need your PS4 trophy key."));
                text(gr, note, 1, 11.0f, FontStyleRegular, rect(296, 544, 694, 22), g.trophy_notice_error ? kArcadePink : kTextMuted);
            } else if (g.current_tab == TAB_SETTINGS) {
                // the tab scrolls (mouse wheel): clipped to the page, shifted by the scroll position
                const GraphicsState unscrolled = gr.Save();
                gr.SetClip(RectF(270.f, 56.f, 746.f, 560.f));
                gr.TranslateTransform(0, -settings_scroll);
                text(gr, tr(L"SETTINGS"), 0, 36.0f, FontStyleBold, rect(296, 76, 300, 40), kArcadeGold);

                // Card 1: GRAPHICS
                const RectF g_card = rect(296, 126, 222, 450);
                fill_round(gr, g_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, g_card, 18, kArcadeDark, 3.0f);
                text(gr, tr(L"WINDOW & RENDERER"), 0, 15.0f, FontStyleBold, rect(312, 142, 190, 24), kArcadeCyan);
                text(gr, tr(L"Display resolution:"), 1, 11.5f, FontStyleBold, rect(312, 172, 190, 18), kTextMuted);

                if (g.resolution == 4 && !g.cfg.fullscreen) {
                    draw_field(gr, rect(312, 314, 88, 28), g.width_edit);
                    draw_field(gr, rect(412, 314, 88, 28), g.height_edit);
                    text(gr, L"x", 1, 12.0f, FontStyleBold, rect(400, 314, 12, 28), kTextMuted, StringAlignmentCenter);
                }

                text(gr, tr(L"Renderer:"), 1, 11.5f, FontStyleBold, rect(312, 400, 190, 20), kTextMuted);

                // Card 2: ONLINE & MULTIPLAYER
                const RectF o_card = rect(532, 126, 222, 450);
                fill_round(gr, o_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, o_card, 18, kArcadeDark, 3.0f);
                text(gr, L"ONLINE & MULTI", 0, 18.0f, FontStyleBold, rect(548, 142, 190, 24), kArcadeGold);
                text(gr, tr(L"Network mode:"), 1, 11.5f, FontStyleBold, rect(548, 172, 190, 18), kTextMuted);

                text(gr, tr(L"Player name:"), 1, 11.5f, FontStyleBold, rect(548, 230, 190, 18), kTextMuted);
                draw_field(gr, rect(548, 252, 190, 32), g.online_id_edit);

                text(gr,g.cfg.online ? L"ONLINE" : L"OFFLINE",0,18,FontStyleBold,rect(548,490,190,28),g.cfg.online ? kArcadeLime : kTextMuted);

                // Card 3: CONTROLLER & AUDIO
                const RectF c_card = rect(768, 126, 222, 300);
                fill_round(gr, c_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, c_card, 18, kArcadeDark, 3.0f);
                text(gr, tr(L"CONTROLLER"), 0, 18.0f, FontStyleBold, rect(784, 142, 190, 24), kArcadePink);
                for (int player = 0; player < 4; ++player)
                    text(gr, trf(L"Player {0}", std::to_wstring(player + 1)), 1, 10.5f, FontStyleBold, rect(784, 168.f + player * 46.f, 190, 16), kTextMuted);

                fill_round(gr, rect(768, 436, 222, 140), 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, rect(768, 436, 222, 140), 18, kArcadeDark, 3);

                text(gr, tr(L"Game language:"), 1, 11, FontStyleBold, rect(784, 516, 190, 20), kTextMuted);
                text(gr, L"AUDIO", 0, 18.0f, FontStyleBold, rect(784, 446, 100, 24), kArcadeLime);
                graphics_section_paint(gr);
                gr.Restore(unscrolled);
                graphics_section_paint_scrollbar(gr);
            } else if (g.current_tab == TAB_ADVANCED) {
                text(gr, tr(L"ADVANCED"), 0, 36.0f, FontStyleBold, rect(296, 76, 300, 40), kArcadeGold);

                text(gr, tr(L"Base game folder (CUSA14876 v01.00):"), 1, 11.5f, FontStyleBold, rect(296, 122, 600, 18), kTextMuted);
                draw_field(gr, rect(296, 144, 580, 34), g.base_edit);
                draw_status(gr, g.base_status, 296, 182, 580);

                text(gr, tr(L"Update folder (v01.21 CUSA14876-patch):"), 1, 11.5f, FontStyleBold, rect(296, 208, 600, 18), kTextMuted);
                draw_field(gr, rect(296, 230, 580, 34), g.patch_edit);
                draw_status(gr, g.patch_status, 296, 268, 580);

                // Logs pre box
                const RectF log_box = rect(296, 364, 694, 216);
                fill_round(gr, log_box, 14, SolidBrush(Color(0xFF0C0824)));
                stroke_round(gr, log_box, 14, kArcadeDark, 3.0f);

                text(gr, tr(L"SESSION LOG"), 1, 12, FontStyleBold, rect(312, 378, 660, 24), kArcadeCyan);
                std::wstring log = tr(L"No session yet.");
                if (!g.log_stem.empty()) {
                    log = widen(read_tail(g.log_stem.wstring() + L".stdout.log", 1400));
                    if (log.empty()) log = tr(L"Waiting for the game...");
                }
                text(gr, log, 1, 11, FontStyleRegular, rect(312, 412, 660, 150), kTextMuted);
            }
        }
    }
    BitBlt(target, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, previous);
    DeleteObject(buffer);
    DeleteDC(dc);
}
