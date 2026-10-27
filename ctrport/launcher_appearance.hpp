// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <string>
#include <string_view>
namespace ctrport::launcher_appearance {
struct Appearance {
    bool classic = false;
    bool motion = true;
};
inline Appearance parse(std::string_view text) {
    Appearance result;
    if (text.size() > 4096) return result;
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line == "skin=classic")
            result.classic = true;
        else if (line == "skin=nitro")
            result.classic = false;
        else if (line == "motion=0")
            result.motion = false;
        else if (line == "motion=1")
            result.motion = true;
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
    }
    return result;
}
inline std::string serialize(Appearance value) {
    return std::string(value.motion ? "motion=1\n" : "motion=0\n") +
           (value.classic ? "skin=classic\n" : "skin=nitro\n");
}
}
