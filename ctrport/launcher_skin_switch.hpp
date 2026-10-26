// SPDX-License-Identifier: GPL-2.0-only
// Rebuild only the view. Runtime, online host and game configuration remain shared.
void apply_launcher_palette() {
    kWidth=classic_skin?1040.f:nitro::width;kHeight=classic_skin?640.f:nitro::height;
    kFriendsTravel=classic_skin?320.f:378.f;
    kArcadeBg=classic_skin?0xFF1B1446:0xFF10152C;
    kArcadeDark=classic_skin?0xFF120C30:0xFF0A1025;
    kArcadeCard=classic_skin?0xFF2A2170:0xFF1B233D;
    kArcadeCardSub=classic_skin?0xFF1B1446:0xFF10152C;
    kTextMuted=classic_skin?0xFFB9B0F0:0xFFB9C2DA;
    kTextOff=classic_skin?0xFF5A5196:0xFF56617D;
}
bool save_launcher_appearance() {
    const auto path=g.root/"runtime/user/nitro-ui.txt";
    const auto temporary=g.root/"runtime/user/nitro-ui.tmp";
    try {
        fs::create_directories(path.parent_path());
        {std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
         file<<ctrport::launcher_appearance::serialize({classic_skin,nitro_motion});
         file.close();if(!file)throw std::runtime_error("Unable to save launcher appearance.");}
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Unable to replace launcher appearance settings.");
        return true;
    }catch(const std::exception& e){MessageBoxW(g.window,widen(e.what()).c_str(),kTitle,MB_ICONERROR);return false;}
}
void switch_launcher_skin() {
    if(g.process||g.host_busy||g.xbox_busy||update_preparing)return;
    if(!save_config())return;
    classic_skin=!classic_skin;
    if(!save_launcher_appearance()){classic_skin=!classic_skin;return;}
    // Keep the old palette/layout alive until snapshots and widgets are released.
    classic_skin=!classic_skin;
    nitro_finish();reset_friends();
    SendMessageW(g.window,WM_SETREDRAW,FALSE,0);
    g.updating=true;
    while(HWND child=GetWindow(g.window,GW_CHILD))DestroyWindow(child);
    nitro_overlay=nullptr;g.friends_drawer=nullptr;
    g.friends_manage=g.friend_code_edit=g.friend_add=g.friend_remove=nullptr;
    g.controls.clear();g.pills.clear();g.tab_buttons.clear();g.hot=nullptr;update_button=nullptr;
    if(g.edit_font){DeleteObject(g.edit_font);g.edit_font=nullptr;}
    nitro_clear_surfaces();
    classic_skin=!classic_skin;apply_launcher_palette();
    RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    g.scale=std::min(float(GetDpiForSystem())/96.f,std::min((work.right-work.left-32)/kWidth,(work.bottom-work.top-40)/kHeight));
    SetWindowPos(g.window,nullptr,work.left+(work.right-work.left-px(kWidth))/2,
        work.top+std::max(0L,(work.bottom-work.top-px(kHeight))/2),px(kWidth),px(kHeight),SWP_NOZORDER|SWP_NOACTIVATE);
    SetWindowRgn(g.window,CreateRoundRectRgn(0,0,px(kWidth)+1,px(kHeight)+1,px(24),px(24)),FALSE);
    SetWindowTextW(g.window,classic_skin?L"CTR Native Ported — Classic":L"CTR Native Ported — Nitro Cinema");
    render_base_layer();create_controls();
    for(HWND child=GetWindow(g.window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT))
        SetWindowLongPtrW(child,GWL_STYLE,GetWindowLongPtrW(child,GWL_STYLE)|WS_CLIPSIBLINGS);
    g.friends_drawer=CreateWindowExW(WS_EX_CONTROLPARENT,L"CTRFriendsDrawer",L"Friends",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_CLIPSIBLINGS,0,0,0,0,g.window,nullptr,GetModuleHandleW(nullptr),nullptr);
    g.updating=false;
    select_tab_now(TAB_ADVANCED);nitro_nav_from=nitro_nav_to=float(TAB_ADVANCED);
    select_resolution(g.resolution);
    EnableWindow(update_button,!update_job.valid());
    SendMessageW(g.window,WM_SETREDRAW,TRUE,0);
    SetFocus(GetDlgItem(g.window,ID_LAUNCHER_SKIN));
    RedrawWindow(g.window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
