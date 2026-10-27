// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <chrono>
#include <cstdlib>
#include <cstdio>
// Host wall time only; fence wait includes queued GPU work. No GPU timestamp claim.
struct CtrTransferPhaseTiming {
    using Clock = std::chrono::steady_clock;
    bool enabled = false;
    const char* operation;
    unsigned long long bytes;
    int status = -1; // -1 means phase result is not yet known; cleanup reports final return code.
    const char* phase = "allocate";
    Clock::time_point last{};
    CtrTransferPhaseTiming(const char* op, unsigned long long size) : operation(op), bytes(size) {
        char value[16]{};
        size_t length = 0;
        enabled = getenv_s(&length, value, sizeof(value), "CTR_TILING_TIMING") == 0 && length == 2 && value[0] == '1';
        if (enabled) last = Clock::now();
    }
    void Next(const char* next) {
        if (enabled) {
            const auto now = Clock::now();
            std::fprintf(stderr, "CTR_TRANSFER_PHASE operation=%s bytes=%llu phase=%s ms=%.6f status=%d\n", operation,
                         bytes, phase, std::chrono::duration<double, std::milli>(now - last).count(), status);
            last = now;
        }
        phase = next;
    }
    ~CtrTransferPhaseTiming() { Next("done"); }
};
