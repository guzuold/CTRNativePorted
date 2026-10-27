// SPDX-License-Identifier: GPL-2.0-only
// Isolated host control, not a guest pthread or scheduler implementation.
#pragma once
#include <functional>
#include <exception>
#include "thread_identity.hpp"
namespace ctrport {
struct WorkerControl {
    std::function<bool()> body;
    std::exception_ptr failure;
    bool passed = false;
    ULONG_PTR stack_low = 0, stack_high = 0, stack_marker = 0;
    DWORD host_id = 0;
    std::uint64_t runtime_identity = 0;
};

}
