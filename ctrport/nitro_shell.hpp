// SPDX-License-Identifier: GPL-2.0-only
// Nitro Cinema shell. Game artwork is read from the user's dump only.
RectF page_rect(float x, float y, float w, float h) {
    return rect(nitro::page_x(x), nitro::page_y(y), nitro::page_w(w), h);
}
void nitro_clear_surfaces() {
    if (g.base_layer && g.base_layer != nitro_bases[0] && g.base_layer != nitro_bases[1]) DeleteObject(g.base_layer);
    for (auto& bitmap : nitro_bases) {
        if (bitmap) DeleteObject(bitmap);
        bitmap = nullptr;
    }
    if (nitro_page_background) DeleteObject(nitro_page_background);
    nitro_page_background = nullptr;
    nitro_page_background_tab = -1;
    g.base_layer = nullptr;
}
void render_base_layer(bool navigation = false, int requested = -1) {
    if (classic_skin) {
        classic_render_base_layer();
        return;
    }
    if (!navigation) nitro_clear_surfaces();
    const int index = requested >= 0 ? requested : g.current_tab == TAB_HOME ? 0 : 1;
    if (nitro_bases[index]) {
        g.base_layer = nitro_bases[index];
        return;
    }
    HDC screen = GetDC(nullptr);
    g.base_layer = CreateCompatibleBitmap(screen, px(kWidth), px(kHeight));
    HBITMAP high = CreateCompatibleBitmap(screen, px(kWidth) * nitro::samples, px(kHeight) * nitro::samples);
    HDC dc = CreateCompatibleDC(screen);
    const auto old = SelectObject(dc, high);
    {
        Graphics gr(dc);
        gr.SetSmoothingMode(SmoothingModeAntiAlias);
        gr.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        gr.SetPixelOffsetMode(PixelOffsetModeHalf);
        gr.ScaleTransform(g.scale * nitro::samples, g.scale * nitro::samples);
        gr.Clear(Color(kArcadeBg));
        const RectF stage(0, nitro::header, kWidth, kHeight - nitro::header);
        if (index == 0) {
            if (g.art) {
                const auto crop =
                    nitro::crop(float(g.art->GetWidth()), float(g.art->GetHeight()), stage.Width, stage.Height);
                ImageAttributes attr;
                attr.SetWrapMode(WrapModeTileFlipXY);
                gr.DrawImage(g.art.get(), stage, crop.x, crop.y, crop.w, crop.h, UnitPixel, &attr);
            } else {
                LinearGradientBrush blank(PointF(0, 86), PointF(kWidth, 676), Color(0xff171642), Color(0xff085b7e));
                gr.FillRectangle(&blank, stage);
            }
            LinearGradientBrush shade(PointF(0, 0), PointF(kWidth, 0), Color(0xf809102a), Color(0x0009102a));
            Color colors[] = {Color(0xf009102a), Color(0x8a09102a), Color(0x0009102a), Color(0x0009102a)};
            REAL positions[] = {0, .23f, .56f, 1};
            shade.SetInterpolationColors(colors, positions, 4);
            gr.FillRectangle(&shade, stage);
            LinearGradientBrush bottom(PointF(0, 500), PointF(0, 720), Color(0x0009102a), Color(0x5009102a));
            gr.FillRectangle(&bottom, 0.f, 500.f, kWidth, 220.f);
        } else {
            LinearGradientBrush background(PointF(0, 86), PointF(kWidth, 676), Color(0xff141630), Color(0xff0d1025));
            gr.FillRectangle(&background, stage);
        }
        SolidBrush bar(Color(0xff101229));
        gr.FillRectangle(&bar, 0.f, 0.f, kWidth, nitro::header);
        LinearGradientBrush footer(PointF(0, 664), PointF(0, 708), Color(0xf0090e28), Color(0xfa090c24));
        fill_round(gr, rect(11, 664, 1258, 44), 14, footer);
        stroke_round(gr, rect(11, 664, 1258, 44), 14, 0xff343961, .8f);
        Pen edge(Color(0xff30365d), 1);
        gr.DrawLine(&edge, 0.f, nitro::header, kWidth, nitro::header);
        gr.DrawLine(&edge, 961.f, 16.f, 961.f, 48.f);
        gr.DrawLine(&edge, 1153.f, 16.f, 1153.f, 48.f);
        for (float x : {850.f, 1020.f, 1158.f}) gr.DrawLine(&edge, x, 676.f, x, 697.f);
        if (g.logo) {
            const float ratio = std::min(174.f / g.logo->GetWidth(), 96.f / g.logo->GetHeight());
            const float w = g.logo->GetWidth() * ratio, h = g.logo->GetHeight() * ratio;
            gr.DrawImage(g.logo.get(), RectF(33, 6, w, h));
        } else
            text(gr, L"CTR NATIVE PORTED", 0, 22, FontStyleBold, rect(22, 12, 190, 60), kArcadeGold);
    }
    HDC resolved = CreateCompatibleDC(screen);
    const auto prior = SelectObject(resolved, g.base_layer);
    SelectObject(dc, old);
    nitro_resolve(resolved, high, px(kWidth), px(kHeight));
    SelectObject(resolved, prior);
    DeleteDC(resolved);
    DeleteObject(high);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    nitro_bases[index] = g.base_layer;
    if (!navigation) {
        render_base_layer(true, 1 - index);
        g.base_layer = nitro_bases[index];
    }
}
void nitro_paint_chrome(Graphics& gr) {
    const float selected = nitro_nav_from + (nitro_nav_to - nitro_nav_from) * nitro_progress();
    fill_round(gr, rect(nitro::nav_center(selected) - 33, 53, 66, 5), 2.5f, SolidBrush(Color(kArcadeGold)));
    std::wstring summary = g.cfg.resolution.substr(0, g.cfg.resolution.find(L' '));
    summary += std::wstring(L"  ·  ") + tr(g.cfg.fps_60 ? L"60 FPS limit" : L"30 FPS limit");
    // Small vector gear; avoids symbol-font substitution at fractional DPI.
    const auto gear = gr.Save();
    gr.TranslateTransform(38, 686);
    Pen teeth(Color(kTextLight), 3);
    for (int i = 0; i < 8; ++i) {
        gr.RotateTransform(45);
        gr.DrawLine(&teeth, 0.f, -6.f, 0.f, -10.f);
    }
    Pen rim(Color(kTextLight), 2.8f);
    gr.DrawEllipse(&rim, -6.f, -6.f, 12.f, 12.f);
    gr.Restore(gear);
    text(gr, tr(L"Configuration") + std::wstring(L"  ·  ") + summary, 2, 15, FontStyleRegular, rect(63, 674, 770, 25),
         kTextLight);
    text(gr, L"Launcher " + ctr_update::wide(ctr_update::version), 2, 14, FontStyleItalic, rect(1033, 674, 114, 25),
         kTextLight, StringAlignmentCenter);
    text(gr, trf(L"Game {0}", L"1.21"), 2, 14, FontStyleItalic, rect(1168, 674, 90, 25), kTextLight,
         StringAlignmentCenter);
}
void nitro_paint_home(Graphics& gr) {
    const auto color = g.can_launch ? kArcadeCyan : kArcadeOrange;
    const std::wstring shaders = trf(L"Translating shaders {0}%", std::to_wstring(g.shaders_percent));
    text(gr,
         g.process        ? tr(L"Game running")
         : g.shaders_busy ? shaders.c_str()
         : g.xbox_busy    ? tr(L"Preparing")
         : g.can_launch   ? tr(L"Ready to play")
                          : tr(L"Set up the game path"),
         2, 25, FontStyleBold | FontStyleItalic, rect(70, 476, 560, 38), color);
    if (!g.can_launch)
        text(gr, tr(L"Open Advanced to select your dump."), 2, 13, FontStyleRegular, rect(48, 627, 670, 24),
             kTextMuted);
}
