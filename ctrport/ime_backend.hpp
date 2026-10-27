// SPDX-License-Identifier: GPL-2.0-only
// Runtime-owned Unicode text backend. Public module identity: shadPS4
// e3ce810f3a653f43ac64ebab63023de281a4103a sysmodule_table.h, id 0x96.
// This activates real Win32 EDIT storage/message processing only. PS4 dialog
// no-client status query is bound; active dialog exports remain explicit missing.
#pragma once
#include "settings/hot_trace.hpp"
#include <Windows.h>
#include <future>
#include <mutex>
#include <thread>
#include <stdexcept>
#include <string>
#include <memory>
#include "original_call.hpp"
namespace ctrport {
class ImeTextBackend {
    std::mutex gate;
    std::thread owner;
    HWND edit=nullptr;
public:
    ImeTextBackend()=default;
    ImeTextBackend(const ImeTextBackend&)=delete;
    ImeTextBackend& operator=(const ImeTextBackend&)=delete;
    ~ImeTextBackend() {
        if(edit)PostMessageW(edit,WM_CLOSE,0,0);
        if(owner.joinable())owner.join();
    }
    bool owner_is_separate() {
        std::lock_guard<std::mutex> guard(gate);
        if(!edit)throw std::runtime_error("IME text backend not loaded");
        DWORD process=0;const DWORD thread=GetWindowThreadProcessId(edit,&process);
        if(!thread || process!=GetCurrentProcessId())throw std::runtime_error("IME window owner invalid");
        return thread!=GetCurrentThreadId();
    }
    void unload() {
        std::lock_guard<std::mutex> guard(gate);
        if(!edit)return;
        // Post a queued close: a synchronous SendMessage can leave GetMessage
        // waiting after servicing a sent message, even after window destruction.
        if(!PostMessageW(edit,WM_CLOSE,0,0))throw std::runtime_error("IME queued close failed");
        owner.join();edit=nullptr;
    }
    int load() {
        std::lock_guard<std::mutex> guard(gate);
        if(edit)return 0;
        std::promise<HWND> ready;
        auto result=ready.get_future();
        owner=std::thread([signal=std::move(ready)]() mutable {
            // Unshown window: activation never opens a dialog or fabricates input.
            HWND window=CreateWindowExW(0,L"EDIT",L"",WS_OVERLAPPED|ES_MULTILINE,
                0,0,640,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            signal.set_value(window);
            if(!window)return;
            MSG message{};
            while(IsWindow(window)) {
                const auto rc=GetMessageW(&message,nullptr,0,0);
                if(rc<=0)break;
                TranslateMessage(&message);DispatchMessageW(&message);
            }
            if(IsWindow(window))DestroyWindow(window);
        });
        edit=result.get();
        if(!edit){owner.join();throw std::runtime_error("Win32 Unicode EDIT creation failed");}
        return 0;
    }
    std::wstring roundtrip_control(const std::wstring& value) {
        std::lock_guard<std::mutex> guard(gate);
        if(!edit)throw std::runtime_error("IME text backend not loaded");
        if(!SendMessageW(edit,WM_SETTEXT,0,reinterpret_cast<LPARAM>(value.c_str())))
            throw std::runtime_error("Unicode EDIT text write failed");
        const auto length=SendMessageW(edit,WM_GETTEXTLENGTH,0,0);
        if(length<0 || length>32768)throw std::runtime_error("Unicode EDIT length invalid");
        std::wstring output(static_cast<size_t>(length)+1,L'\0');
        const auto copied=SendMessageW(edit,WM_GETTEXT,output.size(),reinterpret_cast<LPARAM>(output.data()));
        if(copied!=length)throw std::runtime_error("Unicode EDIT text read incomplete");
        output.resize(static_cast<size_t>(length));return output;
    }
};
inline ImeTextBackend ime_text_backend;
// Public ime_dialog.cpp951-958: absence of an actual dialog client is None=0.
// Text backend activation is separate from a dialog. Open/input/result paths
// remain missing; a client outside this verified domain fails explicitly.
struct ImeDialogClientState { unsigned status=1; };
inline std::mutex ime_dialog_gate;
inline std::unique_ptr<ImeDialogClientState> ime_dialog_client;
inline unsigned ime_dialog_status(){
    std::lock_guard<std::mutex> lock(ime_dialog_gate);
    if(ime_dialog_client)throw std::runtime_error("IME active dialog lifecycle not implemented");
    return 0;
}
extern "C" unsigned ctr_ime_dialog_status(std::uint64_t caller){
    try{const auto status=ime_dialog_status();if(ctrport::settings::hot_trace()){std::printf("{\"scope\":\"owned_ime_dialog_status\",\"caller\":%llu,\"client_present\":false,\"result\":%u}\n",static_cast<unsigned long long>(caller),status);std::fflush(stdout);}return status;}
    catch(const std::exception& e){std::fprintf(stderr,"NID=IADmD4tScBY library=libSceImeDialog caller=0x%llx unsupported: %s\n",static_cast<unsigned long long>(caller),e.what());ExitProcess(4);}
}
extern "C" void ctr_sysv_ime_dialog_status();

}
