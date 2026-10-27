// SPDX-License-Identifier: GPL-2.0-only
void create_controls() {
    if(classic_skin){classic_create_controls();return;}
    g.edit_font = CreateFontW(-px(14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    for(int i=0;i<TAB_COUNT;++i)
        g.tab_buttons.push_back(add_button(ID_TAB_BASE+i,Kind::TabBtn,rect(nitro::nav_left[i],12,nitro::nav_width[i],40),kTabNames[i],nullptr,i,-1));
    add_button(ID_NITRO_FRIENDS,Kind::ActionBtn,rect(1018,13,116,38),L"FRIENDS");
    add_button(ID_MINIMIZE,Kind::ActionBtn,rect(1171,13,38,38),L"–");
    add_button(ID_CLOSE,Kind::ActionBtn,rect(1221,13,38,38),L"×");
    update_button=add_button(ID_CHECK_UPDATE,Kind::ActionBtn,rect(865,671,145,31),update_label.c_str());
    g.launch=add_button(ID_LAUNCH,Kind::Launch,rect(40,520,432,105),L"PLAY",nullptr,-1,TAB_HOME);
    // --- TAB 1: MODS ---
    add_button(ID_MODS_REFRESH, Kind::ActionBtn, page_rect(738, 78, 106, 36), L"REFRESH", nullptr, -1, TAB_MODS);
    add_button(ID_OPEN_MODS, Kind::ActionBtn, page_rect(854, 78, 136, 36), L"OPEN FOLDER", nullptr, -1, TAB_MODS);
    add_button(ID_MODS_FILES, Kind::Segment, page_rect(296, 128, 140, 32), L"FILE MODS", nullptr, 0, TAB_MODS);
    add_button(ID_MODS_PATCHES, Kind::Segment, page_rect(442, 128, 140, 32), L"PATCHES", nullptr, 1, TAB_MODS);
    add_button(ID_MODS_MASTER, Kind::Toggle, page_rect(770, 130, 220, 28), L"Use mods and patches", &g.cfg.mods_enabled, -1, TAB_MODS);
    // Row slots of the MODS list, placed on the visible rows by sync_mod_rows (launcher_mods.hpp).
    for (int row = 0; row < kModRows; ++row) {
        g.mod_row_up[row] = add_button(ID_MOD_ROW_UP + row, Kind::ActionBtn, page_rect(836, 176, 36, 28), L"▲", nullptr, -1, TAB_MODS);
        g.mod_row_down[row] = add_button(ID_MOD_ROW_DOWN + row, Kind::ActionBtn, page_rect(878, 176, 36, 28), L"▼", nullptr, -1, TAB_MODS);
        g.mod_row_toggle[row] = add_button(ID_MOD_ROW_TOGGLE + row, Kind::Toggle, page_rect(930, 176, 54, 32), L"", &g.mod_row_on[row], -1, TAB_MODS);
        g.gb_row_page[row] = add_button(ID_GB_ROW_PAGE + row, Kind::ActionBtn, page_rect(790, 176, 84, 28), L"PAGE", nullptr, -1, TAB_MODS);
        g.gb_row_get[row] = add_button(ID_GB_ROW_GET + row, Kind::ActionBtn, page_rect(882, 176, 102, 28), L"INSTALL", nullptr, -1, TAB_MODS);
    }
    reload_mods();

    // --- TROPHIES ---
    g.trophy_key_edit = add_edit(ID_TROPHY_KEY_EDIT, page_rect(296, 500, 520, 34), g.cfg.trophy_key, ES_PASSWORD);
    add_button(ID_TROPHY_IMPORT, Kind::ActionBtn, page_rect(830, 500, 160, 34), L"IMPORT FROM shadPS4", nullptr, -1, TAB_TROPHIES);
    reload_trophies();

    // --- TAB 2: SETTINGS ---
    // Graphics: Resolution pills
    for (int i = 0; i < 5; ++i)
        g.pills.push_back(add_button(ID_PILL + i, Kind::Pill, page_rect(312 + (i % 2) * 96.f, 196.f + (i / 2) * 36.f, 90, 30), kPills[i], nullptr, i, TAB_SETTINGS));

    g.width_edit = add_edit(ID_WIDTH, page_rect(312, 314, 88, 28), g.cfg.custom_w, ES_NUMBER | ES_CENTER);
    g.height_edit = add_edit(ID_HEIGHT, page_rect(412, 314, 88, 28), g.cfg.custom_h, ES_NUMBER | ES_CENTER);

    // 60 FPS mode, upscaling method and mode: in the GRAPHICS section below (launcher_graphics.hpp)
    add_button(ID_FULLSCREEN, Kind::Toggle, page_rect(312, 356, 190, 26), L"Fullscreen Mode", &g.cfg.fullscreen, -1, TAB_SETTINGS);
    g.scene_combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        px(nitro::page_x(312)), px(nitro::page_y(488)), px(nitro::page_w(190)), px(220), g.window, reinterpret_cast<HMENU>(ID_SCENE_RES), nullptr, nullptr);
    SendMessageW(g.scene_combo, WM_SETFONT, reinterpret_cast<WPARAM>(g.edit_font), FALSE);
    for (int i = 0; i < static_cast<int>(std::size(kModes)); ++i) {
        SendMessageW(g.scene_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(kModeLabels[i])));
        if (g.cfg.upscaling_mode == kModes[i]) SendMessageW(g.scene_combo, CB_SETCURSEL, i, 0);
    }

    g.upscaler_combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        px(nitro::page_x(312)), px(nitro::page_y(436)), px(nitro::page_w(190)), px(170), g.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_UPSCALER)), nullptr, nullptr);
    SendMessageW(g.upscaler_combo, WM_SETFONT, reinterpret_cast<WPARAM>(g.edit_font), FALSE);
    for (int i = 0; i < static_cast<int>(std::size(kUpscalers)); ++i) {
        SendMessageW(g.upscaler_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(tr(kUpscalerLabels[i])));
        if (g.cfg.upscaler == kUpscalers[i]) SendMessageW(g.upscaler_combo, CB_SETCURSEL, i, 0);
    }
    g.renderer_combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        px(nitro::page_x(312)), px(nitro::page_y(420)), px(nitro::page_w(190)), px(120), g.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_RENDERER)), nullptr, nullptr);
    SendMessageW(g.renderer_combo, WM_SETFONT, reinterpret_cast<WPARAM>(g.edit_font), FALSE);
    for (int i = 0; i < static_cast<int>(std::size(kRenderers)); ++i) {
        SendMessageW(g.renderer_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kRendererLabels[i]));
        if (g.cfg.renderer == kRenderers[i]) SendMessageW(g.renderer_combo, CB_SETCURSEL, i, 0);
    }

    // Online Section (Card 2)
    g.online_toggle = add_button(ID_ONLINE_TOGGLE, Kind::Toggle, page_rect(548, 196, 190, 26), L"Multiplayer Online", &g.cfg.online, -1, TAB_SETTINGS);
    g.online_id_edit = add_edit(ID_ONLINE_ID_EDIT, page_rect(548, 252, 190, 32), g.cfg.online_id);
    SendMessageW(g.online_id_edit, EM_LIMITTEXT, 16, 0);


    add_button(ID_WUMPA_X100, Kind::Toggle, page_rect(548, 426, 190, 26), L"Wumpa Coins x100", &g.cfg.wumpa_x100, -1, TAB_SETTINGS);

    // Controller & Audio (Card 3)
    for (int player = 0; player < 4; ++player) {
        g.player_combo[player] = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            px(nitro::page_x(784)), px(nitro::page_y(185.f + player * 46.f)), px(nitro::page_w(190)), px(220), g.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PLAYER_PAD + player)), nullptr, nullptr);
        SendMessageW(g.player_combo[player], WM_SETFONT, reinterpret_cast<WPARAM>(g.edit_font), FALSE);
    }
    refresh_pads();
    add_button(ID_XBOX_PROMPTS, Kind::Toggle, page_rect(784, 362, 190, 26), L"Colored Xbox icons", &g.cfg.xbox_prompts, -1, TAB_SETTINGS);
    add_button(ID_SPLIT_SCREEN, Kind::Toggle, page_rect(784, 392, 190, 26), L"Split screen (2-4)", &g.cfg.split_screen, -1, TAB_SETTINGS);
    add_button(ID_AUDIO_TEST, Kind::ActionBtn, page_rect(850, 444, 126, 30), L"Test audio", nullptr, -1, TAB_SETTINGS);

    g.language_combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        px(nitro::page_x(784)), px(nitro::page_y(538)), px(nitro::page_w(190)), px(200), g.window, reinterpret_cast<HMENU>(ID_GAME_LANGUAGE), nullptr, nullptr);
    SendMessageW(g.language_combo, WM_SETFONT, reinterpret_cast<WPARAM>(g.edit_font), FALSE);
    for (const auto& language : kGameLanguages) SendMessageW(g.language_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(language.label));
    SendMessageW(g.language_combo, CB_SETCURSEL, game_language_index(g.cfg.game_language), 0);

    add_button(ID_LAUNCHER_SKIN,Kind::ActionBtn,page_rect(580,78,250,36),L"Look: Nitro Cinema",nullptr,-1,TAB_ADVANCED);
    add_button(ID_NITRO_MOTION,Kind::ActionBtn,page_rect(840,78,150,36),L"Animations",nullptr,-1,TAB_ADVANCED);
    // --- TAB 3: ADVANCED ---
    g.base_edit = add_edit(ID_BASE_EDIT, page_rect(296, 144, 580, 34), g.cfg.base_dir);
    add_button(ID_BASE_BROWSE, Kind::ActionBtn, page_rect(890, 144, 100, 34), L"BROWSE", nullptr, -1, TAB_ADVANCED);

    g.patch_edit = add_edit(ID_PATCH_EDIT, page_rect(296, 230, 580, 34), g.cfg.patch_dir);
    g.patch_browse = add_button(ID_PATCH_BROWSE, Kind::ActionBtn, page_rect(890, 230, 100, 34), L"BROWSE", nullptr, -1, TAB_ADVANCED);

    add_button(ID_SAVE_BACKUP, Kind::ActionBtn, page_rect(296, 304, 210, 36), L"Back up saves", nullptr, -1, TAB_ADVANCED);
    add_button(ID_OPEN_SAVES, Kind::ActionBtn, page_rect(520, 304, 210, 36), L"Open saves", nullptr, -1, TAB_ADVANCED);
    add_button(ID_OPEN_LOGS, Kind::ActionBtn, page_rect(744, 304, 246, 36), L"Open logs folder", nullptr, -1, TAB_ADVANCED);
    graphics_section_create();
    keyboard_section_create();
}
