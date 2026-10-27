// SPDX-License-Identifier: GPL-2.0-only
// Existing page data/actions rendered in the new content layout.
void paint_window(HDC target) {
    if(classic_skin){classic_paint_window(target);return;}
    const int w = px(kWidth), h = px(kHeight);
    HDC dc = CreateCompatibleDC(target);
    HBITMAP buffer = CreateCompatibleBitmap(target, w, h);
    const HGDIOBJ previous = SelectObject(dc, buffer);
    HDC source = CreateCompatibleDC(target);
    const HGDIOBJ previous_source = SelectObject(source, g.base_layer);
    BitBlt(dc,0,0,w,h,source,0,0,SRCCOPY);
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
            nitro_paint_home(gr);
        } else {
            // Sub-pane background container: page_rect(270, 56, 746, 560)
            // 3a/3b/3c share the uninterrupted purple Pit Lane shell.

            if (g.current_tab == TAB_MODS) {
                mods_list_paint(gr);  // launcher_mods.hpp
            } else if (g.current_tab == TAB_TROPHIES) {
                text(gr, tr(L"TROPHIES"), 0, 36.0f, FontStyleBold, page_rect(296, 76, 300, 40), kArcadeGold);
                const auto total = g.trophy_set.trophies.size();
                draw_slanted_box(gr, page_rect(870, 84, 120, 26), 0.14f, 3.0f, SolidBrush(Color(kArcadeDark)), kArcadeCard, 2.0f);
                text(gr, std::to_wstring(g.trophy_unlocked.size()) + L" / " + std::to_wstring(total), 1, 11.0f, FontStyleBold,
                     page_rect(870, 84, 120, 26), g.trophy_unlocked.empty() ? kTextMuted : kArcadeLime, StringAlignmentCenter);
                ColorMatrix gray = {{{0.2f, 0.2f, 0.2f, 0, 0}, {0.4f, 0.4f, 0.4f, 0, 0}, {0.1f, 0.1f, 0.1f, 0, 0}, {0, 0, 0, 0.35f, 0}, {0, 0, 0, 0, 1}}};
                ImageAttributes locked;
                locked.SetColorMatrix(&gray);
                for (std::size_t i = 0; i < total && i < g.trophy_icons.size(); ++i) {
                    const float x = kTrophyLeft + (i % kTrophyColumns) * kTrophyCell, y = kTrophyTop + (i / kTrophyColumns) * kTrophyCell;
                    const bool unlocked = g.trophy_unlocked.count(g.trophy_set.trophies[i].id) > 0;
                    if (static_cast<int>(i) == g.trophy_hot) fill_round(gr, page_rect(x - 3/1.73f, y - 3, (kTrophyIcon + 6)/1.73f, kTrophyIcon + 6), 8, SolidBrush(Color(kArcadeGold)));
                    if (Bitmap* icon = g.trophy_icons[i].get()) {
                        const RectF r = page_rect(x, y, kTrophyIcon/1.73f, kTrophyIcon);
                        if (unlocked) gr.DrawImage(icon, r);
                        else gr.DrawImage(icon, r, 0, 0, static_cast<REAL>(icon->GetWidth()), static_cast<REAL>(icon->GetHeight()), UnitPixel, &locked);
                    } else {
                        fill_round(gr, page_rect(x, y, kTrophyIcon/1.73f, kTrophyIcon), 8, SolidBrush(Color(kArcadeCard)));
                    }
                }
                trophy_card_paint(gr);  // launcher_trophies.hpp
            } else if (g.current_tab == TAB_SETTINGS) {
                // the tab scrolls (mouse wheel): clipped to the page, shifted by the scroll position
                const GraphicsState unscrolled = gr.Save();
                gr.SetClip(RectF(0.f, nitro::header, kWidth, nitro::footer - nitro::header));
                gr.TranslateTransform(0, -settings_scroll);
                text(gr, tr(L"SETTINGS"), 0, 36.0f, FontStyleBold, page_rect(296, 76, 300, 40), kArcadeGold);

                // Card 1: GRAPHICS
                const RectF g_card = page_rect(296, 126, 222, 450);
                fill_round(gr, g_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, g_card, 18, kArcadeDark, 3.0f);
                text(gr, tr(L"WINDOW & RENDERER"), 0, 15.0f, FontStyleBold, page_rect(312, 142, 190, 24), kArcadeCyan);
                text(gr, tr(L"Display resolution:"), 1, 11.5f, FontStyleBold, page_rect(312, 172, 190, 18), kTextMuted);

                if (g.resolution == 4 && !g.cfg.fullscreen) {
                    draw_field(gr, page_rect(312, 314, 88, 28), g.width_edit);
                    draw_field(gr, page_rect(412, 314, 88, 28), g.height_edit);
                    text(gr, L"x", 1, 12.0f, FontStyleBold, page_rect(400, 314, 12, 28), kTextMuted, StringAlignmentCenter);
                }

                text(gr, tr(L"Renderer:"), 1, 11.5f, FontStyleBold, page_rect(312, 400, 190, 20), kTextMuted);

                // Card 2: ONLINE & MULTIPLAYER
                const RectF o_card = page_rect(532, 126, 222, 450);
                fill_round(gr, o_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, o_card, 18, kArcadeDark, 3.0f);
                text(gr, L"ONLINE & MULTI", 0, 18.0f, FontStyleBold, page_rect(548, 142, 190, 24), kArcadeGold);
                text(gr, tr(L"Network mode:"), 1, 11.5f, FontStyleBold, page_rect(548, 172, 190, 18), kTextMuted);

                text(gr, tr(L"Player name:"), 1, 11.5f, FontStyleBold, page_rect(548, 230, 190, 18), kTextMuted);
                draw_field(gr, page_rect(548, 252, 190, 32), g.online_id_edit);

                text(gr,g.cfg.online ? L"ONLINE" : L"OFFLINE",0,18,FontStyleBold,page_rect(548,490,190,28),g.cfg.online ? kArcadeLime : kTextMuted);

                // Card 3: CONTROLLER & AUDIO
                const RectF c_card = page_rect(768, 126, 222, 300);
                fill_round(gr, c_card, 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, c_card, 18, kArcadeDark, 3.0f);
                text(gr, tr(L"CONTROLLER"), 0, 18.0f, FontStyleBold, page_rect(784, 142, 190, 24), kArcadePink);
                for (int player = 0; player < 4; ++player)
                    text(gr, trf(L"Player {0}", std::to_wstring(player + 1)), 1, 10.5f, FontStyleBold, page_rect(784, 168.f + player * 46.f, 190, 16), kTextMuted);

                fill_round(gr, page_rect(768, 436, 222, 140), 18, SolidBrush(Color(kArcadeCard)));
                stroke_round(gr, page_rect(768, 436, 222, 140), 18, kArcadeDark, 3);

                text(gr, tr(L"Game language:"), 1, 11, FontStyleBold, page_rect(784, 516, 190, 20), kTextMuted);
                text(gr, L"AUDIO", 0, 18.0f, FontStyleBold, page_rect(784, 446, 100, 24), kArcadeLime);
                graphics_section_paint(gr);
                gr.Restore(unscrolled);
                graphics_section_paint_scrollbar(gr);
            } else if (g.current_tab == TAB_ADVANCED) {
                // the tab scrolls (mouse wheel) to its KEYBOARD section
                const GraphicsState unscrolled = gr.Save();
                gr.SetClip(RectF(0.f, nitro::header, kWidth, nitro::footer - nitro::header));
                gr.TranslateTransform(0, -settings_scroll);
                text(gr, tr(L"ADVANCED"), 0, 36.0f, FontStyleBold, page_rect(296, 76, 300, 40), kArcadeGold);

                text(gr, tr(L"Base game folder (v01.00, CUSA14876 or CUSA13795):"), 1, 11.5f, FontStyleBold, page_rect(296, 122, 600, 18), kTextMuted);
                draw_field(gr, page_rect(296, 144, 580, 34), g.base_edit);
                draw_status(gr, g.base_status, nitro::page_x(296), nitro::page_y(182), nitro::page_w(580));

                text(gr, tr(L"Update folder (v01.21, CUSA14876-patch or CUSA13795-patch):"), 1, 11.5f, FontStyleBold, page_rect(296, 208, 600, 18), kTextMuted);
                draw_field(gr, page_rect(296, 230, 580, 34), g.patch_edit);
                draw_status(gr, g.patch_status, nitro::page_x(296), nitro::page_y(268), nitro::page_w(580));

                // Logs pre box
                const RectF log_box = page_rect(296, 364, 694, 216);
                fill_round(gr, log_box, 14, SolidBrush(Color(0xFF0C0824)));
                stroke_round(gr, log_box, 14, kArcadeDark, 3.0f);

                text(gr, tr(L"SESSION LOG"), 1, 12, FontStyleBold, page_rect(312, 378, 660, 24), kArcadeCyan);
                std::wstring log = tr(L"No session yet.");
                if (!g.log_stem.empty()) {
                    log = widen(read_tail(g.log_stem.wstring() + L".stdout.log", 1400));
                    if (log.empty()) log = tr(L"Waiting for the game...");
                }
                text(gr, log, 1, 11, FontStyleRegular, page_rect(312, 412, 660, 150), kTextMuted);
                keyboard_section_paint(gr);
                gr.Restore(unscrolled);
                keyboard_section_paint_scrollbar(gr);
            }
        }
    }
    { Graphics chrome(dc);chrome.ScaleTransform(g.scale,g.scale);chrome.SetSmoothingMode(SmoothingModeAntiAlias);nitro_paint_chrome(chrome); }
    BitBlt(target,0,0,w,h,dc,0,0,SRCCOPY);
    if(!nitro_page_background)nitro_page_background=CreateCompatibleBitmap(target,w,h);
    HDC cache=CreateCompatibleDC(target);const auto cache_old=SelectObject(cache,nitro_page_background);
    BitBlt(cache,0,0,w,h,dc,0,0,SRCCOPY);SelectObject(cache,cache_old);DeleteDC(cache);
    nitro_page_background_tab=g.current_tab;
    SelectObject(dc, previous);
    DeleteObject(buffer);
    DeleteDC(dc);
}
