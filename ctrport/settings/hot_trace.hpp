// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Per-call diagnostic lines of the hottest guest services (mutex lock/unlock, clock,
// condition/event/semaphore waits, pad and file polling) are written only when
// CTR_TRACE_HOT=1. They ran tens of thousands of times per second, each line flushed
// through the shared CRT stream lock. Errors and all other logs are unaffected.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace ctrport::settings {
inline bool hot_trace() noexcept {
    static const bool enabled = [] {
        char value[2]{};
        return GetEnvironmentVariableA("CTR_TRACE_HOT", value, sizeof(value)) == 1 && value[0] == '1';
    }();
    return enabled;
}
} // namespace ctrport::settings
