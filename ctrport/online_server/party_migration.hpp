// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "party_lobby.hpp"
#include "protocol.hpp"
#include "upnp.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace ctr_online {
struct RecoveryMember {
    std::string code, name, endpoint;
};
struct PartyRecovery {
    std::string room, previous_owner, successor;
    std::uint64_t generation = 0;
    std::vector<RecoveryMember> members;
};
inline bool valid_room_selector(const std::string& room) {
    return room.size() == 64 && room.find_first_not_of("0123456789abcdef") == std::string::npos;
}
inline bool valid_party_name(const std::string& name) {
    return name.size() >= 3 && name.size() <= 16 &&
           name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
               std::string::npos;
}
inline bool valid_party_endpoint(const std::string& endpoint) {
    const auto scope = ipv4_scope(endpoint);
    return !scope.empty() && scope != "unspecified" && scope != "multicast" && endpoint != "255.255.255.255";
}
// Verify against the current room's owner, never against an identity chosen by
// the supplied ticket. Existing signed-message freshness is also enforced.
inline PartyRecovery verify_party_recovery(const Json& ticket, const std::string& owner, const std::string& room,
                                           std::uint64_t now) {
    if (dump_json(ticket).size() > 16384) throw std::runtime_error("Checkpoint lobby troppo grande");
    const auto payload = verify_party_message(ticket, owner, now);
    if (payload.at("format").text != "ctr-party-recovery-v1" || payload.at("owner").text != owner ||
        !valid_room_selector(room) || payload.at("room").text != room)
        throw std::runtime_error("Checkpoint of a different room");
    PartyRecovery result;
    result.room = room;
    result.previous_owner = owner;
    result.generation = payload.at("generation").as_u64();
    if (result.generation == (std::numeric_limits<std::uint64_t>::max)())
        throw std::runtime_error("Lobby generation exhausted");
    const auto& rows = payload.at("members");
    if (!rows.is_array() || rows.items.size() < 2 || rows.items.size() > 8)
        throw std::runtime_error("Invalid checkpoint participants");
    std::set<std::string> codes, names;
    std::set<std::uint64_t> accounts;
    bool found_owner = false;
    for (const auto& row : rows.items) {
        RecoveryMember member{row.at("code").text, row.at("name").text, row.at("endpoint").text};
        FriendCodeInfo identity;
        if (parse_friend_code(member.code, identity) != CodeParseError::Ok || identity.code != member.code ||
            !valid_party_name(member.name) || (!member.endpoint.empty() && !valid_party_endpoint(member.endpoint)) ||
            !codes.insert(member.code).second || !names.insert(member.name).second ||
            !accounts.insert(user_id(member.name)).second)
            throw std::runtime_error("Invalid checkpoint identity");
        if (member.code == owner) {
            found_owner = true;
            continue;
        }
        result.members.push_back(std::move(member));
    }
    if (!found_owner) throw std::runtime_error("Proprietario assente dal checkpoint");
    std::sort(result.members.begin(), result.members.end(),
              [](const auto& a, const auto& b) { return user_id(a.name) < user_id(b.name); });
    for (const auto& member : result.members)
        if (!member.endpoint.empty()) {
            result.successor = member.code;
            break;
        }
    if (result.successor.empty()) throw std::runtime_error("No reachable successor registered");
    return result;
}
} // namespace ctr_online
