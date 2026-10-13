// SPDX-License-Identifier: GPL-2.0-or-later
// ctrport: GPU library assertion failures stop the runtime explicitly with exit code 23.
// Adapted from bbport gpu/shim/common/assert.cpp (GPL-2.0).
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "common/assert.h"
#include "common/logging/log.h"

void assert_fail_impl() {
    std::fflush(stdout);
    std::fputs("STOP: GPU library assertion failed (see GPU log above)\n", stderr);
    std::fflush(stderr);
    std::_Exit(23);
}

[[noreturn]] void unreachable_impl() {
    assert_fail_impl();
    throw std::runtime_error("Unreachable code");
}

void assert_fail_debug_msg(const char* msg) {
    LOG_CRITICAL(Debug, "Assertion Failed!\n{}", msg);
    assert_fail_impl();
}
