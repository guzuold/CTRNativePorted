// SPDX-License-Identifier: GPL-2.0-only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdexcept>
namespace Common {
// Match shadPS4 Windows callback ABI: EXCEPTION_POINTERS, not raw CONTEXT.
void* GetRip(void* context) {
    if (!context) throw std::invalid_argument("Missing Windows CONTEXT");
    auto* exception=static_cast<EXCEPTION_POINTERS*>(context);
    if (!exception->ContextRecord) throw std::invalid_argument("Missing Windows ContextRecord");
    return reinterpret_cast<void*>(exception->ContextRecord->Rip);
}
}
