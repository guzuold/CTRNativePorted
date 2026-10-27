// SPDX-License-Identifier: GPL-2.0-only
#include "launcher_appearance.hpp"
#include <cassert>
#include <cstdio>
using namespace ctrport::launcher_appearance;
int main() {
    auto p = parse("");
    assert(!p.classic && p.motion);
    p = parse("motion=0");
    assert(!p.classic && !p.motion); // Existing trial preference.
    p = parse("motion=1\r\nskin=classic\r\n");
    assert(p.classic && p.motion);
    p = parse("skin=unknown\nmotion=garbage\n");
    assert(!p.classic && p.motion);
    p = parse("skin=classic\nskin=nitro\nmotion=0\n");
    assert(!p.classic && !p.motion);
    p = parse(std::string(4097, 'x'));
    assert(!p.classic && p.motion);
    for (bool classic : {false, true})
        for (bool motion : {false, true}) {
            const auto saved = serialize({classic, motion});
            p = parse(saved);
            assert(p.classic == classic && p.motion == motion);
            assert(saved.find(classic ? "skin=classic" : "skin=nitro") != std::string::npos);
        }
    std::puts("LAUNCHER_APPEARANCE_OK");
}
