// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "friend_identity.hpp"
#include "json.hpp"
#include <map>
#include <set>
#include <mutex>

namespace ctr_online {
// Launcher membership is distinct from the game's matchmaking lobby. The
// random selector is passed unchanged to auth3, matchmaking and peer filtering.
class PartyLobby {
public:
    explicit PartyLobby(std::string owner);
    Json apply(const std::string& caller, const Json& request, bool friend_allowed, std::uint64_t now);
    bool admits(const std::string& room, const std::string& name);
    std::map<std::string, std::size_t> expected_players();
private:
    struct Member { std::string name; std::uint64_t seen; };
    std::mutex mutex_;
    std::string owner_, room_, destination_;
    std::map<std::string, Member> members_;
    std::set<std::string> invited_;
    std::map<std::string, std::string> inbox_;
    void expire(std::uint64_t now);
    Json snapshot(const std::string& caller) const;
};
Json sign_party_message(const HostIdentity& identity, const Json& payload);
Json verify_party_message(const Json& packet, const std::string& expected_code, std::uint64_t now);
std::uint64_t party_now();
} // namespace ctr_online
