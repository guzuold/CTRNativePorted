// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "launcher_online.hpp"
#include "room_invite.hpp"

namespace ctr_online {
// Blocking room operations; the drawer executes these off the UI thread.
// 110: share, 111: join, 109: leave and return to local hosting.
struct LobbyActionResult {int action=0;bool ok=false;std::string detail;PartySelection party;};
} // namespace ctr_online
