// SPDX-License-Identifier: GPL-2.0-only
// Two finite snapshots, one timer; a new tab replaces the current transition.
#pragma comment(lib,"msimg32.lib")
void select_tab_now(int tab);
void nitro_release_frames() {
    if(nitro_from_dc)DeleteDC(nitro_from_dc);
    if(nitro_to_dc)DeleteDC(nitro_to_dc);
    if(nitro_frame_dc)DeleteDC(nitro_frame_dc);
    nitro_from_dc=nitro_to_dc=nitro_frame_dc=nullptr;
    if(nitro_frame)DeleteObject(nitro_frame);nitro_frame=nullptr;
    if(nitro_from) DeleteObject(nitro_from);
    if(nitro_to) DeleteObject(nitro_to);
    nitro_from=nitro_to=nullptr;
}
void nitro_compose(HDC dc) {
    if(!nitro_from_dc) return;
    const float p=nitro_progress();
    const int w=px(kWidth), h=px(nitro::footer-nitro::header), y=px(nitro::header);
    BitBlt(dc,0,0,w,h,nitro_from_dc,0,y,SRCCOPY);
    if(!nitro_to_dc) return;
    const int shift=px((1-p)*28);
    BLENDFUNCTION blend{AC_SRC_OVER,0,static_cast<BYTE>(std::lround(p*255)),0};
    AlphaBlend(dc,shift,0,w-shift,h,nitro_to_dc,0,y,w-shift,h,blend);
}
LRESULT CALLBACK nitro_overlay_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_ERASEBKGND) return 1;
    if(message==WM_PRINTCLIENT) {nitro_compose(reinterpret_cast<HDC>(wp));return 0;}
    if(message==WM_PAINT) {
        PAINTSTRUCT ps{}; HDC target=BeginPaint(window,&ps);
        if(nitro_frame_dc){
            nitro_compose(nitro_frame_dc);
            BitBlt(target,0,0,px(kWidth),px(nitro::footer-nitro::header),nitro_frame_dc,0,0,SRCCOPY);
            const auto now=GetTickCount64();if(nitro_frames)nitro_max_gap=std::max(nitro_max_gap,now-nitro_last_frame);
            nitro_last_frame=now;++nitro_frames;
        }
        EndPaint(window,&ps);return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
HBITMAP nitro_capture(bool include_transition) {
    HDC screen=GetDC(g.window), dc=CreateCompatibleDC(screen);
    HBITMAP result=CreateCompatibleBitmap(screen,px(kWidth),px(kHeight));
    const auto old=SelectObject(dc,result);
    if(nitro_page_background && nitro_page_background_tab==g.current_tab){
        HDC cache=CreateCompatibleDC(screen);const auto previous=SelectObject(cache,nitro_page_background);
        BitBlt(dc,0,0,px(kWidth),px(kHeight),cache,0,0,SRCCOPY);SelectObject(cache,previous);DeleteDC(cache);
    }else paint_window(dc);
    std::vector<HWND> children;
    for(HWND h=GetWindow(g.window,GW_CHILD);h;h=GetWindow(h,GW_HWNDNEXT))
        if(IsWindowVisible(h) && (h!=nitro_overlay || include_transition)) children.push_back(h);
    for(auto it=children.rbegin();it!=children.rend();++it) {
        RECT r{};GetWindowRect(*it,&r);MapWindowPoints(nullptr,g.window,reinterpret_cast<POINT*>(&r),2);
        if(r.bottom<=px(nitro::header)||r.top>=px(nitro::footer)||r.left>=px(kWidth))continue;
        const int saved=SaveDC(dc);
        IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);
        SetViewportOrgEx(dc,r.left,r.top,nullptr);
        SendMessageW(*it,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
        RestoreDC(dc,saved);
    }
    SelectObject(dc,old);DeleteDC(dc);ReleaseDC(g.window,screen);return result;
}
void nitro_finish(bool repaint) {
    if(nitro_animating){
        const auto path=g.root/"runtime/user/nitro-animation.log";std::error_code ec;
        const bool reset=fs::exists(path,ec)&&fs::file_size(path,ec)>32768;
        std::ofstream log(path,reset?std::ios::trunc:std::ios::app);
        log<<"tab="<<g.current_tab<<" frames="<<nitro_frames<<" elapsed_ms="<<(GetTickCount64()-nitro_started)
           <<" max_gap_ms="<<nitro_max_gap<<" capture_ms="<<nitro_capture_ms<<"\n";
    }
    KillTimer(g.window,kNitroTimer);
    nitro_animating=nitro_preparing=false;nitro_nav_from=nitro_nav_to;
    nitro_finishing=true;
    if(repaint)for(HWND control:nitro_hidden_controls)ShowWindow(control,SW_SHOWNA);
    nitro_hidden_controls.clear();
    if(nitro_overlay) ShowWindow(nitro_overlay,SW_HIDE);
    if(g.window && repaint) RedrawWindow(g.window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    nitro_finishing=false;
    nitro_release_frames();
}
void nitro_begin(int tab) {
    if(tab<0 || tab>=TAB_COUNT) return;
    if(tab==g.current_tab && nitro_animating)return;
    if(tab==g.current_tab || !IsWindowVisible(g.window)) {select_tab_now(tab);nitro_nav_from=nitro_nav_to=float(tab);return;}
    BOOL system_motion=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&system_motion,0);
    if(!nitro_motion || !system_motion) {nitro_finish();select_tab_now(tab);nitro_nav_from=nitro_nav_to=float(tab);return;}
    const float prior_nav=nitro_nav_from+(nitro_nav_to-nitro_nav_from)*nitro_progress();
    const auto capture_started=GetTickCount64();
    HBITMAP from=nitro_capture(true);
    nitro_finish(false);
    nitro_from=from;
    if(!nitro_overlay) {
        WNDCLASSW wc{};wc.lpfnWndProc=nitro_overlay_proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"CTRNitroTransition";
        RegisterClassW(&wc);
        nitro_overlay=CreateWindowExW(0,wc.lpszClassName,L"",WS_CHILD|WS_CLIPSIBLINGS,0,px(nitro::header),px(kWidth),px(nitro::footer-nitro::header),g.window,nullptr,wc.hInstance,nullptr);
    }
    if(!nitro_from || !nitro_overlay) {nitro_finish();select_tab_now(tab);return;}
    HDC screen=GetDC(g.window);
    nitro_from_dc=CreateCompatibleDC(screen);SelectObject(nitro_from_dc,nitro_from);
    nitro_frame_dc=CreateCompatibleDC(screen);
    nitro_frame=CreateCompatibleBitmap(screen,px(kWidth),px(nitro::footer-nitro::header));SelectObject(nitro_frame_dc,nitro_frame);
    ReleaseDC(g.window,screen);
    if(!nitro_frame||!nitro_from_dc||!nitro_frame_dc){nitro_finish();select_tab_now(tab);return;}
    // Freeze the currently visible content BEFORE changing HWND visibility.
    // Native widgets must never paint above the transition surface.
    nitro_preparing=true;
    SetWindowPos(nitro_overlay,HWND_TOP,0,px(nitro::header),px(kWidth),px(nitro::footer-nitro::header),SWP_NOACTIVATE|SWP_SHOWWINDOW);
    InvalidateRect(nitro_overlay,nullptr,FALSE);UpdateWindow(nitro_overlay);
    select_tab_now(tab);
    nitro_nav_from=nitro_nav_to=float(tab);
    nitro_to=nitro_capture(false);
    screen=GetDC(g.window);nitro_to_dc=CreateCompatibleDC(screen);SelectObject(nitro_to_dc,nitro_to);ReleaseDC(g.window,screen);
    if(!nitro_to||!nitro_to_dc){nitro_finish();return;}
    nitro_nav_from=prior_nav;nitro_nav_to=float(tab);
    for(HWND child=GetWindow(g.window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){
        if(child==nitro_overlay||!IsWindowVisible(child))continue;
        RECT r{};GetWindowRect(child,&r);MapWindowPoints(nullptr,g.window,reinterpret_cast<POINT*>(&r),2);
        if(r.top>=px(nitro::header)&&r.top<px(nitro::footer))nitro_hidden_controls.push_back(child);
    }
    for(HWND child:nitro_hidden_controls)ShowWindow(child,SW_HIDE);
    SetWindowPos(nitro_overlay,HWND_TOP,0,px(nitro::header),px(kWidth),px(nitro::footer-nitro::header),SWP_NOACTIVATE|SWP_SHOWWINDOW);
    RedrawWindow(g.window,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW);
    nitro_capture_ms=GetTickCount64()-capture_started;
    nitro_frames=0;nitro_last_frame=nitro_max_gap=0;
    nitro_started=GetTickCount64();nitro_preparing=false;nitro_animating=true;
    SetWindowPos(nitro_overlay,HWND_TOP,0,px(nitro::header),px(kWidth),px(nitro::footer-nitro::header),SWP_NOACTIVATE|SWP_SHOWWINDOW);
    SetTimer(g.window,kNitroTimer,15,nullptr);
    InvalidateRect(nitro_overlay,nullptr,FALSE);
    UpdateWindow(nitro_overlay);
}
void nitro_tick() {
    if(!nitro_animating) return;
    if(GetTickCount64()-nitro_started>=static_cast<ULONGLONG>(nitro::duration_ms)) {InvalidateRect(nitro_overlay,nullptr,FALSE);UpdateWindow(nitro_overlay);nitro_finish();return;}
    RECT header{0,px(51),px(kWidth),px(nitro::header)};
    InvalidateRect(g.window,&header,FALSE);InvalidateRect(nitro_overlay,nullptr,FALSE);
    UpdateWindow(g.window);UpdateWindow(nitro_overlay);
}
