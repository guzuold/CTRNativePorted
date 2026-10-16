// SPDX-License-Identifier: GPL-2.0-or-later
#include "party_lobby.hpp"
#include "crypto.hpp"
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
        throw std::runtime_error("Identita lobby non valida");
    const auto timestamp = packet.at("time").as_u64();
    if ((timestamp > now ? timestamp - now : now - timestamp) > 60)
        throw std::runtime_error("Messaggio lobby scaduto");
    const auto& body = packet.at("payload").text;
    if (!verify_friend_response(identity.public_key, "party-v1:" + hex(sha256(bytes_of(body))), {}, timestamp,
                                from_hex(packet.at("signature").text)))
        throw std::runtime_error("Firma lobby non valida");
    return parse_json(body);
}
PartyLobby::PartyLobby(std::string owner) : owner_(std::move(owner)), room_(hex(random_bytes(32))), destination_(owner_) {}
void PartyLobby::expire(std::uint64_t now) {
    for (auto it=invited_.begin(); it!=invited_.end();) {
        if (now>it->second.created && now-it->second.created>900) it=invited_.erase(it); else ++it;
    }
    for (auto it=inbox_seen_.begin(); it!=inbox_seen_.end();) {
        if (now>it->second && now-it->second>900) { inbox_.erase(it->first); it=inbox_seen_.erase(it); } else ++it;
    }
    for (auto it=members_.begin(); it!=members_.end();) {
        if (it->first != owner_ && now > it->second.seen && now-it->second.seen > 180) it=members_.erase(it);
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
    if (!self && !friend_allowed && !members_.count(caller) && !invited_.count(caller))
        throw std::runtime_error("Aggiungi reciprocamente il codice amico prima di collegarti");
    if (action=="configure" || action=="join") {
        if (action=="configure" && !self) throw std::runtime_error("Solo il proprietario puo configurare la lobby");
        if (destination_!=owner_) throw std::runtime_error("Questo amico e nella lobby di un altro host");
        const std::string name=request.at("name").text;
        if (name.size()<3 || name.size()>16 || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)
            throw std::runtime_error("Nome: 3-16 lettere, numeri, trattino o underscore");
        for (const auto& [code, member] : members_) if (code!=caller && member.name==name)
            throw std::runtime_error("Nome gia usato nella lobby");
        if (!members_.count(caller) && members_.size()>=8) throw std::runtime_error("Lobby piena (8/8)");
        members_[caller]={name,now}; invited_.erase(caller);
    } else if (action=="heartbeat") {
        const auto it=members_.find(caller);
        if (it==members_.end()) throw std::runtime_error("Non fai piu parte della lobby");
        it->second.seen=now;
    } else if (action=="leave") {
        if (self) throw std::runtime_error("Usa nuova lobby per uscire come proprietario");
        members_.erase(caller);
    } else if (action=="invite") {
        if (!members_.count(caller) || destination_!=owner_) throw std::runtime_error("Entra in una lobby prima di invitare");
        if (members_.size()>=8) throw std::runtime_error("Lobby piena (8/8)");
        FriendCodeInfo target;
        if (parse_friend_code(request.at("target").text,target)!=CodeParseError::Ok) throw std::runtime_error("Codice invitato non valido");
        if (invited_.size()>=64 && !invited_.count(target.code)) throw std::runtime_error("Troppi inviti pendenti");
        invited_[target.code]={caller,now};
    } else if (action=="notify" || action=="receive_invite") {
        std::string sender=caller;
        if(action=="receive_invite") {
            if(!self) throw std::runtime_error("Operazione riservata al launcher locale");
            FriendCodeInfo from;
            if(parse_friend_code(request.at("from").text,from)!=CodeParseError::Ok) throw std::runtime_error("Mittente non valido");
            sender=from.code;
        }
        FriendCodeInfo host;
        if (parse_friend_code(request.at("owner").text,host)!=CodeParseError::Ok) throw std::runtime_error("Host invito non valido");
        if (inbox_.size()>=64 && !inbox_.count(sender)) throw std::runtime_error("Troppi inviti ricevuti");
        inbox_[sender]=host.code; inbox_seen_[sender]=now;
    } else if (action=="destination" || action=="new") {
        if (!self) throw std::runtime_error("Operazione riservata al launcher locale");
        if (members_.size()>1) throw std::runtime_error("La tua lobby contiene altri giocatori");
        const std::string destination=action=="new" ? owner_ : request.at("owner").text;
        FriendCodeInfo target;
        if (parse_friend_code(destination,target)!=CodeParseError::Ok) throw std::runtime_error("Destinazione non valida");
        const auto new_room=hex(random_bytes(32));
        destination_=target.code;room_=new_room;members_.clear();invited_.clear();
        for(auto it=inbox_.begin();it!=inbox_.end();) {
            if(it->second==destination_) { inbox_seen_.erase(it->first); it=inbox_.erase(it); } else ++it;
        }
    } else if (action!="status") throw std::runtime_error("Azione lobby sconosciuta");
    return snapshot(caller);
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
