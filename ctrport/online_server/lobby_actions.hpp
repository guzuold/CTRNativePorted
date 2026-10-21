// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "launcher_online.hpp"
#include "room_invite.hpp"

namespace ctr_online {
// Blocking room operations; the drawer executes these off the UI thread.
// 110: share, 111: join, 109: leave and return to local hosting.
struct LobbyActionResult {int action=0;bool ok=false;std::string detail;PartySelection party;};
inline LobbyActionResult lobby_action(const std::filesystem::path& root,const std::string& self,
    const std::string& name,const std::string& ip,const PartySelection& selection,
    const std::string& invite,int action) {
    auto command=[](const char* action_name) {auto j=Json::object();j.set("action",Json::of(action_name));return j;};
        LobbyActionResult result;result.action=action;result.party=selection;
        try {
            if(action==110) {
                format_room_invite({self,std::string(64,'0'),ip});
                auto response=party_request(root,selection.ip,selection.owner,command("share"));
                result.detail=format_room_invite({self,response.at("token").text,ip});
            } else if(action==111) {
                const auto ticket=parse_room_invite(invite);
                if(ticket.host==self || ticket.host==selection.owner)throw std::runtime_error("You are already in this room");
                const auto verified=verify_candidate_endpoint(ticket.ip+":8780",ticket.host);
                if(verified.status!=VerifyStatus::Verified)throw std::runtime_error(verified.detail);
                auto join=command("join");join.set("name",Json::of(name));join.set("invite_token",Json::of(ticket.token));
                join.set("endpoint",Json::of(ip));
                auto snapshot=party_request(root,verified.ip,ticket.host,join);
                try {
                    auto dest=command("destination");dest.set("owner",Json::of(ticket.host));
                    dest.set("room",snapshot.at("room"));
                    party_request(root,"127.0.0.1",self,dest);
                } catch(...) {
                    try {party_request(root,verified.ip,ticket.host,command("leave"));}catch(...){}
                    throw;
                }
                if(selection.owner!=self) {
                    try {party_request(root,selection.ip,selection.owner,command("leave"));}catch(...){}
                }
                result.party={verified.ip,ticket.host,snapshot.at("room").text,snapshot};
            } else if(action==109) {
                party_request(root,"127.0.0.1",self,command("new"));
                auto req=command("configure");req.set("name",Json::of(name));
                const auto snapshot=party_request(root,"127.0.0.1",self,req);
                result.party={"127.0.0.1",self,snapshot.at("room").text,snapshot};
                try {party_request(root,selection.ip,selection.owner,command("leave"));}catch(...){}
            } else throw std::runtime_error("Azione lobby sconosciuta");
            result.ok=true;
        } catch(const std::exception& e){result.detail=e.what();}
        return result;
}
} // namespace ctr_online
