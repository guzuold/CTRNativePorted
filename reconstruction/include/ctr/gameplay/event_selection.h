// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
namespace ctr {
struct EventSelection { std::uint32_t state; std::int32_t selected; std::int32_t timestamp; };
struct EventSelectionBits { std::uint32_t first,second; };
struct EventSelectionServices {
 EventSelectionBits (*lookup)(void*)=nullptr;
 std::int32_t (*clock)(void*)=nullptr;
 void* context=nullptr;
};
// Whole00efa210; lookup must perform genuine singleton/owner table traversal.
void select_event(EventSelection&,std::uint64_t event_bits,const EventSelectionServices&);
}
