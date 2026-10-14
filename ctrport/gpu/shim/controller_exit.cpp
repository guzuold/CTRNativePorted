// SPDX-License-Identifier: GPL-2.0-only
#include <windows.h>
#include <Xinput.h>
#include <atomic>
#include <cstdio>
#include <SDL3/SDL.h>
#include "controller_exit_state.h"
#include "ctr_gpu.h"
namespace {
Frontend::ExitGesture gesture;
std::atomic<bool> captured{false};
HWND dialog{};
bool requested=false;
void dismiss(){if(dialog){DestroyWindow(dialog);dialog=nullptr;}}
LRESULT CALLBACK exit_proc(HWND hwnd,UINT message,WPARAM w,LPARAM l){
    if(message==WM_COMMAND){
        if(LOWORD(w)==IDOK){requested=true;return 0;}
        if(LOWORD(w)==IDCANCEL){gesture.cancel();dismiss();return 0;}
    }
    if(message==WM_CLOSE){gesture.cancel();dismiss();return 0;}
    return DefWindowProcW(hwnd,message,w,l);
}
void show(HWND owner){
    static bool registered=[] {WNDCLASSW c{};c.lpfnWndProc=exit_proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"CTRPortExitConfirmation";c.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);c.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));return RegisterClassW(&c)!=0;}();
    if(!registered){gesture.cancel();std::fputs("ctrport: exit dialog registration failed\n",stderr);return;}
    RECT r{};GetWindowRect(owner,&r);
    dialog=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,L"CTRPortExitConfirmation",L"Esci dal gioco",WS_CAPTION|WS_SYSMENU|WS_VISIBLE,
        r.left+(r.right-r.left-520)/2,r.top+(r.bottom-r.top-220)/2,520,220,owner,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!dialog){gesture.cancel();return;}
    auto child=[&](const wchar_t* type,const wchar_t* text,int x,int y,int width,int height,int id){
        HWND h=CreateWindowW(type,text,WS_CHILD|WS_VISIBLE|(id?WS_TABSTOP:0),x,y,width,height,dialog,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return h;
    };
    child(L"STATIC",L"Vuoi chiudere il gioco?\nAttendi la fine di eventuali salvataggi.\nRilascia i pulsanti, poi premi A per uscire o B per annullare.",20,20,475,80,0);
    child(L"BUTTON",L"A - Esci",40,125,190,36,IDOK);
    HWND cancel=child(L"BUTTON",L"B - Annulla",270,125,190,36,IDCANCEL);
    SetForegroundWindow(dialog);SetFocus(cancel);
    std::fputs("ctrport: controller exit confirmation opened\n",stderr);
}
}
extern "C" CTR_GPU_API int ctr_gpu_input_captured(){return captured.load(std::memory_order_relaxed)?1:0;}
namespace Frontend {
bool PollControllerExit(SDL_Window* window){
    using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
    static auto poll=[] {auto module=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);return module?reinterpret_cast<GetState>(GetProcAddress(module,"XInputGetState")):nullptr;}();
    HWND owner=static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(window),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr));
    HWND foreground=GetForegroundWindow();bool focused=foreground==owner || (dialog && foreground==dialog);
    XINPUT_STATE state{};bool connected=poll && poll(0,&state)==ERROR_SUCCESS;
    const auto action=gesture.update(state.Gamepad.wButtons,connected,focused,GetTickCount64());
    if(action==ExitGesture::Action::Open)show(owner);
    if(action==ExitGesture::Action::Cancel){dismiss();std::fputs("ctrport: controller exit cancelled\n",stderr);}
    if(action==ExitGesture::Action::Confirm)requested=true;
    captured.store(gesture.captured,std::memory_order_relaxed);
    if(requested){std::fputs("ctrport: controller exit confirmed\n",stderr);dismiss();return true;}
    return false;
}
}

