// SPDX-License-Identifier: GPL-2.0-or-later
#include "party_lobby.hpp"
#include "crypto.hpp"
#include "party_migration.hpp"
#include <ctime>
#include <stdexcept>
#include <algorithm>
namespace ctr_online {
std::uint64_t party_now() { return static_cast<std::uint64_t>(std::time(nullptr)); }
Json sign_party_message(const HostIdentity& identity, const Json& payload) {
    const std::string body = dump_json(payload);
    const auto timestamp = party_now();
    Json packet = Json::object();
    packet.set("code", Json::of(identity.friend_code()));
    packet.set("payload", Json::of(body));
    packet.set("time", Json::of(timestamp));
    packet.set("signature", Json::of(hex(identity.sign_challenge("party-v1:" + hex(sha256(bytes_of(body))), {}, timestamp))));
    return packet;
}
Json verify_party_message(const Json& packet, const std::string& expected_code, std::uint64_t now) {
    FriendCodeInfo identity;
    if (parse_friend_code(expected_code, identity) != CodeParseError::Ok || packet.at("code").text != identity.code)
        throw std::runtime_error("Invalid lobby identity");
    const auto timestamp = packet.at("time").as_u64();
    if ((timestamp > now ? timestamp - now : now - timestamp) > 60)
        throw std::runtime_error("Messaggio lobby scaduto");
    const auto& body = packet.at("payload").text;
    if (!verify_friend_response(identity.public_key, "party-v1:" + hex(sha256(bytes_of(body))), {}, timestamp,
                                from_hex(packet.at("signature").text)))
        throw std::runtime_error("Invalid lobby signature");
    return parse_json(body);
}
PartyLobby::PartyLobby(std::string owner) : owner_(std::move(owner)), room_(hex(random_bytes(32))), destination_(owner_) {}
Json PartyLobby::recovery_checkpoint(const std::string& caller) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(destination_!=owner_ || !members_.count(caller))throw std::runtime_error("Checkpoint reserved for members");
    Json result=Json::object(),people=Json::array();
    result.set("format",Json::of("ctr-party-recovery-v1"));result.set("owner",Json::of(owner_));
    result.set("room",Json::of(room_));result.set("generation",Json::of(generation_));
    for(const auto& [code,member]:members_) {
        Json row=Json::object();row.set("code",Json::of(code));row.set("name",Json::of(member.name));
        row.set("endpoint",Json::of(member.endpoint));people.items.push_back(std::move(row));
    }
    result.set("members",std::move(people));return result;
}
void PartyLobby::expire(std::uint64_t now) {
    for (auto it=invited_.begin(); it!=invited_.end();) {
        if (now>it->second.created && now-it->second.created>900) it=invited_.erase(it); else ++it;
    }
    for (auto it=inbox_seen_.begin(); it!=inbox_seen_.end();) {
        if (now>it->second && now-it->second>900) { inbox_.erase(it->first); it=inbox_seen_.erase(it); } else ++it;
    }
    for (auto it=members_.begin(); it!=members_.end();) {
        if (it->first != owner_ && now > it->second.seen && now-it->second.seen > 180) {it=members_.erase(it);++generation_;}
        else ++it;
    }
}
Json PartyLobby::snapshot(const std::string& caller) const {
    Json result=Json::object(), people=Json::array();
    result.set("owner",Json::of(destination_));
    result.set("capacity",Json::of(8));
    for (const auto& [code, member] : members_) {
        Json row=Json::object(); row.set("code",Json::of(code)); row.set("name",Json::of(member.name)); people.items.push_back(row);
    }
    result.set("members",people);
    result.set("count",Json::of(static_cast<unsigned>(people.items.size())));
    if (destination_ == owner_ && members_.count(caller)) result.set("room",Json::of(room_));
    if (caller == owner_) {
        Json invitations=Json::array();
        for (const auto& [sender, host] : inbox_) {
            Json row=Json::object(); row.set("from",Json::of(sender)); row.set("owner",Json::of(host)); invitations.items.push_back(row);
        }
        result.set("invitations",invitations);
    }
    return result;
}
Json PartyLobby::apply(const std::string& caller, const Json& request, bool friend_allowed, std::uint64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    expire(now);
    const std::string action=request.at("action").text;
    const bool self=caller==owner_;
    if(action=="restore") {
        if(!self || destination_==owner_)throw std::runtime_error("No recovery expected for this lobby");
        const auto recovery=verify_party_recovery(request.at("ticket"),destination_,room_,now);
        if(recovery.successor!=owner_)throw std::runtime_error("This client is not the lobby successor");
        std::map<std::string,Member> restored;
        for(const auto& member:recovery.members)restored.emplace(member.code,Member{member.name,now,member.endpoint});
        // Publish only after the entire signed checkpoint has been validated.
        members_=std::move(restored);destination_=owner_;generation_=recovery.generation+1;
        invited_.clear();share_token_.clear();share_created_=0;
        auto result=snapshot(caller);result.set("backup_name",Json::of(members_.at(caller).name));return result;
    }
    if (action=="share") {
        if (!self || destination_!=owner_ || !members_.count(owner_))
            throw std::runtime_error("Only the room host can create an invitation");
        share_token_=hex(random_bytes(32)); share_created_=now;
        Json result=Json::object(); result.set("token",Json::of(share_token_));
        result.set("expires",Json::of(now+900)); return result;
    }
    const auto* supplied_token=request.find("invite_token");
    const bool shared_join=action=="join" && destination_==owner_ && !share_token_.empty() &&
        now>=share_created_ && now-share_created_<900 && supplied_token && supplied_token->is_string() &&
        supplied_token->text==share_token_;
    // Authenticated pull: never exposes another recipient's invitations or the room secret.
    if (action=="invitations") {
        Json result=Json::object(), invitations=Json::array();
        const auto it=invited_.find(caller);
        if (it!=invited_.end() && destination_==owner_) {
            Json row=Json::object(); row.set("from",Json::of(it->second.sender));
            row.set("owner",Json::of(owner_)); invitations.items.push_back(row);
        }
        result.set("invitations",invitations); return result;
    }
    if (!self && !friend_allowed && !members_.count(caller) && !invited_.count(caller) && !shared_join)
        throw std::runtime_error("Invitation missing, invalid or expired: ask the host for a new one");
    if (action=="configure" || action=="join") {
        if (action=="configure" && !self) throw std::runtime_error("Only the owner can configure the lobby");
        if (destination_!=owner_) throw std::runtime_error("This friend is in another host's lobby");
        const std::string name=request.at("name").text;
        if (name.size()<3 || name.size()>16 || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
            throw std::runtime_error("Nome: 3-16 lettere, numeri, trattino o underscore");
        for (const auto& [code, member] : members_) if (code!=caller && member.name==name)
            throw std::runtime_error("Name already used in the lobby");
        if (!members_.count(caller) && members_.size()>=8) throw std::runtime_error("Lobby piena (8/8)");
        const auto* supplied_endpoint=request.find("endpoint");
        const auto endpoint=supplied_endpoint ? supplied_endpoint->text : members_.count(caller) ? members_.at(caller).endpoint : std::string{};
        if(!endpoint.empty() && !valid_party_endpoint(endpoint))throw std::runtime_error("Invalid host address");
        if(!members_.count(caller) || members_.at(caller).name!=name || members_.at(caller).endpoint!=endpoint)++generation_;
        members_[caller]={name,now,endpoint}; invited_.erase(caller);
    } else if (action=="heartbeat") {
        const auto it=members_.find(caller);
        if (it==members_.end()) throw std::runtime_error("You are no longer in the lobby");
        it->second.seen=now;
    } else if (action=="leave") {
        if (self) throw std::runtime_error("Use a new lobby to leave as the owner");
        if(members_.erase(caller))++generation_;
    } else if (action=="invite") {
        if (!members_.count(caller) || destination_!=owner_) throw std::runtime_error("Join a lobby before inviting");
        if (members_.size()>=8) throw std::runtime_error("Lobby piena (8/8)");
        FriendCodeInfo target;
        if (parse_friend_code(request.at("target").text,target)!=CodeParseError::Ok) throw std::runtime_error("Invalid invited code");
        if (invited_.size()>=64 && !invited_.count(target.code)) throw std::runtime_error("Too many pending invitations");
        invited_[target.code]={caller,now};
    } else if (action=="notify" || action=="receive_invite") {
        std::string sender=caller;
        if(action=="receive_invite") {
            if(!self) throw std::runtime_error("Operation reserved for the local launcher");
            FriendCodeInfo from;
            if(parse_friend_code(request.at("from").text,from)!=CodeParseError::Ok) throw std::runtime_error("Invalid sender");
            sender=from.code;
        }
        FriendCodeInfo host;
        if (parse_friend_code(request.at("owner").text,host)!=CodeParseError::Ok) throw std::runtime_error("Invalid invitation host");
        if (inbox_.size()>=64 && !inbox_.count(sender)) throw std::runtime_error("Too many invitations received");
        inbox_[sender]=host.code; inbox_seen_[sender]=now;
    } else if (action=="destination" || action=="new") {
        if (!self) throw std::runtime_error("Operation reserved for the local launcher");
        if (members_.size()>1) throw std::runtime_error("Your lobby has other players");
        const std::string destination=action=="new" ? owner_ : request.at("owner").text;
        FriendCodeInfo target;
        if (parse_friend_code(destination,target)!=CodeParseError::Ok) throw std::runtime_error("Invalid destination");
        const auto* supplied_room=request.find("room");
        const auto new_room=action=="destination" && supplied_room ? supplied_room->text : hex(random_bytes(32));
        if(!valid_room_selector(new_room))throw std::runtime_error("Invalid room code");
        destination_=target.code;room_=new_room;members_.clear();invited_.clear();
        generation_=0;
        share_token_.clear();share_created_=0;
        for(auto it=inbox_.begin();it!=inbox_.end();) {
            if(it->second==destination_) { inbox_seen_.erase(it->first); it=inbox_.erase(it); } else ++it;
        }
    } else if (action!="status") throw std::runtime_error("Unknown lobby action");
    auto result = snapshot(caller);
    // Resolve the backup account from authenticated membership, never a caller
    // supplied account number. Only that member's join/configure/heartbeat gets it.
    if (action == "heartbeat" || action == "configure" || action == "join")
        result.set("backup_name", Json::of(members_.at(caller).name));
    return result;
}
bool PartyLobby::admits(const std::string& room,const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    expire(party_now());
    return destination_==owner_ && room==room_ && std::any_of(members_.begin(),members_.end(),[&](const auto& row){return row.second.name==name;});
}
std::map<std::string,std::size_t> PartyLobby::expected_players() {
    std::lock_guard<std::mutex> lock(mutex_); expire(party_now());
    return destination_==owner_ ? std::map<std::string,std::size_t>{{room_,members_.size()}} : std::map<std::string,std::size_t>{};
}
} // namespace ctr_online
