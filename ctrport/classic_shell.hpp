// SPDX-License-Identifier: GPL-2.0-only
// Original v0.6.0 presentation, retained as the Classic skin.
void classic_render_base_layer() {
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

        if (g.logo) {
            const float ratio = std::min(236.f / g.logo->GetWidth(), 140.f / g.logo->GetHeight());
            const float lw = g.logo->GetWidth() * ratio, lh = g.logo->GetHeight() * ratio;
            gr.DrawImage(g.logo.get(), RectF(12.f + (236.f - lw) / 2, 20.f + (140.f - lh) / 2, lw, lh));
        } else {
            text(gr, L"CRASH TEAM", 0, 26, FontStyleBold, rect(26, 28, 220, 36), kArcadeGold);
            text(gr, L"RACING", 0, 40, FontStyleBold, rect(26, 60, 220, 52), kTextLight);
            draw_slanted_box(gr, rect(28, 120, 188, 28), .14f, 4, SolidBrush(Color(kArcadeCyan)), kArcadeDark, 2);
            text(gr, L"NATIVE PORTED", 1, 12, FontStyleBold, rect(28, 120, 188, 28), kArcadeDark,
                 StringAlignmentCenter);
        }

        text(gr, L"Launcher " + ctr_update::wide(ctr_update::version), 1, 11.5f, FontStyleRegular,
             rect(24, 550, 210, 24), kTextMuted);
        // Aside bottom tag (slanted -8 deg)
        draw_slanted_box(gr, rect(24, kHeight - 50, 80, 26), 0.14f, 3.0f, SolidBrush(Color(kArcadeCyan)), kArcadeDark,
                         2.0f);
        text(gr, L"V1.21", 1, 11.5f, FontStyleBold, rect(24, kHeight - 50, 80, 26), kArcadeDark, StringAlignmentCenter);

        if (g.current_tab == TAB_HOME) {
            // Main Stage diagonal background cut (clip-path: polygon(14% 0, 100% 0, 100% 100%, 0 100%))
            // Top-left at x ~ 370, bottom-left at x ~ 260
            GraphicsPath cut_path;
            PointF cut_pts[] = {PointF(370.f, 0.f), PointF(kWidth, 0.f), PointF(kWidth, kHeight),
                                PointF(260.f, kHeight)};
            cut_path.AddPolygon(cut_pts, 4);

            gr.SetClip(&cut_path);
            // Stage cyan background
            SolidBrush cyan_stage{Color(kArcadeCyan)};
            gr.FillRectangle(&cyan_stage, 260.f, 0.f, kWidth - 260.f, kHeight);

            if (g.art) {
                // Original user-supplied artwork stays in the dump, loaded read-only.
                draw_cover(gr, *g.art, rect(260, 0, kWidth - 260, kHeight));
            } else {
                // Procedural waving chequered flag when no dump artwork is available.
                const auto point = [](int col, int row) {
                    const float x = 170.f + col * 72.f;
                    return PointF(x, -100.f + row * 72.f + 30.f * std::sin(col * .52f + row * .18f));
                };
                for (int row = 0; row < 12; ++row)
                    for (int col = 0; col < 14; ++col) {
                        PointF tile[] = {point(col, row), point(col + 1, row), point(col + 1, row + 1),
                                         point(col, row + 1)};
                        const BYTE shade = static_cast<BYTE>(210 + 35 * std::cos(col * .52f + row * .18f));
                        SolidBrush fill((row + col) % 2 ? Color(255, shade, static_cast<BYTE>(shade - 6),
                                                                static_cast<BYTE>(shade - 16))
                                                        : Color(255, 27, 20, 62));
                        gr.FillPolygon(&fill, tile, 4);
                        Pen seam(&fill, 1.f);
                        gr.DrawPolygon(&seam, tile, 4);
                    }
            }
            gr.ResetClip();
        }
        stroke_round(gr, rect(2, 2, kWidth - 4, kHeight - 4), 26, kArcadeDark, 4);
    }
    SelectObject(dc, previous);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}
