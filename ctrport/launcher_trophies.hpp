// SPDX-License-Identifier: GPL-2.0-only
// TROPHIES tab, shared by both skins: the card of the selected trophy and the note under the
// key field. Without the trophy key the texts come from the Crash Bandicoot Wiki
// (trophy_wiki.hpp), credited here as CC BY-SA requires, with its "how to unlock" tips.
// Coordinates are the Classic page's (x 296..990); Nitro maps them with page_rect.
#pragma once

namespace trophy_page {
inline RectF box(float x, float y, float w, float h) {
    return classic_skin ? rect(x, y, w, h) : page_rect(x, y, w, h);
}
inline float icon_w(float w) {
    return classic_skin ? w : w / 1.73f;
} // square icons on the stretched Nitro page
} // namespace trophy_page

void trophy_card_paint(Graphics& gr) {
    using trophy_page::box;
    const auto total = g.trophy_set.trophies.size();
    if (total == 0) {
        text(gr, tr(L"No trophy file in the dump"), 1, 13.0f, FontStyleRegular, box(296, 140, 694, 24), kTextMuted);
    } else {
        const int shown = g.trophy_hot >= 0 && g.trophy_hot < static_cast<int>(total) ? g.trophy_hot : 0;
        const auto& t = g.trophy_set.trophies[static_cast<std::size_t>(shown)];
        const auto when = g.trophy_unlocked.find(t.id);
        const RectF card = box(296, 362, 694, 108);
        fill_round(gr, card, 14, SolidBrush(Color(kArcadeCard)));
        stroke_round(gr, card, 14, kArcadeDark, 3.0f);
        if (static_cast<std::size_t>(shown) < g.trophy_icons.size() && g.trophy_icons[static_cast<std::size_t>(shown)])
            gr.DrawImage(g.trophy_icons[static_cast<std::size_t>(shown)].get(),
                         box(310, 376, trophy_page::icon_w(80), 80));
        std::wstring name = t.name.empty() ? trf(L"Trophy {0}", std::to_wstring(t.id)) : widen(t.name);
        std::wstring detail = widen(t.detail);
        if (t.hidden && when == g.trophy_unlocked.end() && g.trophy_set.texts) {
            name = tr(L"Hidden trophy");
            detail.clear();
        }
        const wchar_t* grade = t.grade == 'P'   ? L"Platinum"
                               : t.grade == 'G' ? L"Gold"
                               : t.grade == 'S' ? L"Silver"
                               : t.grade == 'B' ? L"Bronze"
                                                : nullptr;
        text(gr, name, 1, 15.0f, FontStyleBold, box(404, 370, 440, 22), kTextLight);
        if (grade)
            text(gr, tr(grade), 1, 11.0f, FontStyleBold, box(850, 372, 130, 18), kArcadeGold, StringAlignmentFar);
        text(gr, detail, 1, 11.5f, FontStyleRegular, box(404, 393, 576, 18), kTextMuted);
        if (!t.tip.empty())
            text(gr, trf(L"How to unlock: {0}", widen(t.tip)), 1, 10.5f, FontStyleRegular, box(404, 413, 576, 32),
                 kArcadeCyan);
        std::wstring state = tr(L"Locked");
        if (when != g.trophy_unlocked.end()) {
            const std::time_t at = static_cast<std::time_t>(when->second);
            std::tm local{};
            wchar_t date[32]{};
            if (!localtime_s(&local, &at)) std::wcsftime(date, 32, L"%Y-%m-%d %H:%M", &local);
            state = trf(L"Unlocked {0}", date);
        }
        text(gr, state, 1, 11.0f, FontStyleBold, box(404, 447, 576, 18),
             when != g.trophy_unlocked.end() ? kArcadeLime : kTextMuted);
    }
    text(gr, tr(L"Trophy key (optional):"), 1, 11.5f, FontStyleBold, box(296, 478, 520, 18), kTextMuted);
    draw_field(gr, box(296, 500, 520, 34), g.trophy_key_edit);
    std::wstring note = g.trophy_notice;
    if (note.empty() && !g.trophy_set.texts) {
        note = g.trophy_wiki ? trf(L"Texts in English from the {0}. Your PS4 trophy key shows the game's own.",
                                   ctr_trophy::wiki::kCredit)
               : g.trophy_wiki_loading ? std::wstring(tr(L"Loading the trophy texts from the Crash Bandicoot Wiki..."))
                                       : std::wstring(tr(L"Names and descriptions need your PS4 trophy key."));
    }
    text(gr, note, 1, 11.0f, FontStyleRegular, box(296, 544, 694, 34),
         g.trophy_notice_error ? kArcadePink : kTextMuted);
}
