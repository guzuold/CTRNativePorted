// SPDX-License-Identifier: GPL-2.0-only
// Host policy for upstream shader assertions: fatal, with no emulator singleton.
#include <windows.h>
#include <cstdio>

namespace {
[[noreturn]] void shader_fatal(const char* reason) {
    std::fprintf(stderr, "CTR graphics fatal: %s\n", reason);
    std::fflush(stderr);
    // ExitProcess cannot resume the shader after an assertion or unreachable path.
    ::ExitProcess(86);
}
}

void assert_fail_impl() {
    shader_fatal("assert_fail_impl");
}
[[noreturn]] void unreachable_impl() {
    shader_fatal("unreachable_impl");
}
