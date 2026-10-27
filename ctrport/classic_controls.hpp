// SPDX-License-Identifier: GPL-2.0-only
// Original v0.6.0 presentation, retained as the Classic skin.
void classic_draw_control(const DRAWITEMSTRUCT& item) {
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
    const int control_id = GetDlgCtrlID(item.hwndItem);
    if (control.tab == TAB_SETTINGS || keyboard_section::owns(control_id) ||
        (control_id >= ID_MOD_ROW_TOGGLE && control_id < ID_MOD_ROW_DOWN + kModRows)) {
        HBRUSH card = CreateSolidBrush(RGB(42, 33, 112));
        FillRect(dc, &item.rcItem, card);
        DeleteObject(card);
    }
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
            const bool is_disabled = disabled;
            const float shadow_d = is_disabled ? 2.0f : pressed ? 1.0f : 5.0f;
            const float btn_alpha = is_disabled ? 0.35f : 1.0f;
            SolidBrush fill(Color(fade(active ? kArcadeGold : hot && !is_disabled ? 0xFF372C8F : kArcadeCard, btn_alpha)));
            draw_slanted_box(gr, rect(2.f, 2.f, lw - 14.f, lh - 8.f), 0.176f, shadow_d, fill, is_disabled ? fade(kArcadeDark, 0.4f) : kArcadeDark, is_disabled ? 2.0f : 3.0f);
            text(gr, tr(control.label), 0, 20.0f, FontStyleBold, rect(2.f, 2.f + (pressed && !is_disabled ? 2.f : 0.f), lw - 14.f, lh - 8.f),
                 is_disabled ? fade(kTextOff, 0.8f) : active ? kArcadeDark : kTextLight, StringAlignmentCenter);
            break;
        }
        case Kind::Launch: {
            // The giant PLAY! button from Design 2a
            const float shadow_d = pressed ? 2.0f : 8.0f;
            SolidBrush fill(Color(disabled ? 0xFF4A4068 : hot ? 0xFFFFD766 : kArcadeGold));
            draw_slanted_box(gr, rect(2.f, 2.f, lw - 20.f, lh - 12.f), 0.176f, shadow_d, fill, kArcadeDark, 4.0f);
            std::wstring btn_text = tr(g.process ? L"RACING" : g.xbox_busy || g.shaders_busy ? L"PLEASE WAIT" : L"PLAY!");
            if (g.shaders_busy) btn_text += L" " + std::to_wstring(g.shaders_percent) + L"%";
            text(gr, btn_text.c_str(), 0, g.shaders_busy ? 30.0f : 44.0f, FontStyleBold, rect(2.f, 2.f + (pressed ? 3.f : 0.f), lw - 20.f, lh - 12.f),
                 disabled ? kTextMuted : kArcadeDark, StringAlignmentCenter);
            break;
        }
        case Kind::ActionBtn:
        case Kind::Browse: {
            fill_round(gr, full, 10, SolidBrush(Color(fade(GetDlgCtrlID(item.hwndItem) == ID_CLOSE ? kArcadePink : hot ? 0xFF6BE1FF : kArcadeCyan, alpha))));
            stroke_round(gr, full, 10, kArcadeDark, 3.0f);
            text(gr, tr(control.label), 1, 12.0f, FontStyleBold, full, kArcadeDark, StringAlignmentCenter);
            break;
        }
        case Kind::Segment:
        case Kind::Pill: {
            const bool selected = control.kind == Kind::Segment ? g.mods_view == control.pill : g.resolution == control.pill;
            fill_round(gr, full, 8, SolidBrush(Color(fade(selected ? kArcadeGold : kArcadeCardSub, alpha))));
            stroke_round(gr, full, 8, selected ? kArcadeDark : fade(kArcadeDark, 0.6f), selected ? 2.5f : 1.5f);
            text(gr, tr(control.label), 1, 11.5f, FontStyleBold, full, selected ? kArcadeDark : kTextLight, StringAlignmentCenter);
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
            text(gr, tr(control.label), 1, 12.5f, FontStyleBold, rect(52, 0, lw - 52, lh), fade(hot ? 0xFFFFFFFF : kTextLight, alpha));
            break;
        }
        }
    }
    BitBlt(item.hDC, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, previous);
    DeleteObject(buffer);
    DeleteDC(dc);
}
