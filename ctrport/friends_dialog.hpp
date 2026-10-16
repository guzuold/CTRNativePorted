// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include "online_server/launcher_online.hpp"
#include <filesystem>
#include <string>

namespace ctr_online {
class DhtService;

// Displays the Friends / Friend Code management dialog.
// Returns true if a verified/selected server IP was chosen and written to out_server_ip.
bool show_friends_dialog(HWND parent, const std::filesystem::path& root, PartySelection& selection,
                         const std::string& player_name, DhtService* shared_dht = nullptr);

} // namespace ctr_online
