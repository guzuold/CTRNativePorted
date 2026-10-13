// SPDX-License-Identifier: GPL-2.0-only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdexcept>
namespace Common { void* GetRip(void*); }
int main() {
    try { (void)Common::GetRip(nullptr); return 1; } catch(const std::invalid_argument&) {}
    CONTEXT context{};
    EXCEPTION_POINTERS exception{};
    try { (void)Common::GetRip(&exception); return 5; } catch(const std::invalid_argument&) {}
    exception.ContextRecord=&context;
    RtlCaptureContext(&context);
    if (!context.Rip || Common::GetRip(&exception)!=reinterpret_cast<void*>(context.Rip)) return 2;
    context.Rip=0x123456789abcdef0ULL;
    if(Common::GetRip(&exception)!=reinterpret_cast<void*>(context.Rip)) return 3;
    context.Rip=0;
    return Common::GetRip(&exception)==nullptr?0:4;
}
