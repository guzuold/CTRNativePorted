// SPDX-License-Identifier: GPL-2.0-only
// Diagnostic only: one-shot INT3 restored before executing the original instruction.
#pragma once
#include "flat_loader.hpp"
namespace ctrport {
struct OriginalOncePoint {
    unsigned char* address;
    unsigned char byte;
    std::uint64_t offset;
};
inline std::vector<OriginalOncePoint> original_once_points;
inline LONG CALLBACK original_once_handler(EXCEPTION_POINTERS* event) {
    if (event->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    for (auto& point : original_once_points)
        if (event->ExceptionRecord->ExceptionAddress == point.address) {
            DWORD previous = 0;
            if (!VirtualProtect(point.address, 1, PAGE_EXECUTE_READWRITE, &previous)) ExitProcess(4);
            *point.address = point.byte;
            DWORD ignored = 0;
            if (!VirtualProtect(point.address, 1, previous, &ignored) ||
                !FlushInstructionCache(GetCurrentProcess(), point.address, 1))
                ExitProcess(4);
            auto* c = event->ContextRecord;
            std::cout << "{\"scope\":\"original_once_trace\",\"offset\":" << point.offset << ",\"rdi\":" << c->Rdi
                      << ",\"rsi\":" << c->Rsi << ",\"rdx\":" << c->Rdx << ",\"rcx\":" << c->Rcx << ",\"r8\":" << c->R8
                      << ",\"rax\":" << c->Rax << ",\"r13\":" << c->R13 << ",\"r14\":" << c->R14 << "}\n"
                      << std::flush;
            c->Rip = reinterpret_cast<DWORD64>(point.address);
            point.address = nullptr;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    return EXCEPTION_CONTINUE_SEARCH;
}
inline void install_original_once_trace(MappedImage& image, const ImageMetadata& metadata) {
    if (metadata.sha256 != "1936f5700cc590877d9bf493c6a6bb7714e84003dfd6d74a7d77b1869f71d816")
        throw std::runtime_error("Trace libc pin differs");
    if (!AddVectoredExceptionHandler(1, original_once_handler)) throw std::runtime_error("Trace handler missing");
    for (const auto offset : {0x60220ULL, 0x602a2ULL, 0x602bbULL, 0x602cdULL, 0x602f6ULL, 0x60330ULL}) {
        bool valid = false;
        for (const auto& span : metadata.executable_spans)
            if (offset >= span.first && offset < span.second) valid = true;
        if (!valid) throw std::runtime_error("Trace offset outside original code");
        auto* address = image.get() + offset;
        DWORD previous = 0;
        if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &previous))
            throw std::runtime_error("Trace permission failed");
        original_once_points.push_back({address, *address, offset});
        *address = 0xcc;
        DWORD ignored = 0;
        if (!VirtualProtect(address, 1, previous, &ignored) || !FlushInstructionCache(GetCurrentProcess(), address, 1))
            throw std::runtime_error("Trace restore permission failed");
    }
}
}
