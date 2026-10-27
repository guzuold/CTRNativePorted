// SPDX-License-Identifier: GPL-2.0-only
// Bounded startup attributes and explicit Windows priority mapping.
#pragma once
#include "thread_attributes.hpp"
namespace ctrport {
inline void validate_startup_thread_output(std::uint64_t* output) {
    if (!output) throw std::runtime_error("Thread output handle is required");
    require_time_output(output, sizeof(*output));
}
inline ThreadAttributes snapshot_create_attributes(const std::uint64_t* cell) {
    if (!cell) return ThreadAttributes{};
    require_time_output(cell, 8);
    std::lock_guard<std::mutex> guard(thread_attributes_lock);
    if (!*cell) return ThreadAttributes{};
    const auto found = thread_attributes.find(*cell);
    if (found == thread_attributes.end()) throw std::runtime_error("Unknown create attribute handle");
    return *found->second;
}
inline void validate_startup_create_domain(const ThreadAttributes& value) {
    if (value.inherit != 0) throw std::runtime_error("Inherited scheduling attribute snapshot missing");
    if (value.policy != 1 || (value.priority < 256 || value.priority > 767))
        throw std::runtime_error("Nondefault guest scheduling domain missing");
    if (value.suspended || !value.system_scope) throw std::runtime_error("Suspended/process-scope worker missing");
    if (value.stack_address || (value.cpu_set_size && (value.cpu_set_size != 8 || !value.cpu_set)))
        throw std::runtime_error("User stack or explicit CPU set worker missing");
    if (!value.stack_size || value.stack_size > 64 * 1024 * 1024 || value.guard_size > 64 * 1024 * 1024)
        throw std::runtime_error("Worker stack domain rejected");
}
// Own monotonic five-bucket policy: lower guest priority number is higher.
// FIFO/RR domain256..767: 256..358 highest,359..460 above normal,
// 461..563 normal,564..665 below normal,666..767 lowest.
// Windows performs scheduling; no PS4 timing/order equivalence is claimed.
inline int startup_windows_priority(const ThreadAttributes& value) {
    if (value.policy != 1 || value.priority < 256 || value.priority > 767)
        throw std::runtime_error("Unsupported Windows priority mapping domain");
    const int priorities[] = {THREAD_PRIORITY_HIGHEST, THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_NORMAL,
                              THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_LOWEST};
    return priorities[((value.priority - 256) * 5) / 512];
}
}
