// SPDX-License-Identifier: GPL-2.0-only
#include "bridge.hpp"
extern "C" std::uint64_t ctr_settings_host_event(std::uint64_t self, std::uint64_t event,
                                                 std::uint64_t place) noexcept {
    return ctrport::settings::bridge::Adapter::dispatch_event(self, event, place);
}
extern "C" std::uint64_t ctr_settings_host_index(std::uint64_t self, std::uint64_t index, std::uint64_t place,
                                                 std::uint64_t reverse) noexcept {
    return ctrport::settings::bridge::Adapter::dispatch_index(self, index, place, reverse);
}
