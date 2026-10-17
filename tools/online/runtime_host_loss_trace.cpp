// SPDX-License-Identifier: GPL-2.0-only
// Diagnostic for the user's isolated EU retail 1.21 runtime. One-shot breakpoints:
// restore the original instruction and resume at it; no game logic is replaced.
// Arm only once the race has started. Does not terminate or focus the target.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iterator>
#include <string>

namespace {
constexpr char kRetail[] = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
struct Point { std::uintptr_t offset; const char* name; bool armed = false; unsigned char original = 0; };
std::array<Point, 4> points{{{0xa44c00, "peer_disconnect"},
                           {0xa44190, "replace_race_session"},
                           {0xa441f2, "disconnect_all_peers"},
                           {0x409390, "deactivate_transport_session"}}};
bool read(HANDLE process, std::uintptr_t address, void* data, SIZE_T size) {
    SIZE_T done = 0;
    return address && ReadProcessMemory(process, reinterpret_cast<void*>(address), data, size, &done) && done == size;
}
template<class T> T scalar(HANDLE process, std::uintptr_t address) {
    T value{}; read(process, address, &value, sizeof(value)); return value;
}
bool byte(HANDLE process, std::uintptr_t address, unsigned char value) {
    auto* p = reinterpret_cast<void*>(address); DWORD previous = 0, ignored = 0; SIZE_T done = 0;
    if (!VirtualProtectEx(process, p, 1, PAGE_EXECUTE_READWRITE, &previous)) return false;
    const bool written = WriteProcessMemory(process, p, &value, 1, &done) && done == 1;
    const bool restored = VirtualProtectEx(process, p, 1, previous, &ignored) != FALSE;
    return FlushInstructionCache(process, p, 1) && written && restored;
}
std::uintptr_t find_image(HANDLE process) {
    // Independently inspected retail instruction signatures at both function entries.
    constexpr unsigned char disconnect[]{0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x18};
    constexpr unsigned char session[]{0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50,0x49,0x89,0xfd};
    std::uintptr_t address = 0, last = 0;
    MEMORY_BASIC_INFORMATION memory{};
    while (VirtualQueryEx(process, reinterpret_cast<void*>(address), &memory, sizeof(memory))) {
        const auto allocation = reinterpret_cast<std::uintptr_t>(memory.AllocationBase);
        if (allocation && allocation != last && memory.State == MEM_COMMIT) {
            last = allocation; unsigned char a[sizeof(disconnect)]{}, b[sizeof(session)]{};
            if (read(process, allocation + 0xa44c00, a, sizeof(a)) && !std::memcmp(a, disconnect, sizeof(a)) &&
                read(process, allocation + 0xa44190, b, sizeof(b)) && !std::memcmp(b, session, sizeof(b))) return allocation;
        }
        const auto next = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
        if (next <= address) break;
        address = next;
    }
    return 0;
}
void snapshot(HANDLE process, std::uintptr_t base, const Point& point, const CONTEXT& c) {
    const auto manager = point.offset == 0x409390 ? 0 : point.offset == 0xa441f2 ? c.R13 : c.Rdi;
    std::uint64_t socket = 0, session = point.offset==0x409390 ? c.Rdi : 0; unsigned role = 0;
    const bool valid = (session && read(process,session+0x118,&role,sizeof(role))) || (manager && read(process, manager + 0x20, &socket, sizeof(socket)) &&
        socket && read(process, socket + 0x10, &session, sizeof(session)) &&
        (!session || read(process, session + 0x118, &role, sizeof(role))));
    std::printf("{\"event\":\"%s\",\"tick_ms\":%llu,\"offset\":%llu,\"rdi\":%llu,\"rsi\":%llu,"
                "\"manager\":%llu,\"socket\":%llu,\"session\":%llu,\"session_role\":%u,\"caller_offset\":%llu,\"fields_readable\":%s}\n",
                point.name, GetTickCount64(), static_cast<unsigned long long>(point.offset), c.Rdi, c.Rsi,
                manager, socket, session, role,
                point.offset == 0xa441f2 ? 0 : scalar<std::uint64_t>(process, c.Rsp) - base, valid ? "true" : "false");
    // Bounded frame-pointer observations, not a verified unwind. Keep only retail
    // code candidates; stop at unreadable, descending or out-of-stack pointers.
    auto frame=c.Rbp;
    for(unsigned depth=0;depth<12 && frame>=c.Rsp && frame-c.Rsp<0x100000 && !(frame&7);++depth) {
        std::uint64_t pair[2]{};
        if(!read(process,frame,pair,sizeof(pair)))break;
        if(pair[1]>=base && pair[1]-base<0x2000000)
            std::printf("{\"event\":\"frame_pointer_candidate\",\"at\":%llu,\"depth\":%u,\"return_offset\":%llu}\n",
                static_cast<unsigned long long>(point.offset),depth,pair[1]-base);
        if(pair[0]<=frame)break;
        frame=pair[0];
    }
    std::fflush(stdout);
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 4) { std::fprintf(stderr,"usage: runtime_host_loss_trace PID SECONDS RETAIL_SHA256\n"); return 2; }
    const DWORD pid = wcstoul(argv[1], nullptr, 10); const unsigned seconds = wcstoul(argv[2], nullptr, 10);
    const std::wstring expected(kRetail, kRetail + sizeof(kRetail) - 1);
    if (!pid || !seconds || seconds > 180 || argv[3] != expected) return 2;
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, pid);
    if (!process) return 3;
    wchar_t path[32768]{}; DWORD length = static_cast<DWORD>(std::size(path));
    if (!QueryFullProcessImageNameW(process, 0, path, &length) ||
        std::wstring(path).substr(std::wstring(path).find_last_of(L"\\/") + 1) != L"ctrport.exe") { CloseHandle(process); return 3; }
    const auto base = find_image(process);
    if (!base || !DebugActiveProcess(pid)) { CloseHandle(process); return 4; }
    if (!DebugSetProcessKillOnExit(FALSE)) { DebugActiveProcessStop(pid); CloseHandle(process); return 4; }
    bool initial = true, stopping = false, exited = false, failed = false;
    const auto deadline = GetTickCount64() + seconds * 1000ULL;
    std::printf("{\"event\":\"attached\",\"pid\":%lu,\"base\":%llu,\"expected_retail_sha256\":\"%s\",\"entry_signatures_verified\":true}\n",pid,static_cast<unsigned long long>(base),kRetail); std::fflush(stdout);
    for (;;) {
        if (!stopping && GetTickCount64() >= deadline) { stopping = true; if (!DebugBreakProcess(process)) { failed = true; break; } }
        DEBUG_EVENT e{};
        if (!WaitForDebugEvent(&e, 100)) {
            if (GetLastError() != ERROR_SEM_TIMEOUT || (stopping && GetTickCount64() > deadline + 5000)) { failed = true; break; }
            continue;
        }
        DWORD disposition = DBG_CONTINUE;
        if (e.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            if(e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile);
            CloseHandle(e.u.CreateProcessInfo.hProcess); CloseHandle(e.u.CreateProcessInfo.hThread);
        } else if (e.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) CloseHandle(e.u.CreateThread.hThread);
        else if (e.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && e.u.LoadDll.hFile) CloseHandle(e.u.LoadDll.hFile);
        else if (e.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) { exited = true; stopping = true; }
        else if (e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            const auto& exception = e.u.Exception.ExceptionRecord;
            disposition = DBG_EXCEPTION_NOT_HANDLED;
            if (exception.ExceptionCode == EXCEPTION_BREAKPOINT) {
                if (initial) {
                    initial = false; disposition = DBG_CONTINUE;
                    for (auto& p : points) {
                        if (!read(process, base + p.offset, &p.original, 1) || p.original == 0xcc) { failed = true; break; }
                        // Set armed first: even a partial write failure must attempt restoration.
                        p.armed = true;
                        if (!byte(process, base + p.offset, 0xcc)) { failed = true; break; }
                    }
                    std::puts(failed ? "{\"event\":\"arming_failed\"}" : "{\"event\":\"armed\"}"); std::fflush(stdout);
                } else for (auto& p : points) if (p.armed && reinterpret_cast<std::uintptr_t>(exception.ExceptionAddress) == base + p.offset) {
                    HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, e.dwThreadId);
                    CONTEXT c{}; c.ContextFlags = CONTEXT_FULL;
                    bool recovered = false;
                    if (thread && GetThreadContext(thread,&c)) {
                        snapshot(process,base,p,c); c.Rip=base+p.offset;
                        if (SetThreadContext(thread,&c) && byte(process,base+p.offset,p.original)) { p.armed=false; recovered=true; }
                    }
                    if (!recovered) { failed=true; std::fprintf(stderr,"Breakpoint recovery failed; exception is not swallowed.\n"); }
                    if(thread)CloseHandle(thread);
                    disposition=recovered ? DBG_CONTINUE : DBG_EXCEPTION_NOT_HANDLED; break;
                }
                if (stopping && !failed) disposition = DBG_CONTINUE;
            }
        }
        bool any=false; for(const auto& p:points)any|=p.armed;
        stopping = stopping || failed || (!initial && !any);
        if(stopping && !exited) for(auto& p:points) if(p.armed) { if(!byte(process,base+p.offset,p.original))failed=true; else p.armed=false; }
        if(!ContinueDebugEvent(e.dwProcessId,e.dwThreadId,disposition)){failed=true;break;}
        if(stopping)break;
    }
    // An unexpected wait/break failure must still restore bytes before detaching.
    if(!exited)for(auto& p:points)if(p.armed){if(!byte(process,base+p.offset,p.original))failed=true;}
    if(!exited && !DebugActiveProcessStop(pid))failed=true;
    CloseHandle(process); std::printf("{\"event\":\"detached\",\"ok\":%s}\n",failed?"false":"true");
    return failed?5:0;
}
