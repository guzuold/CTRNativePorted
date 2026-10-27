// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../../online_server/json.hpp"
#include <vector>
#include <utility>
#include <stdexcept>
namespace ctrport::settings::pacing {
inline std::vector<std::pair<std::string, std::string>> environment(const ctr_online::Json& doc, bool extended) {
    const char* keys[] = {"presentation_limit", "window_mode", "display_refresh"};
    const char* vars[] = {"CTR_SETTINGS_PRESENTATION_LIMIT", "CTR_SETTINGS_WINDOW_MODE",
                          "CTR_SETTINGS_DISPLAY_REFRESH"};
    const std::vector<std::vector<std::string>> domains = {
        {"original", "30", "60", "90", "120", "144", "165", "240", "unlimited"},
        {"original", "windowed", "borderless", "exclusive"},
        {"original", "60", "90", "120", "144", "165", "240"}};
    std::vector<std::pair<std::string, std::string>> result;
    std::string selected[3];
    for (unsigned i = 0; i < 3; ++i) {
        const auto* p = doc.find(keys[i]);
        if (p && !p->is_string()) throw std::invalid_argument(keys[i]);
        selected[i] = p ? p->text : "original";
        bool found = false;
        for (auto& v : domains[i]) found |= selected[i] == v;
        if (!found) throw std::invalid_argument(keys[i]);
        if (!extended && selected[i] != "original") throw std::runtime_error("pacing setting requires pacing ABI 1");
        result.emplace_back(vars[i], selected[i] == "original" ? "" : selected[i]);
    }
    if (selected[2] != "original" && selected[1] != "exclusive")
        throw std::invalid_argument("display refresh requires exclusive window mode");
    return result;
}
}
