// SPDX-License-Identifier: GPL-2.0-only
// Own diagnostic. Briefly suspend/resume only target threads; save metadata only.
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <array>
static bool executable(HANDLE process, uint64_t address) {
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQueryEx(process, reinterpret_cast<void*>(address), &m, sizeof(m))) return false;
    return m.State == MEM_COMMIT &&
           (m.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) &&
           !(m.Protect & PAGE_GUARD);
}
int main(int argc, char** argv) {
    if (argc != 4) return 60;
    const DWORD pid = strtoul(argv[1], nullptr, 10);
    const uint64_t base = _strtoui64(argv[2], nullptr, 0), bytes = _strtoui64(argv[3], nullptr, 0);
    if (!pid || pid == GetCurrentProcessId() || !base || !bytes || bytes > 0x40000000 || base > UINT64_MAX - bytes)
        return 60;
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) return 62;
    wchar_t path[32768]{};
    DWORD length = 32768;
    if (!QueryFullProcessImageNameW(process, 0, path, &length)) {
        CloseHandle(process);
        return 62;
    }
    const wchar_t* name = wcsrchr(path, L'\\');
    if (!name || wcscmp(name + 1, L"ctrport.exe")) {
        CloseHandle(process);
        return 88;
    }
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        CloseHandle(process);
        return 62;
    }
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    unsigned captured = 0, rejected = 0;
    if (Thread32First(snapshot, &entry)) do {
            if (entry.th32OwnerProcessID != pid) continue;
            HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE,
                                       entry.th32ThreadID);
            if (!thread) {
                ++rejected;
                continue;
            }
            CONTEXT c{};
            c.ContextFlags = CONTEXT_CONTROL;
            bool valid = false;
            if (SuspendThread(thread) != DWORD(-1)) {
                valid = GetThreadContext(thread, &c) != 0;
                const DWORD resumed = ResumeThread(thread);
                if (resumed == DWORD(-1)) {
                    CloseHandle(thread);
                    CloseHandle(snapshot);
                    CloseHandle(process);
                    return 116;
                }
            }
            CloseHandle(thread);
            if (!valid) {
                ++rejected;
                continue;
            }
            // Snapshot stack AFTER resume; candidates may change. Never claim an unwind.
            std::array<uint64_t, 256> stack{};
            SIZE_T read = 0;
            ReadProcessMemory(process, reinterpret_cast<void*>(c.Rsp), stack.data(), sizeof(stack), &read);
            std::printf(
                "{\"scope\":\"native_thread_checkpoint\",\"thread\":%lu,\"rip\":%llu,\"guest_rip\":%s,\"return_candidates\":[",
                entry.th32ThreadID, static_cast<unsigned long long>(c.Rip),
                (c.Rip >= base && c.Rip < base + bytes) ? "true" : "false");
            bool comma = false;
            unsigned candidates = 0;
            for (size_t i = 0; i < read / 8 && candidates < 24; ++i) {
                const auto value = stack[i];
                if (value < base || value >= base + bytes || !executable(process, value)) continue;
                if (comma) std::printf(",");
                std::printf("%llu", static_cast<unsigned long long>(value - base));
                comma = true;
                ++candidates;
            }
            std::printf("],\"stack_payload_persisted\":false,\"unwind_verified\":false,\"resumed\":true}\n");
            ++captured;
        } while (Thread32Next(snapshot, &entry));
    CloseHandle(snapshot);
    CloseHandle(process);
    std::printf(
        "{\"scope\":\"native_thread_checkpoint_summary\",\"captured\":%u,\"rejected\":%u,\"scheduler_emulation\":false}\n",
        captured, rejected);
    return captured ? 0 : 62;
}
