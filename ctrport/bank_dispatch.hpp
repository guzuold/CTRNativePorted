// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "missing_import.hpp"
namespace ctrport {
struct BankCall {
    const MissingTrapBank* functions = nullptr;
    const MissingTrapBank* data = nullptr;
    std::size_t index = 0;
    std::uint64_t caller = 0;
    bool captured = false, data_access = false;
};
inline int capture_bank_call(EXCEPTION_POINTERS* exception, BankCall* call) noexcept {
    const auto* record = exception->ExceptionRecord;
    const auto rip = exception->ContextRecord->Rip;
    const auto begin = reinterpret_cast<std::uint64_t>(call->functions->memory.get());
    if (record->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION && rip >= begin &&
        rip - begin < call->functions->records.size() * 16 && (rip - begin) % 16 == 0) {
        SIZE_T read = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(exception->ContextRecord->Rsp),
                               &call->caller, 8, &read) ||
            read != 8)
            return EXCEPTION_CONTINUE_SEARCH;
        call->index = static_cast<std::size_t>((rip - begin) / 16);
        call->data_access = false;
    } else if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2 &&
               record->ExceptionInformation[0] <= 1) {
        const auto address = record->ExceptionInformation[1];
        const auto guard = reinterpret_cast<std::uint64_t>(call->data->memory.get());
        if (address < guard || address - guard >= call->data->records.size() * 4096) return EXCEPTION_CONTINUE_SEARCH;
        call->index = static_cast<std::size_t>((address - guard) / 4096);
        call->caller = rip;
        call->data_access = true;
    } else
        return EXCEPTION_CONTINUE_SEARCH;
    call->captured = true;
    return EXCEPTION_EXECUTE_HANDLER;
}
}
