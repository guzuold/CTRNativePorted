// SPDX-License-Identifier: GPL-2.0-only
#include <cstring>
#include <cstdio>
void assert_fail_impl();
[[noreturn]] void unreachable_impl();
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    if (std::strcmp(argv[1], "assert") == 0) assert_fail_impl();
    else if (std::strcmp(argv[1], "unreachable") == 0) unreachable_impl();
    else return 2;
    std::fputs("ERROR: fatal returned\n", stderr);
    return 99;
}
