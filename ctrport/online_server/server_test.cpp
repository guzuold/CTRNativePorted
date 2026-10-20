// SPDX-License-Identifier: GPL-2.0-or-later
// Unit checks of the C++ project server's building blocks against published test vectors and
// against byte strings produced by the Python reference modules (tools/online). The
// network-level comparison of the two servers is tools/online/e2e_server_check.py.
#include "crypto.hpp"
#include "json.hpp"
#include "protocol.hpp"
#include "relay.hpp"
#include "server.hpp"
#include "stores.hpp"
#include "local_backup.hpp"
#include "launcher_online.hpp"
#include "upnp.hpp"
#include "friend_identity.hpp"
#include "dht_service.hpp"
#include "friend_verifier.hpp"
#include "friend_store.hpp"
#include "party_lobby.hpp"
#include "party_migration.hpp"
#include "room_invite.hpp"
#include "friend_network.hpp"
#include "../dht/dht.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <thread>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

using namespace ctr_online;
namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("{\"scope\": \"online_server_test\", \"check\": \"%s\", \"ok\": %s}\n", what, ok ? "true" : "false");
    std::fflush(stdout);
    if (!ok) ++failures;
}
Bytes b(const std::string& h) { return from_hex(h); }
Param param(int kind, Json value) { Param p; p.kind = kind; p.value = std::move(value); return p; }
} // namespace

void test_dht_discovery() {
    // Isolated 2-process DHT discovery test
    {
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);

        const std::uint16_t host_dht_port = 8793;
        const Bytes test_info_hash = sha1(bytes_of("test_2proc_dht_info_hash_vector_1"));

        const std::string hex_hash = hex(test_info_hash);
        const std::wstring w_hex(hex_hash.begin(), hex_hash.end());
        std::wstring cmd = std::wstring(L"\"") + exe_path + L"\" --dht-node-host " +
                           std::to_wstring(host_dht_port) + L" " + w_hex;

        STARTUPINFOW si{sizeof(si)};
        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
        check(ok != FALSE, "spawned DHT host child process");
        if (ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(400));

            dht_allow_loopback = 1;
            DhtService guest_dht;
            const bool guest_started = guest_dht.start(0, {}, {"127.0.0.1:1"});
            check(guest_started, "guest DHT node started and bootstrapped to host child process");
            // Reproduce a cold start: lookup starts before any bootstrap reply.
            std::thread delayed_bootstrap([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(700));
                guest_dht.ping_node("127.0.0.1", host_dht_port);
            });

            const std::vector<std::string> candidates = guest_dht.search(test_info_hash, 3500);
            delayed_bootstrap.join();
            guest_dht.stop();
            dht_allow_loopback = 0;

            bool found = false;
            for (const auto& ep : candidates) {
                if (ep == "127.0.0.1:8780") found = true;
            }
            check(found, "2-process isolated DHT test: guest discovered host candidate 127.0.0.1:8780 via BitTorrent DHT");

            TerminateProcess(pi.hProcess, 0);
            WaitForSingleObject(pi.hProcess, 1000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }

}

void test_invitation_pull() {
    const auto root=std::filesystem::temp_directory_path()/("ctr_invite_pull_"+hex(random_bytes(8)));
    const auto aroot=root/"host", broot=root/"guest", croot=root/"stranger";
    const auto a=local_friend_code(aroot), b=local_friend_code(broot), c=local_friend_code(croot);
    auto req=[](const char* action) { auto j=Json::object();j.set("action",Json::of(action));return j; };
    ServerConfig ac; ac.host="127.0.0.2"; ac.public_host=ac.host; ac.party_enabled=true;
    ac.state_dir=aroot/"runtime/user/online"; ac.log_path=aroot/"runtime/user/logs/pull.jsonl";
    Server server(ac);server.start();
    auto configure=req("configure");configure.set("name",Json::of("Host"));
    const auto original=party_request(aroot,ac.host,a,configure);
    auto invite=req("invite");invite.set("target",Json::of(b));
    party_request(aroot,ac.host,a,invite);
    // No recipient service exists: all network requests below originate from the guest.
    const auto fetched=party_request(broot,ac.host,a,req("invitations"));
    check(fetched.at("invitations").items.size()==1 && fetched.at("invitations").items[0].at("from").text==a,
          "invite pulled from existing host without any recipient listener");
    check(!fetched.find("room") && !fetched.find("members"),"invitation pull discloses no room secret or roster");
    check(party_request(croot,ac.host,a,req("invitations")).at("invitations").items.empty(),
          "different signed identity cannot retrieve another recipient invitation");
    FriendStore book(broot/"runtime/user/online/friends.json");book.add_or_update(a,"Host");
    book.update_endpoint(a,"127.0.0.2:8780","verified");
    ServerConfig bc=ac;bc.host="127.0.0.1";bc.public_host=bc.host;
    bc.state_dir=broot/"runtime/user/online";bc.log_path=broot/"runtime/user/logs/pull.jsonl";
    Server receiver(bc);receiver.start();
    check(poll_party_invitations(broot,a,nullptr,0),"launcher pulls via cached host without DHT or incoming remote connection");
    check(party_request(broot,bc.host,b,req("status")).at("invitations").items.size()==1,
          "verified outbound pull updates local launcher inbox");
    poll_party_invitations(broot,a,nullptr,0);
    check(party_request(broot,bc.host,b,req("status")).at("invitations").items.size()==1,"repeated polls do not duplicate invitation");
    auto join=req("join");join.set("name",Json::of("Guest"));
    const auto joined=party_request(broot,ac.host,a,join);
    check(joined.at("room").text==original.at("room").text,"accepting pulled invite preserves original game room selector");
    check(party_request(broot,ac.host,a,req("invitations")).at("invitations").items.empty(),"accepting consumes server invitation");
    auto destination=req("destination");destination.set("owner",Json::of(a));
    check(party_request(broot,bc.host,b,destination).at("invitations").items.empty(),"accepting clears local inbox");
    receiver.stop();server.stop();
    PartyLobby ttl(a);const auto now=party_now();ttl.apply(a,configure,false,now);ttl.apply(a,invite,false,now);
    check(ttl.apply(b,req("invitations"),false,now+901).at("invitations").items.empty(),"unclaimed invitations expire after fifteen minutes");
    bool rejected=false;try {ttl.apply(b,join,false,now+901);}catch(...) {rejected=true;}
    check(rejected,"expired invitation no longer authorizes joining");
    std::error_code ignored;std::filesystem::remove_all(root,ignored); // the servers' logs close at scope exit
}

void test_room_invite() {
    const auto root=std::filesystem::temp_directory_path()/("ctr_room_invite_"+hex(random_bytes(8)));
    const auto host=root/"host", guest=root/"guest";
    const auto a=local_friend_code(host), b=local_friend_code(guest);
    auto req=[](const char* action) { auto j=Json::object();j.set("action",Json::of(action));return j; };
    auto configure=req("configure");configure.set("name",Json::of("Host"));
    auto join=req("join");join.set("name",Json::of("Guest"));
    auto rejects=[&](auto operation,const char* label) {bool denied=false;try{operation();}catch(const std::exception&){denied=true;}check(denied,label);};
    PartyLobby lobby(a);
    const auto original=lobby.apply(a,configure,false,1000);
    rejects([&]{lobby.apply(b,join,false,1000);},"room: no invitation and no friendship denied");
    const auto shared=lobby.apply(a,req("share"),false,1000);
    const auto text=format_room_invite({a,shared.at("token").text,"127.0.0.2"});
    const auto parsed=parse_room_invite(" \n"+text+"\r\n");
    check(parsed.host==a && parsed.token==shared.at("token").text && parsed.ip=="127.0.0.2","room: invitation roundtrip with surrounding whitespace");
    for(const auto& bad:std::vector<std::string>{"",text+"|extra","CTRROOM2"+text.substr(8),text.substr(0,text.size()-9)+"999.0.0.1"})
        rejects([&]{parse_room_invite(bad);},"room: malformed invitation rejected");
    join.set("invite_token",Json::of(std::string(64,'0')));
    rejects([&]{lobby.apply(b,join,false,1001);},"room: wrong token denied");
    join.set("invite_token",Json::of(parsed.token));
    PartyLobby expiry(a);expiry.apply(a,configure,false,1000);
    auto expired_join=join;
    expired_join.set("invite_token",expiry.apply(a,req("share"),false,1000).at("token"));
    rejects([&]{expiry.apply(b,expired_join,false,1900);},"room: token expires at fifteen minutes");
    const auto joined=lobby.apply(b,join,false,1001);
    check(joined.at("room").text==original.at("room").text && joined.at("count").as_u64()==2,
          "room: invited non-friend receives the same retail matchmaking selector");
    check(!joined.find("token") && !joined.find("invite_token"),"room: roster never discloses sharing token");
    rejects([&]{lobby.apply(b,req("share"),false,1002);},"room: guest cannot mint an invitation");
    // Test rotation and room lifecycle with monotonic synthetic timestamps.
    PartyLobby rotation(a);rotation.apply(a,configure,false,2000);
    const auto old=rotation.apply(a,req("share"),false,2000).at("token").text;
    const auto fresh=rotation.apply(a,req("share"),false,2001).at("token").text;
    join.set("invite_token",Json::of(old));
    rejects([&]{rotation.apply(b,join,false,2002);},"room: copying a fresh invitation revokes previous token");
    rotation.apply(a,req("new"),false,2002);rotation.apply(a,configure,false,2002);
    join.set("invite_token",Json::of(fresh));
    rejects([&]{rotation.apply(b,join,false,2003);},"room: new room revokes old invitation");
    // Roster lifecycle, with monotonic synthetic time and no game emulation.
    lobby.apply(b,req("leave"),false,1003);
    check(lobby.apply(a,req("heartbeat"),false,1004).at("count").as_u64()==1,"room: explicit leave updates roster");
    join.set("invite_token",Json::of(parsed.token));
    lobby.apply(b,join,false,1005);
    check(lobby.apply(a,req("heartbeat"),false,1186).at("count").as_u64()==1,"room: silent guest expires after 180 seconds");
    join.set("invite_token",rotation.apply(a,req("share"),false,2004).at("token"));
    for(unsigned i=0;i<7;++i) {
        auto member=join;member.set("name",Json::of("Player"+std::to_string(i)));
        rotation.apply(local_friend_code(root/("member"+std::to_string(i))),member,false,2005);
    }
    check(rotation.apply(a,req("status"),false,2006).at("count").as_u64()==8,"room: host plus seven guests admitted");
    rejects([&]{rotation.apply(b,join,false,2006);},"room: ninth player rejected despite valid invitation");
    // Actual signed HTTP on loopback; no friend entries or guest listener.
    {
    ServerConfig config;config.host="127.0.0.2";config.public_host=config.host;config.party_enabled=true;
    config.state_dir=host/"runtime/user/online";config.log_path=host/"runtime/user/logs/room.jsonl";
    Server server(config);server.start();
    const auto before=party_request(host,config.host,a,configure);
    const auto ticket=party_request(host,config.host,a,req("share"));
    const auto verified=verify_candidate_endpoint(config.host+":8780",a);
    check(verified.status==VerifyStatus::Verified,"room: invitation host identity verified over HTTP");
    join.set("invite_token",ticket.at("token"));
    const auto after=party_request(guest,config.host,a,join);
    check(after.at("room").text==before.at("room").text && after.at("count").as_u64()==2,
          "room: signed HTTP join without reciprocal address books");
    join=req("join");join.set("name",Json::of("Guest"));
    check(party_request(guest,config.host,a,join).at("room").text==before.at("room").text,
          "room: admitted guest revalidates membership at game launch without token");
    server.stop();
    } // Close server log and store handles before removing the fixture on Windows.
    std::filesystem::remove_all(backup_io_path(root));
}

void test_party_migration_http() {
    struct Environment {
        std::string before;bool existed=false;
        Environment(){char value[256]{};const auto n=GetEnvironmentVariableA("CTR_EXPERIMENTAL_HOST_MIGRATION",value,256);existed=n>0 && n<256;if(existed)before=value;SetEnvironmentVariableA("CTR_EXPERIMENTAL_HOST_MIGRATION","1");}
        ~Environment(){SetEnvironmentVariableA("CTR_EXPERIMENTAL_HOST_MIGRATION",existed?before.c_str():nullptr);}
    } environment;
    const auto root=std::filesystem::temp_directory_path()/("ctr_party_recovery_"+hex(random_bytes(8)));
    const auto aroot=root/"previous",broot=root/"successor",croot=root/"survivor";
    const auto a=local_friend_code(aroot),b=local_friend_code(broot),c=local_friend_code(croot);
    auto config=[](const std::filesystem::path& folder,const char* ip) {
        ServerConfig x;x.host=ip;x.public_host=ip;x.party_enabled=true;
        x.state_dir=folder/"runtime/user/online";x.log_path=folder/"runtime/user/logs/recovery.jsonl";return x;
    };
    Server previous(config(aroot,"127.0.0.2")),successor(config(broot,"127.0.0.1")),survivor(config(croot,"127.0.0.3"));
    previous.start();successor.start();survivor.start();
    auto command=[](const char* action){auto j=Json::object();j.set("action",Json::of(action));return j;};
    auto configure=command("configure");configure.set("name",Json::of("Host"));
    party_request(aroot,"127.0.0.2",a,configure);
    auto invite=command("invite");invite.set("target",Json::of(b));party_request(aroot,"127.0.0.2",a,invite);
    invite.set("target",Json::of(c));party_request(aroot,"127.0.0.2",a,invite);
    auto join=command("join");join.set("name",Json::of("Migration2"));join.set("endpoint",Json::of("127.0.0.1"));
    party_request(broot,"127.0.0.2",a,join);
    join.set("name",Json::of("Migration1"));join.set("endpoint",Json::of("127.0.0.3"));
    const auto csnapshot=party_request(croot,"127.0.0.2",a,join);
    const auto bsnapshot=party_request(broot,"127.0.0.2",a,command("heartbeat"));
    const auto room=bsnapshot.at("room").text;
    auto destination=command("destination");destination.set("owner",Json::of(a));destination.set("room",Json::of(room));
    party_request(broot,"127.0.0.1",b,destination);party_request(croot,"127.0.0.3",c,destination);
    const PartySelection bselection{"127.0.0.2",a,room,bsnapshot},cselection{"127.0.0.2",a,room,csnapshot};
    check(verify_party_recovery(bsnapshot.at("migration_ticket"),a,room,party_now()).successor==b,
          "HTTP recovery checkpoint contains registered successor endpoint");
    previous.stop();
    bool pending=false;try{recover_party(croot,cselection,"127.0.0.3");}catch(...){pending=true;}
    check(pending && party_request(croot,"127.0.0.3",c,command("status")).at("owner").text==a,
          "survivor waits for successor without changing destination prematurely");
    const auto hosted=recover_party(broot,bselection);
    const auto followed=recover_party(croot,cselection,"127.0.0.3");
    check(hosted.owner==b && followed.owner==b && hosted.room==room && followed.room==room &&
          followed.snapshot.at("count").as_u64()==2,"HTTP restore reunites both survivors in unchanged room after old server stops");
    check(party_request(croot,"127.0.0.3",c,command("status")).at("owner").text==b,
          "survivor local backend follows new room owner");
    check(recover_party(broot,bselection).owner==b,"restore retry recovers a lost response without replacing room again");
    check(verify_party_message(followed.snapshot.at("migration_ticket"),b,party_now()).at("room").text==room,
          "successor refreshes signed checkpoint for next failure");
    check(party_request(broot,"127.0.0.1",b,command("share")).at("token").text.size()==64,
          "successor can issue fresh invitations after recovery");
    bool departed=false;try{party_request(aroot,"127.0.0.1",b,command("heartbeat"));}catch(...){departed=true;}
    check(departed,"old host is not silently readmitted by recovered membership");
    survivor.stop();successor.stop();
    std::printf("{\"scope\":\"party_migration_http\",\"failures\":%d}\n",failures);
}

namespace ctr_online {
int http_request(const std::string& ipv4, std::uint16_t port, const std::string& path, const std::string& payload,
                 std::string& body, int timeout_ms); // launcher_online.cpp
}
// A request as the game sends it to its local service (identity header, port 8780).
int game_http(const std::string& ip, const std::string& method, const std::string& path, const std::string& online_id,
              const std::string& body, std::string& answer, const std::string& content_type = "application/json") {
    answer.clear();
    const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8780);
    inet_pton(AF_INET, ip.c_str(), &address.sin_addr);
    if (connect(s, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) { closesocket(s); return 0; }
    const std::string request = method + " " + path + " HTTP/1.1\r\nHost: " + ip + "\r\nConnection: close\r\nX-Ctr-Online-Id: " +
        online_id + "\r\nContent-Type: " + content_type + "\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(s, request.data(), static_cast<int>(request.size()), 0);
    std::string raw;
    char buffer[8192];
    for (int n; (n = recv(s, buffer, sizeof(buffer), 0)) > 0;) raw.append(buffer, static_cast<std::size_t>(n));
    closesocket(s);
    const auto end = raw.find("\r\n\r\n");
    if (end == std::string::npos || raw.size() < 12) return 0;
    answer = raw.substr(end + 4);
    return std::atoi(raw.c_str() + 9);
}
// Phase 2: three local services on separate addresses. A and B have each other in their
// rubrica; C has A in its own but A does not know C.
void test_peer_channel() {
    const auto root=std::filesystem::temp_directory_path()/("ctr_peer_"+hex(random_bytes(8)));
    const auto aroot=root/"a",broot=root/"b",croot=root/"c";
    const auto a=local_friend_code(aroot),b=local_friend_code(broot),c=local_friend_code(croot);
    FriendStore(aroot/"runtime/user/online/friends.json").add_or_update(b,"Bruno");
    FriendStore(broot/"runtime/user/online/friends.json").add_or_update(a,"Anna");
    FriendStore(croot/"runtime/user/online/friends.json").add_or_update(a,"Anna");
    const std::map<std::string,std::string> address{{a,"127.0.0.2"},{b,"127.0.0.3"},{c,"127.0.0.4"}};
    auto config=[&](const std::filesystem::path& folder,const std::string& ip,const char* name) {
        ServerConfig x;x.host=ip;x.public_host=ip;x.relay_port=0;x.tcp_record_ports={};x.udp_record_ports={};
        x.state_dir=folder/"runtime/user/online";x.log_path=folder/"runtime/user/logs/peer.jsonl";
        x.friend_network=true;x.local_name=name;x.friend_poll_period=std::chrono::hours(1);
        x.friend_resolver=[address](const std::string& code)->std::optional<std::pair<std::string,std::uint16_t>> {
            const auto it=address.find(code);
            if(it==address.end()) return std::nullopt;
            return std::make_pair(it->second,std::uint16_t(8780));
        };
        return x;
    };
    Server sa(config(aroot,"127.0.0.2","PeerAnna")),sb(config(broot,"127.0.0.3","PeerBruno")),sc(config(croot,"127.0.0.4","PeerCarla"));
    sa.start();sb.start();sc.start();
    sa.poll_friends();sb.poll_friends();sc.poll_friends();
    const auto af=sa.friends(),bf=sb.friends(),cf=sc.friends();
    check(af.size()==1 && af[0].code==b && af[0].online && af[0].name=="PeerBruno" && af[0].nickname=="Bruno" &&
          af[0].account==user_id("PeerBruno") && !af[0].in_game,"peer presence: friend's service reports its player");
    check(bf.size()==1 && bf[0].online && bf[0].name=="PeerAnna","peer presence: both directions");
    check(cf.size()==1 && !cf[0].online && cf[0].name.empty(),"peer presence: service refuses a caller outside its rubrica");
    const auto cid=HostIdentity::load_or_create(croot/"runtime/user/identity/host_identity.json");
    const auto bid=HostIdentity::load_or_create(broot/"runtime/user/identity/host_identity.json");
    auto presence=[]{Json j=Json::object();j.set("action",Json::of("presence"));return j;};
    std::string reason;
    try{peer_request(cid,"127.0.0.2",8780,a,presence());}catch(const std::exception& e){reason=e.what();}
    check(reason=="Non e nella lista amici","peer channel: stranger gets the rubrica refusal");
    Json payload=presence();payload.set("nonce",Json::of(hex(random_bytes(16))));payload.set("to",Json::of(a));
    const std::string packet=dump_json(sign_peer_message(bid,payload));
    std::string first,second;
    const int s1=http_request("127.0.0.2",8780,"/ctr/v1/peer",packet,first,3000);
    const int s2=http_request("127.0.0.2",8780,"/ctr/v1/peer",packet,second,3000);
    check(s1==200 && s2==403 && second.find("duplicato")!=std::string::npos,"peer channel: replayed packet refused");
    Json elsewhere=presence();elsewhere.set("nonce",Json::of(hex(random_bytes(16))));elsewhere.set("to",Json::of(c));
    std::string misdirected;
    check(http_request("127.0.0.2",8780,"/ctr/v1/peer",dump_json(sign_peer_message(bid,elsewhere)),misdirected,3000)==403 &&
          misdirected.find("altro servizio")!=std::string::npos,"peer channel: packet addressed to another service refused");
    Json forged=sign_peer_message(bid,payload);forged.set("code",Json::of(c));
    std::string third;
    check(http_request("127.0.0.3",8780,"/ctr/v1/peer",dump_json(forged),third,3000)==403,
          "peer channel: claimed sender must match the signature");
    Json unknown=Json::object();unknown.set("action",Json::of("format_disk"));
    reason.clear();
    try{peer_request(bid,"127.0.0.2",8780,a,unknown);}catch(const std::exception& e){reason=e.what();}
    check(reason.find("Azione non supportata")==0,"peer channel: unknown action refused");
    std::string learned;
    const auto identified=identify_endpoint("127.0.0.3:8780",learned,2000);
    check(identified.status==VerifyStatus::Verified && learned==b && identified.ip=="127.0.0.3",
          "public matchmaking: a service's code is learned and verified from its identity answer");
    learned.clear();
    check(identify_endpoint("127.0.0.9:8780",learned,500).status!=VerifyStatus::Verified && learned.empty(),
          "public matchmaking: an endpoint without a service is not a candidate");
    const MatchHub self{"CTR1-M",""},low{"CTR1-A","10.0.0.1"},high{"CTR1-Z","10.0.0.2"};
    check(choose_hub(self,true,{high,low})->code=="CTR1-A" && choose_hub(self,true,{high})->code=="CTR1-M" &&
          choose_hub(self,false,{high})->code=="CTR1-Z" && !choose_hub(self,false,{}) && choose_hub(self,true,{})->code=="CTR1-M",
          "public matchmaking: hub is the lowest code among reachable services, never an unreachable self");
    const auto anna=std::to_string(user_id("PeerAnna")),bruno=std::to_string(user_id("PeerBruno"));
    std::string answer;
    check(game_http("127.0.0.3","PUT","/webapi/sdk:userProfile/v1/users/"+bruno+"/presence/gameStatus","PeerBruno",
                    R"({"gameStatus":"In gara"})",answer)==204,"peer presence: game status stored on the friend's own service");
    sa.poll_friends();
    const auto reported=sa.friends();
    check(reported.size()==1 && reported[0].report.at("game_presence").at("gameStatus").text=="In gara",
          "peer presence: friend's game status reaches this service");
    const auto list_route="/webapi/sdk:userProfile/v1/users/"+anna+"/friendList?offset=0&limit=100";
    check(game_http("127.0.0.2","GET",list_route,"PeerAnna","",answer)==200 &&
          answer.find("\"onlineId\": \"PeerBruno\"")!=std::string::npos && answer.find("\"accountId\": \""+bruno+"\"")!=std::string::npos &&
          answer.find("\"onlineStatus\": \"offline\"")!=std::string::npos && answer.find("PeerCarla")==std::string::npos,
          "in-game friendList is the rubrica; friend without a running game is offline");
    FriendStore(broot/"runtime/user/online/friends.json").remove(a);
    sa.poll_friends();
    const auto after=sa.friends();
    check(after.size()==1 && !after[0].online && after[0].name=="PeerBruno",
          "peer presence: removal takes effect at once, offline friend keeps its last name");
    sa.stop();
    {
        Server restarted(config(aroot,"127.0.0.2","PeerAnna"));restarted.start();
        check(game_http("127.0.0.2","GET",list_route,"PeerAnna","",answer)==200 &&
              answer.find("\"onlineId\": \"PeerBruno\"")!=std::string::npos,
              "in-game friendList keeps an offline friend after a restart");
        restarted.stop();
    }
    FriendStore(aroot/"runtime/user/online/friends.json").remove(b);
    {
        Server restarted(config(aroot,"127.0.0.2","PeerAnna"));restarted.start();
        check(game_http("127.0.0.2","GET",list_route,"PeerAnna","",answer)==200 &&
              answer.find("PeerBruno")==std::string::npos,"friend removed from the rubrica leaves the in-game list");
        restarted.stop();
    }
    sb.stop();sc.stop();
    std::error_code ignored;std::filesystem::remove_all(root,ignored);
    std::printf("{\"scope\":\"peer_channel\",\"failures\":%d}\n",failures);
}

// Phase 2: a player invites a friend whose game is connected to another PC's service. The
// session stays on the host's service; the guest's service forwards accept, details and join.
void test_friend_invitation() {
    const auto root=std::filesystem::temp_directory_path()/("ctr_invite_"+hex(random_bytes(8)));
    const auto aroot=root/"a",broot=root/"b",croot=root/"c";
    const auto a=local_friend_code(aroot),b=local_friend_code(broot),c=local_friend_code(croot);
    FriendStore(aroot/"runtime/user/online/friends.json").add_or_update(b,"Bruno");
    FriendStore(broot/"runtime/user/online/friends.json").add_or_update(a,"Anna");
    FriendStore(croot/"runtime/user/online/friends.json").add_or_update(a,"Anna");
    const std::map<std::string,std::string> address{{a,"127.0.0.2"},{b,"127.0.0.3"},{c,"127.0.0.4"}};
    auto config=[&](const std::filesystem::path& folder,const std::string& ip,const char* name) {
        ServerConfig x;x.host=ip;x.public_host=ip;x.relay_port=0;x.tcp_record_ports={};x.udp_record_ports={};
        x.state_dir=folder/"runtime/user/online";x.log_path=folder/"runtime/user/logs/invite.jsonl";
        x.friend_network=true;x.local_name=name;x.friend_poll_period=std::chrono::hours(1);
        x.friend_resolver=[address](const std::string& code)->std::optional<std::pair<std::string,std::uint16_t>> {
            const auto it=address.find(code);
            if(it==address.end()) return std::nullopt;
            return std::make_pair(it->second,std::uint16_t(8780));
        };
        return x;
    };
    Server sa(config(aroot,"127.0.0.2","HostAnna")),sb(config(broot,"127.0.0.3","GuestBruno")),sc(config(croot,"127.0.0.4","OtherCarla"));
    sa.start();sb.start();sc.start();
    sa.poll_friends();sb.poll_friends();
    const std::string meta=R"({"sessionPrivacy":"private","sessionMaxUser":8,"sessionName":"x","availablePlatforms":["PS4"]})";
    const std::string body="--B\r\nContent-Type: application/json\r\nContent-Description: session-request\r\nContent-Length: "+
        std::to_string(meta.size())+"\r\n\r\n"+meta+"\r\n--B\r\nContent-Type: application/octet-stream\r\n"
        "Content-Description: changeable-session-data\r\nContent-Length: 2\r\n\r\nab";
    std::string answer;
    check(game_http("127.0.0.2","POST","/webapi/sdk:sessionInvitation/v1/sessions","HostAnna",body,answer,"multipart/mixed; boundary=B")==201,
          "cross invite: host creates a private session on its own service");
    const auto session=parse_json(answer).at("sessionId").text;
    const auto guest=user_id("GuestBruno");
    check(game_http("127.0.0.2","POST","/ctr/v1/session-invitations","HostAnna",
                    R"({"sessionId":")"+session+R"(","recipients":[)"+std::to_string(guest)+"]}",answer)==201,
          "cross invite: host sends the invitation");
    const auto invitation=parse_json(answer).items.at(0).at("invitationId").text;
    check(game_http("127.0.0.3","GET","/ctr/v1/session-invitations","GuestBruno","",answer)==200 &&
          answer.find(invitation)!=std::string::npos && answer.find(session)!=std::string::npos &&
          answer.find("\"onlineId\":\"HostAnna\"")!=std::string::npos,
          "cross invite: guest's own service delivers it to the guest's game");
    check(game_http("127.0.0.4","GET","/ctr/v1/session-invitations","OtherCarla","",answer)==200 && answer=="[]",
          "cross invite: another PC's player does not receive it");
    check(game_http("127.0.0.3","GET","/webapi/sdk:sessionInvitation/v1/users/me/invitations","GuestBruno","",answer)==200 &&
          answer.find("\"size\":1")!=std::string::npos && answer.find("\"sessionCreator\"")!=std::string::npos,
          "cross invite: PSN invitation list carries the session details from the host's service");
    check(game_http("127.0.0.3","GET","/webapi/sdk:sessionInvitation/v1/sessions/"+session+"?fields=@default,members","GuestBruno","",answer)==200 &&
          answer.find("HostAnna")!=std::string::npos,"cross invite: invited guest reads the host's private session");
    check(game_http("127.0.0.3","PUT","/ctr/v1/session-invitations/"+invitation,"GuestBruno","",answer)==200 &&
          answer.find(session)!=std::string::npos,"cross invite: acceptance reaches the host's service");
    check(game_http("127.0.0.3","GET","/ctr/v1/session-invitations","GuestBruno","",answer)==200 && answer=="[]",
          "cross invite: an accepted invitation is no longer pending");
    check(game_http("127.0.0.3","POST","/webapi/sdk:sessionInvitation/v1/sessions/"+session+"/members?index=0&priority=49","GuestBruno","",answer)==204,
          "cross invite: guest joins the host's session from its own service");
    check(game_http("127.0.0.2","GET","/webapi/sdk:sessionInvitation/v1/sessions/"+session,"HostAnna","",answer)==200 &&
          answer.find("GuestBruno")!=std::string::npos,"cross invite: the host's service lists the guest as a member");
    sb.poll_friends();
    check(game_http("127.0.0.3","GET","/webapi/sdk:sessionInvitation/v1/users/me/friends/sessions?offset=0&limit=100","GuestBruno","",answer)==200 &&
          answer.find(session)!=std::string::npos,"cross invite: friend sessions on the guest's service include the host's session");
    check(game_http("127.0.0.3","GET","/webapi/sdk:sessionInvitation/v1/users/@users/sessions?accountIds="+std::to_string(user_id("HostAnna")),
                    "GuestBruno","",answer)==200 && answer.find(session)!=std::string::npos,
          "cross invite: the host's sessions by account come from the host's report");
    const auto bid=HostIdentity::load_or_create(broot/"runtime/user/identity/host_identity.json");
    const auto cid=HostIdentity::load_or_create(croot/"runtime/user/identity/host_identity.json");
    auto webapi=[&](const std::string& name,const std::string& method,const std::string& target) {
        Json j=Json::object();j.set("action",Json::of("webapi"));j.set("method",Json::of(method));j.set("target",Json::of(target));
        j.set("name",Json::of(name));j.set("content_type",Json::of(""));j.set("body",Json::of(""));return j;
    };
    std::string reason;
    try{peer_request(bid,"127.0.0.2",8780,a,webapi("HostAnna","DELETE","/webapi/sdk:sessionInvitation/v1/sessions/"+session));}
    catch(const std::exception& e){reason=e.what();}
    check(!reason.empty(),"cross invite: a friend's service cannot act as the host's own player");
    reason.clear();
    try{peer_request(bid,"127.0.0.2",8780,a,webapi("GuestBruno","GET","/ctr/v1/config"));}catch(const std::exception& e){reason=e.what();}
    check(reason=="Richiesta non inoltrabile","cross invite: only session and invitation routes are forwarded");
    reason.clear();
    try{peer_request(cid,"127.0.0.2",8780,a,webapi("OtherCarla","GET","/webapi/sdk:sessionInvitation/v1/sessions/"+session));}
    catch(const std::exception& e){reason=e.what();}
    check(reason=="Non e nella lista amici","cross invite: a stranger's service cannot read the session");
    check(game_http("127.0.0.3","DELETE","/webapi/sdk:sessionInvitation/v1/sessions/"+session+"/members/me","GuestBruno","",answer)==204 &&
          game_http("127.0.0.2","GET","/webapi/sdk:sessionInvitation/v1/sessions/"+session,"HostAnna","",answer)==200 &&
          answer.find("GuestBruno")==std::string::npos,"cross invite: guest leaves the host's session through its own service");
    sa.stop();sb.stop();sc.stop();
    std::error_code ignored;std::filesystem::remove_all(root,ignored);
    std::printf("{\"scope\":\"friend_invitation\",\"failures\":%d}\n",failures);
}

int run_tests(int argc, char* argv[]) {
    if(argc>=2 && std::string(argv[1])=="--peer-only") {test_peer_channel();test_friend_invitation();return failures ? 1 : 0;}
    if(argc>=2 && std::string(argv[1])=="--party-migration-only") {test_party_migration_http();return failures ? 1 : 0;}
    if(argc>=2 && std::string(argv[1])=="--room-invite-only") {test_room_invite();return failures ? 1 : 0;}
    if(argc>=2 && std::string(argv[1])=="--invite-pull-only") {test_invitation_pull();return failures ? 1 : 0;}
    if (argc >= 2 && std::string(argv[1]) == "--dht-network-check") {
        DhtService dht;
        if (!dht.start(0, {})) return 2;
        for (int i = 0; i < 4; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            const auto s = dht.stats();
            std::printf("DHT elapsed=%d good=%d dubious=%d incoming=%d port=%u\n",
                        (i + 1) * 5, s.good_nodes, s.dubious_nodes, s.incoming_nodes,
                        static_cast<unsigned>(s.bound_port));
            std::fflush(stdout);
        }
        const auto s = dht.stats();
        return s.good_nodes + s.dubious_nodes > 0 ? 0 : 1;
    }
    if (argc >= 2 && std::string(argv[1]) == "--dht-only") {
        test_dht_discovery();
        const auto root = std::filesystem::temp_directory_path() / ("ctr_dht_diagnostic_" + hex(random_bytes(8)));
        const auto code = local_friend_code(root);
        const auto absent = find_friend_server(root, code, 100, 100, {"127.0.0.1:1"});
        check(absent.status == VerifyStatus::NoResponse && absent.detail.find("Nessun nodo DHT raggiunto") != std::string::npos,
              "unreachable bootstrap is distinguished from missing friend announcement");
        DhtService stopped;
        const auto unavailable = find_friend_server(root, code, 100, 100, {}, &stopped);
        check(unavailable.status == VerifyStatus::NoResponse && unavailable.detail.find("DHT locale non avviata") != std::string::npos,
              "stopped local DHT is reported separately");
        std::filesystem::remove_all(root);
        return failures ? 1 : 0;
    }
    if (argc >= 4 && std::string(argv[1]) == "--dht-node-host") {
        const std::uint16_t port = static_cast<std::uint16_t>(std::stoi(argv[2]));
        const Bytes info_hash = from_hex(argv[3]);
        dht_allow_loopback = 1;
        DhtService dht;
        if (!dht.start(port, {}, {"127.0.0.1:1"})) return 2;
        dht.set_announce(info_hash, 8780);
        std::printf("DHT_HOST_RUNNING\n");
        std::fflush(stdout);
        for (int i = 0; i < 150; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        dht.stop();
        return 0;
    }
    // JSON: order, 64-bit integers, Python compact and default separators, ensure_ascii.
    const Json parsed = parse_json("{\"b\":18446744073709551615,\"a\":[-5,1.5,true,null],\"t\":\"\xC3\xA8\xF0\x9F\x98\x80\\\"\\n\"}");
    check(dump_json(parsed) == "{\"b\":18446744073709551615,\"a\":[-5,1.5,true,null],\"t\":\"\\u00e8\\ud83d\\ude00\\\"\\n\"}",
          "JSON round trip keeps order, u64 and ensure_ascii escapes");
    check(dump_json(parse_json("\"\\u00e8\\ud83d\\ude00\"")) == "\"\\u00e8\\ud83d\\ude00\"", "JSON escapes parsed and written back");
    check(dump_json_spaced(parse_json(R"({"x":[1,2],"y":{}})")) == R"({"x": [1, 2], "y": {}})", "JSON default separators");
    check(dump_json_sorted(parse_json(R"({"z":1,"a":{"d":1,"c":2}})")) == R"({"a":{"c":2,"d":1},"z":1})", "JSON sorted keys");
    check(dump_json(parse_json("[100.0,1e16,0.0001,1760480000.123]")) == "[100.0,1e+16,0.0001,1760480000.123]", "JSON float repr");
    check(parse_json(R"({"a":1,"b":2})") == parse_json(R"({"b":2,"a":1})"), "JSON object equality ignores order");

    // Hashes and HMAC (FIPS 180 "abc", RFC 2202 case 2), AES-128-CBC (SP 800-38A F.2.1).
    check(hex(sha1(bytes_of("abc"))) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 abc");
    check(hex(sha256(bytes_of("abc"))) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 abc");
    check(hex(hmac_sha1(bytes_of("Jefe"), bytes_of("what do ya want for nothing?"))) == "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79",
          "HMAC-SHA1 RFC 2202");
    const Bytes key = b("2b7e151628aed2a6abf7158809cf4f3c"), iv = b("000102030405060708090a0b0c0d0e0f");
    const Bytes plain = b("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51");
    const Bytes cipher = aes128_cbc_encrypt(key, iv, plain);
    check(hex(cipher) == "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2", "AES-128-CBC SP 800-38A");
    check(aes128_cbc_decrypt(key, iv, cipher) == plain, "AES-128-CBC decrypt");
    check(base64_encode(bytes_of("ab")) == "YWI=" && base64_decode("YWI=") == bytes_of("ab"), "base64");
    bool rejected = false;
    try { base64_decode("YW I="); } catch (const std::exception&) { rejected = true; }
    check(rejected, "strict base64 rejects non-alphabet characters");

    // RSA key: SPKI size, persistence, PSS signature verified by CNG with salt 0.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ctr_online_server_test";
    std::filesystem::remove_all(backup_io_path(dir));
    {
        RsaKey rsa(dir / "key.cng");
        check(rsa.spki().size() == 0x126, "SPKI DER is 0x126 bytes");
        RsaKey again(dir / "key.cng");
        check(again.spki() == rsa.spki(), "server key persists across starts");
        const Bytes message = bytes_of(R"({"auth_task":"47"})");
        const Bytes signature = rsa.sign_pss_sha256(message);
        BCRYPT_KEY_HANDLE pub = nullptr;
        CERT_PUBLIC_KEY_INFO* info = nullptr;
        DWORD info_size = 0;
        const bool decoded = CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, rsa.spki().data(), static_cast<DWORD>(rsa.spki().size()),
                                                 CRYPT_DECODE_ALLOC_FLAG, nullptr, &info, &info_size);
        bool verified = false, tampered = true;
        if (decoded && CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, info, 0, nullptr, &pub)) {
            const Bytes digest = sha256(message);
            BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 0};
            verified = BCryptVerifySignature(pub, &padding, const_cast<PUCHAR>(digest.data()), 32, const_cast<PUCHAR>(signature.data()),
                                             static_cast<ULONG>(signature.size()), BCRYPT_PAD_PSS) == 0;
            const Bytes other = sha256(bytes_of(R"({"auth_task":"48"})"));
            tampered = BCryptVerifySignature(pub, &padding, const_cast<PUCHAR>(other.data()), 32, const_cast<PUCHAR>(signature.data()),
                                             static_cast<ULONG>(signature.size()), BCRYPT_PAD_PSS) == 0;
            BCryptDestroyKey(pub);
        }
        if (info) LocalFree(info);
        check(verified && !tampered, "PSS-SHA256 salt 0 signature verifies from the SPKI, altered message fails");
    }

    // Room selector and user ID: values printed by tools/online/room.py and demonware_auth3.py.
    check(room_token("same") == "97f972ee94f7ad1f72c34b02a6a566364b43884c57aae15d8459775e7497de91", "room selector as room.py");
    check(user_id("CTRTester1") == 0xa4d0dbda95b833e6ull, "user ID as demonware_auth3.user_id");

    // auth3 tickets: layout of demonware_auth3.client_ticket.
    const Bytes session_key(24, 0x11);
    const Bytes ticket = client_ticket(5760, "CTRTester1", session_key, 1000);
    check(ticket.size() == 0x80 && hex(Bytes(ticket.begin(), ticket.begin() + 4)) == "deadbdef" && ticket[4] == 0 &&
          Bytes(ticket.begin() + 0x61, ticket.begin() + 0x79) == session_key, "client ticket layout");
    const Bytes server = server_ticket("CTRTester1", session_key);
    check(server.size() == 0x80 && Bytes(server.begin(), server.begin() + 24) == session_key, "server ticket carries the session key");

    // Typed buffers.
    Writer w;
    w.u64(7).u32(0).u8(18).string("{\"mm_id\":5}").boolean(true).blob(b("0102"));
    Reader r(w.data());
    const auto values = r.values();
    check(values.size() == 6 && values[0].value.as_u64() == 7 && values[3].value.text == "{\"mm_id\":5}" && values[4].kind == typed::BOOL &&
          values[5].value.text == "0102", "typed writer and reader");

    // HTTP proxy task: the bytes of demonware_rest.response.
    RestRequest rest{"POST", "http://abtesting/v2/users/psn-123/enrollments/", {}};
    check(hex(rest_response(rest)) == "1708630000000a200a0c436f6e74656e742d5479706512106170706c69636174696f6e2f6a736f6e10c8011a3c7b224142546f6b656e223a226374722d6c6f63616c2d6162222c2265787069726573496e223a333630302c22656e726f6c6c6d656e7473223a5b5d7d00",
          "REST A/B response bytes as demonware_rest.response");
    check(hex(rest_response(RestRequest{"GET", "http://x/", {}})) == "1708440000000a200a0c436f6e74656e742d5479706512106170706c69636174696f6e2f6a736f6e1094031a1d7b226572726f72223a7b226e616d65223a224e6f74466f756e64227d7d00",
          "REST 404 response bytes");

    // Discovery: the cases of tools/online/test_discovery.py and test_internet.py.
    DiscoveryLayout one{{"203.0.113.7", 3078}, {"203.0.113.7", 3080}, 3079, std::uint16_t(3081), "203.0.113.7"};
    auto ip = discovery_answer(Bytes{30, 3, 0}, {"127.0.0.1", 3174}, one);
    check(ip && ip->first == Endpoint{"203.0.113.7", 3078} && hex(ip->second) == "1f0300cb007107660c89a46326742d",
          "IP request answered from primary, loopback reported with the public address (discovery.py bytes)");
    auto nat = discovery_answer(Bytes{20, 2, 0, 3}, {"198.51.100.9", 4000}, one);
    check(nat && nat->first == Endpoint{"203.0.113.7", 3081} && hex(nat->second) == "150200c6336409a00fcb007107080c",
          "change address+port from the fourth endpoint (discovery.py bytes)");
    auto same = discovery_answer(Bytes{20, 2, 0, 0}, {"198.51.100.9", 4000}, one);
    check(same && same->first == Endpoint{"203.0.113.7", 3078}, "NAT request without change answered from primary");
    check(!discovery_answer(Bytes{20, 2, 0, 4}, {"198.51.100.9", 4000}, one), "unknown NAT request ignored");
    check(hex(discovery_address({"1.2.3.4", 0x1234}, true)) == "43a611252633", "protected address form");

    // Lobby handshake: a client built from the same contract, then records both ways.
    {
        Bytes header;
        for (const std::uint32_t v : {200u, 200u, 0xd2u, 0xdcu, 0x10000u}) for (int i = 0; i < 4; ++i) header.push_back(static_cast<unsigned char>(v >> (8 * i)));
        const Bytes nonce = random_bytes(8);
        header.insert(header.end(), nonce.begin(), nonce.end());
        const LobbyHandshake handshake(header);
        const Bytes server_header = handshake.server_header();
        Bytes auth = lobby_frame(0x82, Bytes(40, 0x5a));
        // Client proof: CLIENTCHAL[:8] over the material that includes the auth frame itself.
        Bytes material;
        for (const std::uint32_t v : {0xd2u, 0xdcu, 0x10000u}) for (int i = 0; i < 4; ++i) material.push_back(static_cast<unsigned char>(v >> (8 * i)));
        material.insert(material.end(), nonce.begin(), nonce.end());
        material.insert(material.end(), server_header.begin(), server_header.end());
        auth.insert(auth.end(), 8, 0);
        auth[0] = static_cast<unsigned char>(auth.size() - 4);
        material.insert(material.end(), auth.begin(), auth.end() - 8);
        const Bytes prk = hmac_sha1(sha1(material), session_key);
        const Bytes challenge = hkdf_expand_sha1(prk, "CLIENTCHAL", 16);
        std::copy(challenge.begin(), challenge.begin() + 8, auth.end() - 8);
        const auto result = handshake.complete(auth, {Bytes(24, 0x22), session_key});
        check(result && result->key_index == 1 && result->done_frame == lobby_frame(0x83, Bytes(challenge.begin() + 8, challenge.end())),
              "lobby handshake finds the session key and proves the server");
        if (result) {
            LobbySession server_side(result->keys);
            // Client record with the client keys.
            const Bytes keys = result->keys;
            const Bytes client_mac(keys.begin(), keys.begin() + 20), client_aes(keys.begin() + 40, keys.begin() + 56);
            const Bytes payload = b("26080a");
            Bytes inner{3, 0, 0, 0, 0x86};
            inner.insert(inner.end(), payload.begin(), payload.end());
            inner.resize(16, 0);
            const Bytes rec_iv = random_bytes(16);
            Bytes body{0xab, 0x85, 1, 0, 0, 0};
            body.insert(body.end(), rec_iv.begin(), rec_iv.end());
            const Bytes enc = aes128_cbc_encrypt(client_aes, rec_iv, inner);
            body.insert(body.end(), enc.begin(), enc.end());
            Bytes whole;
            const std::size_t size = body.size() + 8;
            for (int i = 0; i < 4; ++i) whole.push_back(static_cast<unsigned char>(size >> (8 * i)));
            whole.insert(whole.end(), body.begin(), body.end());
            const Bytes mac = hmac_sha1(client_mac, whole);
            body.insert(body.end(), mac.begin(), mac.begin() + 8);
            const auto opened = server_side.open_record(body);
            check(opened.first == 0x86 && opened.second == payload, "client record opened");
            bool replay = false;
            try { server_side.open_record(body); } catch (const ProtocolError&) { replay = true; }
            check(replay, "replayed record counter rejected");
            const Bytes sealed = server_side.seal_record(b("01"), 1);
            const Bytes server_mac(keys.begin() + 20, keys.begin() + 40);
            const Bytes expected_mac = hmac_sha1(server_mac, Bytes(sealed.begin(), sealed.end() - 8));
            check(std::equal(expected_mac.begin(), expected_mac.begin() + 8, sealed.end() - 8), "server record MAC");
        }
    }

    // Matchmaking: the scenario of tools/online/test_room_search.py and test_lobby_sync.py.
    {
        MatchmakingStore store;
        const std::string first = room_token("same"), other = room_token("different");
        const std::string zero67 = base64_encode(Bytes(67, 0));
        const std::string info = R"({"listen_server":{"local_address":")" + zero67 + R"(","security_id":")" + base64_encode(Bytes(8, 0)) +
                                 R"(","security_key":")" + base64_encode(Bytes(16, 0)) + R"(","nat":1},"dedicated_server":{"build_name":"synthetic"}})";
        std::map<std::uint64_t, std::vector<Param>> searches;
        for (const auto& [account, selector] : std::vector<std::pair<std::uint64_t, std::string>>{{42, first}, {43, first}, {44, other}}) {
            store.advertise({param(typed::STRING, Json::of(info))}, account, selector);
            const std::string document = R"({"members":[)" + std::to_string(account) + R"(],"tokens":[)" + std::to_string(store.player_token(account)) +
                                         R"(],"lobby_slots":{"min":2,"max":8},"dedicated_server":{"request_level":"not_needed"},"team_balance":{"num_teams":0},)"
                                         R"("ruleset_payload":{"filter":{"matchmaking_version":200,"playlist_id":0}},"player_id_to_slots":{")" +
                                         std::to_string(account) + R"(":0},"performance":{"game_type":1}})";
            searches[account] = {param(typed::STRING, Json::of(document))};
        }
        store.start_search(searches[42], 42, first);
        store.start_search(searches[44], 44, other);
        const std::map<std::uint64_t, std::string> available{{42, first}, {43, first}, {44, other}};
        check(store.match_ready(available).empty(), "one player per room: no lobby");
        bool wrong_room = false;
        try { store.start_search(searches[43], 43, other); } catch (const std::exception&) { wrong_room = true; }
        check(wrong_room, "search in another room than the advertisement rejected");
        const std::uint64_t id = store.start_search(searches[43], 43, first);
        check(id > 0 && id < (1ull << 52) && store.start_search(searches[43], 43, first) == id, "repeated search keeps its ID");
        check(store.match_ready({{42, other}, {43, first}, {44, other}}).empty(), "player connected in another room not paired");
        check(store.match_ready(available,{{first,3}}).empty(), "launcher lobby waits for all three members rather than pairing first two");
        const auto plans = store.match_ready(available,{{first,2}});
        check(plans.size() == 1 && plans[0].players.size() == 2 && plans[0].players[0].account == 42 && plans[0].players[1].account == 43 &&
              plans[0].host == 42, "same password pairs 42 and 43, 42 hosts");
        check(store.match_ready(available).empty(), "paired searches not paired again");
        if (plans.size() == 1) {
            const auto [host, backend] = lobby_documents(plans[0], parse_json(info), 100);
            check(dump_json(host).rfind(R"({"update_time":100,"update_id":1,"lobby_open":true,"lobby_open_for_pres_join":false,"player_state":{"42":0,"43":0},)", 0) == 0,
                  "host document order and values");
            store.save_documents(plans[0].lobby_id, host, backend, 1);
            Json next = host;
            next.set("attachment", Json::of(""));
            next.find("player_state")->set("43", Json::of(1));
            std::vector<Param> sync{param(typed::UINT64, Json::of(plans[0].lobby_id)), param(typed::UINT64, Json::of(1)),
                                    param(typed::STRING, Json::of(dump_json(next)))};
            bool not_host = false;
            try { store.sync_document(sync, 43, first); } catch (const std::exception&) { not_host = true; }
            check(not_host, "guest cannot update the lobby document");
            check(store.sync_document(sync, 42, first) && store.documents(plans[0].lobby_id)->update_id == 2, "host update accepted, version 2");
            check(!store.sync_document(sync, 42, first), "stale version refused");
            next.set("attachment", Json::null());
            next.set("update_id", Json::of(2));
            sync[1] = param(typed::UINT64, Json::of(2));
            sync[2] = param(typed::STRING, Json::of(dump_json(next)));
            check(store.sync_document(sync, 42, first) && store.documents(plans[0].lobby_id)->update_id == 3, "null attachment after race start");
            const auto saved=*store.documents(plans[0].lobby_id);
            auto successor_info=parse_json(info);
            successor_info.find("listen_server")->set("local_address",Json::of(base64_encode(Bytes(67,1))));
            successor_info.find("listen_server")->set("security_id",Json::of(base64_encode(Bytes(8,2))));
            successor_info.find("listen_server")->set("security_key",Json::of(base64_encode(Bytes(16,3))));
            store.advertise({param(typed::STRING,Json::of(dump_json(successor_info)))},43,first);
            check(store.promote_disconnected(43,{{42,first}}).empty(),"guest disconnect does not elect a host");
            check(store.promote_disconnected(42,{{43,other}}).empty(),"successor in another room rejected");
            check(store.promote_disconnected(42,{}).empty(),"no connected successor leaves documents unchanged");
            const auto promoted=store.promote_disconnected(42,{{43,first}});
            check(promoted.size()==1 && promoted[0].host==43 && promoted[0].lobby_id==plans[0].lobby_id,"connected member inherits the same lobby");
            const auto* migrated=store.documents(plans[0].lobby_id);
            check(migrated->update_id==4 && migrated->host.at("listen_server").at("host_player_id").as_u64()==43,"migration increments version and changes owner");
            auto expected_host=saved.host,expected_backend=saved.backend;
            expected_host.find("listen_server")->set("host_player_id",Json::of(43));
            expected_host.find("listen_server")->set("host_address",Json::of(base64_encode(Bytes(67,1))));
            expected_host.set("update_id",Json::of(4));expected_backend.set("update_id",Json::of(4));
            const auto routes=migrated->host.at("ctr_migration").text;
            check(routes.find("43="+hex(Bytes(67,1)))!=std::string::npos && routes.find("42=")==std::string::npos,"cached migration routes contain survivor address only");
            expected_host.set("ctr_migration",migrated->host.at("ctr_migration"));
            check(migrated->host==expected_host && migrated->backend==expected_backend,"migration preserves race attachment and security documents");
            check(store.promote_disconnected(42,{{43,first}}).empty(),"duplicate disconnect does not repeat election");
            bool former_host=false;
            try{store.sync_document(sync,42,first);}catch(const std::exception&){former_host=true;}
            check(former_host,"former owner cannot overwrite successor document");
            auto pruned=migrated->host;
            auto& fields=pruned.members;
            fields.erase(std::remove_if(fields.begin(),fields.end(),[](const auto& e){return e.first=="ctr_migration";}),fields.end());
            auto& states=pruned.find("player_state")->members;
            states.erase(std::remove_if(states.begin(),states.end(),[](const auto& e){return e.first=="42";}),states.end());
            sync[1]=param(typed::UINT64,Json::of(4));sync[2]=param(typed::STRING,Json::of(dump_json(pruned)));
            check(store.sync_document(sync,43,first),"successor can remove the disconnected former host");
            check(!store.documents(plans[0].lobby_id)->backend.at("players").find("42"),"departed host removed from backend membership");
            check(store.promote_disconnected(43,{{42,first}}).empty(),"former host reconnect cannot reclaim an old race");
        }
    }

    // CTR_ONLINE_MIN_PLAYERS environment variable overrides minimum players before pairing.
    {
        MatchmakingStore min_store;
        const std::string room_tok = room_token("triotest");
        const std::string min_info = R"({"listen_server":{"local_address":")" + base64_encode(Bytes(67, 0)) + R"(","security_id":")" +
                                     base64_encode(Bytes(8, 0)) + R"(","security_key":")" + base64_encode(Bytes(16, 0)) +
                                     R"(","nat":1},"dedicated_server":{"build_name":"synthetic"}})";
        std::map<std::uint64_t, std::vector<Param>> psearches;
        for (std::uint64_t a : {101ull, 102ull, 103ull}) {
            min_store.advertise({param(typed::STRING, Json::of(min_info))}, a, room_tok);
            const std::string sdoc = R"({"members":[)" + std::to_string(a) + R"(],"tokens":[)" + std::to_string(min_store.player_token(a)) +
                                     R"(],"lobby_slots":{"min":2,"max":8},"dedicated_server":{"request_level":"not_needed"},"team_balance":{"num_teams":0},)"
                                     R"("ruleset_payload":{"filter":{"matchmaking_version":200,"playlist_id":0}},"player_id_to_slots":{")" +
                                     std::to_string(a) + R"(":0},"performance":{"game_type":1}})";
            psearches[a] = {param(typed::STRING, Json::of(sdoc))};
        }
        _putenv("CTR_ONLINE_MIN_PLAYERS=3");
        min_store.start_search(psearches[101], 101, room_tok);
        min_store.start_search(psearches[102], 102, room_tok);
        const std::map<std::uint64_t, std::string> avail2{{101, room_tok}, {102, room_tok}};
        check(min_store.match_ready(avail2).empty(), "CTR_ONLINE_MIN_PLAYERS=3: 2 players do not match");
        min_store.start_search(psearches[103], 103, room_tok);
        const std::map<std::uint64_t, std::string> avail3{{101, room_tok}, {102, room_tok}, {103, room_tok}};
        const auto trio_plans = min_store.match_ready(avail3);
        check(trio_plans.size() == 1 && trio_plans[0].players.size() == 3, "CTR_ONLINE_MIN_PLAYERS=3: 3 players match together");
        _putenv("CTR_ONLINE_MIN_PLAYERS=");
    }

    // Public matchmaking (no room token): players of the same game playlist are paired with each
    // other, never with another playlist or with a private room choosing the same playlist.
    {
        const std::string& pub = public_room_token();
        check(pub == "57b9a23ab919f9933c4617830fcc991c2c519db7daa92ab333f6527eb39dcda0" && valid_room_token(pub) &&
              pub != room_token("ctr-public-room-v1") && pub != room_token("public"), "public selector as room.py, distinct from passwords");
        MatchmakingStore store;
        const std::string info = R"({"listen_server":{"local_address":")" + base64_encode(Bytes(67, 0)) + R"(","security_id":")" +
                                 base64_encode(Bytes(8, 0)) + R"(","security_key":")" + base64_encode(Bytes(16, 0)) +
                                 R"(","nat":1},"dedicated_server":{"build_name":"synthetic"}})";
        const std::string private_room = room_token("friends only");
        const std::map<std::uint64_t, std::pair<std::string, int>> players{
            {201, {pub, 0}}, {202, {pub, 3}}, {203, {private_room, 0}}, {204, {pub, 0}}};
        std::map<std::uint64_t, std::string> available;
        for (const auto& [account, choice] : players) {
            store.advertise({param(typed::STRING, Json::of(info))}, account, choice.first);
            const std::string doc = R"({"members":[)" + std::to_string(account) + R"(],"tokens":[)" + std::to_string(store.player_token(account)) +
                                    R"(],"lobby_slots":{"min":2,"max":8},"dedicated_server":{"request_level":"not_needed"},"team_balance":{"num_teams":0},)"
                                    R"("ruleset_payload":{"filter":{"matchmaking_version":200,"playlist_id":)" + std::to_string(choice.second) +
                                    R"(}},"player_id_to_slots":{")" + std::to_string(account) + R"(":0},"performance":{"game_type":1}})";
            store.start_search({param(typed::STRING, Json::of(doc))}, account, choice.first);
            available[account] = choice.first;
        }
        const auto plans = store.match_ready(available);
        bool paired = plans.size() == 1 && plans[0].players.size() == 2 && plans[0].selector == pub;
        if (paired) for (const auto& p : plans[0].players) paired = paired && (p.account == 201 || p.account == 204);
        check(paired, "public: same playlist paired, other playlist and private room left waiting");
    }

    // Relay (relay.hpp): tokens, bindings, forwarding within one room, addresses shown, refusals.
    {
        RelayLimits limits;
        limits.bytes_per_second = 1000;
        limits.burst = 2000;
        limits.binding_timeout = 60;
        Relay relay(limits);
        const std::string room_a = room_token("relay a"), room_b = room_token("relay b");
        const Endpoint pub1{"198.51.100.1", 3174}, pub2{"198.51.100.2", 3274}, pub3{"198.51.100.3", 3374};
        const Endpoint loc1{"192.168.1.10", 3074}, loc2{"192.168.1.20", 3074};
        const Endpoint nat1{"203.0.113.1", 40001}, nat2{"203.0.113.2", 40002}, nat3{"203.0.113.3", 40003};
        std::vector<RelayMember> members_a{{1, pub1, loc1}, {2, pub2, loc2}}, members_b{{3, pub3, std::nullopt}};
        const RelayRoster roster = [&](const std::string& room) { return room == room_a ? members_a : room == room_b ? members_b : std::vector<RelayMember>{}; };
        auto type_of = [](const Relay::Outcome& o) { return o.out.size() == 1 && o.out[0].data.size() >= 6 ? int(o.out[0].data[5]) : -1; };
        auto reason_of = [](const Relay::Outcome& o) { return o.out.size() == 1 && o.out[0].data.size() == 13 ? int(o.out[0].data[12]) : -1; };
        auto send = [](const Endpoint& to, const std::string& text) { return relay::encode(relay::SEND, to, bytes_of(text)); };

        const Bytes t1 = relay.issue(1, room_a), t2 = relay.issue(2, room_a), t3 = relay.issue(3, room_b);
        Relay::Outcome o = relay.handle(relay::hello(Bytes(16, 7)), nat1, 0, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: HELLO with an unknown token refused");
        check(relay.handle(bytes_of("not relay"), nat1, 0, roster).out.empty(), "relay: foreign datagrams get no answer");
        o = relay.handle(relay::hello(t1), nat1, 0, roster);
        check(type_of(o) == relay::WELCOME && o.out[0].to == nat1 && o.out[0].data == relay::encode(relay::WELCOME, nat1, Bytes{0, 60}) &&
              o.event == "relay_bound", "relay: HELLO binds the endpoint and reports what the server saw");
        o = relay.handle(send(pub2, "x"), nat1, 1, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::DESTINATION_OFFLINE, "relay: player without binding is offline");
        relay.handle(relay::hello(t2), nat2, 1, roster);
        relay.handle(relay::hello(t3), nat3, 1, roster);
        o = relay.handle(send(pub2, "hello host"), nat1, 2, roster);
        check(o.out.size() == 1 && o.out[0].to == nat2 && o.out[0].data == relay::encode(relay::DELIVER, pub1, bytes_of("hello host")),
              "relay: SEND to the advertised public endpoint delivered, sender shown by its public endpoint");
        o = relay.handle(send(pub1, "reply"), nat2, 2, roster);
        check(o.out.size() == 1 && o.out[0].to == nat1 && o.out[0].data == relay::encode(relay::DELIVER, pub2, bytes_of("reply")),
              "relay: reply delivered with the address the first sender used");
        o = relay.handle(send(loc2, "lan"), nat1, 3, roster);
        check(o.out.size() == 1 && o.out[0].to == nat2, "relay: advertised local endpoint resolves when unambiguous");
        o = relay.handle(send(pub1, "back"), nat2, 3, roster);
        check(o.out.size() == 1 && o.out[0].data == relay::encode(relay::DELIVER, loc2, bytes_of("back")),
              "relay: source shown is the address the receiving game used last");
        o = relay.handle(send(pub3, "other room"), nat1, 4, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::UNKNOWN_DESTINATION, "relay: player of another room unreachable");
        o = relay.handle(send(pub1, "stranger"), Endpoint{"203.0.113.9", 1}, 4, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: unregistered sender refused");
        o = relay.handle(relay::encode(relay::SEND, pub2, Bytes(relay::kMaxPayload + 1, 0)), nat1, 5, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::MALFORMED, "relay: oversized payload refused");
        o = relay.handle(relay::encode(relay::SEND, pub2, Bytes(1400, 0)), nat1, 6, roster);
        check(o.out.size() == 1 && o.out[0].to == nat2, "relay: datagram within the byte budget forwarded");
        o = relay.handle(relay::encode(relay::SEND, pub2, Bytes(1400, 0)), nat1, 6, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::RATE_LIMITED && o.event == "relay_rate_limited", "relay: byte rate limited");
        o = relay.handle(relay::encode(relay::SEND, pub2, Bytes(1000, 0)), nat1, 8, roster);
        check(o.out.size() == 1 && o.out[0].to == nat2, "relay: budget refilled over time");
        o = relay.handle(relay::hello(t1), Endpoint{"203.0.113.1", 40999}, 9, roster);
        check(type_of(o) == relay::WELCOME && o.event == "relay_bound", "relay: new NAT mapping rebinds the player");
        o = relay.handle(send(pub2, "old mapping"), nat1, 9, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: previous endpoint no longer accepted");
        members_a = {{2, pub2, loc2}};
        o = relay.handle(send(pub1, "after leaving"), nat2, 10, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::UNKNOWN_DESTINATION, "relay: player who left the room unreachable");
        o = relay.handle(send(pub2, "expired"), nat2, 100, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: binding expires without traffic");
        relay.forget(3);
        o = relay.handle(relay::hello(t3), nat3, 100, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: token withdrawn when the player leaves the lobby");
        const Bytes block = b("c0a8017d020c" + std::string(108, '0') + "c0a8017dca0c01");
        check(block_endpoint(block, 0) == Endpoint{"192.168.1.125", 3074} && block_endpoint(block, 60) == Endpoint{"192.168.1.125", 3274} &&
              !block_endpoint(Bytes(67, 0), 60), "relay: endpoints read from the retail advertisement block");
    }

    // Statistics and sessions.
    {
        StatsStore stats(dir / "stats.txt");
        stats.write({param(typed::UINT32, Json::of(7)), param(typed::UINT64, Json::of(42)), param(typed::UINT8, Json::of(0)),
                     param(typed::INT64, Json::of(-3))}, 42);
        bool other_account = false;
        try {
            stats.write({param(typed::UINT32, Json::of(7)), param(typed::UINT64, Json::of(43)), param(typed::UINT8, Json::of(0)),
                         param(typed::INT64, Json::of(1))}, 42);
        } catch (const std::exception&) { other_account = true; }
        StatsStore reloaded(dir / "stats.txt");
        check(other_account && reloaded.size() == 1, "statistics persisted, other account refused");
        SessionStore sessions;
        const std::string meta = R"({"sessionPrivacy":"private","sessionMaxUser":8,"sessionName":"x","availablePlatforms":["PS4"]})";
        const std::string body = "--B\r\nContent-Type: application/json\r\nContent-Description: session-request\r\nContent-Length: " +
                                 std::to_string(meta.size()) + "\r\n\r\n" + meta + "\r\n--B\r\nContent-Type: application/octet-stream\r\n"
                                 "Content-Description: changeable-session-data\r\nContent-Length: 2\r\n\r\nab";
        const Json created = sessions.create(42, "multipart/mixed; boundary=B", bytes_of(body));
        check(created.find("sessionId") && created.at("sessionId").text.size() == 32, "retail session multipart without final delimiter");
        bool bad = false;
        try { sessions.create(42, "multipart/mixed; boundary=B", bytes_of(body + "x")); } catch (const std::exception&) { bad = true; }
        check(bad, "data after the final part refused");
        // Project transport: synthetic invitations, independent of a retail join.
        const auto sender = user_id("InviteHost");
        const auto session = sessions.create(sender, "multipart/mixed; boundary=B", bytes_of(body)).at("sessionId").text;
        const auto sent = sessions.invite(sender, "InviteHost", session, {43,44}, 1000);
        check(sent.items.size() == 2 && sessions.invitations(43, 1001).items.size() == 1 &&
              sessions.invitations(45, 1001).items.empty(), "invitation: batch delivery isolated to recipients");
        check(sessions.invite(sender, "InviteHost", session, {43}, 1002).items[0].at("invitationId") ==
              sent.items[0].at("invitationId"), "invitation: send retry reuses outstanding ID");
        const auto refused = [](auto action) { try { action(); return false; } catch (const std::exception&) { return true; } };
        check(refused([&] { sessions.invite(43, "Other", session, {44}, 1003); }) &&
              refused([&] { sessions.invite(sender, "InviteHost", session, {45,45}, 1003); }) &&
              sessions.invitations(45,1003).items.empty(), "invitation: nonowner and duplicate batch rejected without delivery");
        check(refused([&] { sessions.dismiss(44, sent.items[0].at("invitationId").text, 1003); }),
              "invitation: another recipient cannot consume invite");
        check(sessions.invitations(43,1004).items.size() == 1, "invitation: polling does not consume");
        sessions.dismiss(43, sent.items[0].at("invitationId").text, 1005);
        check(sessions.invitations(43,1006).items.empty() && sessions.invitations(44,1006).items.size() == 1,
              "invitation: dismissal affects only selected recipient");
        check(sessions.invitations(44,1300).items.empty(), "invitation: stale session invitations expire");
    }

    // Synthetic Delta lifecycle. A reservation must not masquerade as a published lobby.
    {
        MatchmakingStore store;
        const auto text = [](const std::string& s) { return param(typed::STRING, Json::of(s)); };
        const auto blob = [](const Bytes& b) { return param(typed::BLOB, Json::of(hex(b))); };
        const auto number = [](int n) { return param(typed::UINT32, Json::of(n)); };
        const auto refused = [](auto action) { try { action(); return false; } catch (const std::exception&) { return true; } };
        const auto selector = room_token("delta-fixture");
        check(refused([&] { store.reserve_session({text("Delta")}, 42, ""); }), "Delta: invalid reservation selector refused");
        const auto id = store.reserve_session({text("Delta")}, 42, selector);
        check(store.friend_sessions("Delta", {42}, 43).empty(), "Delta: reservation hidden before initialization");
        const std::vector<Param> update{text("Delta"), blob(id), blob(Bytes(67, 0)), number(8), number(8), blob(Bytes{1,2,3})};
        check(refused([&] { store.update_session(update, 43, true); }), "Delta: other account cannot initialize");
        store.update_session(update, 42, true);
        auto players = update;
        players.insert(players.begin() + 2, number(2));
        store.update_session_players(players, 42, selector);
        const auto rows = store.friend_sessions("Delta", {42}, 43);
        check(rows.size() == 1 && rows[0].id == id && rows[0].players == 2 && rows[0].custom == Bytes({1,2,3}),
              "Delta: two players use one info block");
        players.push_back(number(9));
        check(refused([&] { store.update_session_players(players, 42, selector); }) &&
              store.friend_sessions("Delta", {42}, 43)[0].players == 2, "Delta: malformed update preserves published state");
        check(store.friend_sessions("Delta", {42}, 42).empty() &&
              store.friend_sessions("Other", {42}, 43).empty(), "Delta: self and other context excluded");
        check(refused([&] { store.delete_session({text("Delta"), blob(id)}, 43); }), "Delta: other account cannot delete");
        store.delete_session({text("Delta"), blob(id)}, 42);
        check(store.friend_sessions("Delta", {42}, 43).empty(), "Delta: deletion removes friend session");
    }

    // Pit Stop and Wumpa Challenges: exact retail v01.21 requests of
    // tools/online/test_local_pitstop.py; expected replies (after the transaction ID) produced
    // by the Python reference handlers (demonware_tasks.handle) for account 42.
    {
        MarketplaceStore market(dir / "marketplace.txt");
        AchievementStore achievements(dir / "achievement_events.jsonl");
        auto answer = [&](const std::string& request, std::uint64_t account = 42) {
            const std::optional<Bytes> reply = pitstop_task(parse_task_request(b(request)), account, market, achievements);
            return reply ? hex(Bytes(reply->begin() + 9, reply->end())) : std::string("none");
        };
        const std::string inventory = "5003a5106f6374616e655f70736e000801000000085803000000";
        const std::string balances = "5003f517080e0000000a0a6f6374616e655f70736e100300";
        const std::string login = "7d030117082e0000000a0a6f6374616e655f70736e129b8080000a056c6f67696e10f6bac3d606"
                                  "1a878080000a03656e76100220001a0000";
        auto states = [](int kind) {
            char middle[3];
            std::snprintf(middle, sizeof middle, "%02x", kind);
            return "7d03031708120000000a0a6f6374616e655f70736e10" + std::string(middle) + "1864220000";
        };
        const TaskRequest login_request = parse_task_request(b(login));
        check(login_request.params.size() == 1 && login_request.params[0].kind == typed::STRUCT &&
              dump_json(login_request.params_json()) == R"([[23,"0a0a6f6374616e655f70736e129b8080000a056c6f67696e10f6bac3d6061a878080000a03656e76100220001a00"]])",
              "pit stop: struct buffer parameter logged as the reference");
        check(answer(inventory) == "080000000003a50800000000", "pit stop: retail inventory page answered empty");
        check(answer(balances) == "080000000003f517080000000000", "pit stop: retail balances answered with an empty struct");
        check(answer(login) == "080000000003010800000000", "wumpa: retail login event acknowledged");
        bool all_kinds = true;
        for (int kind = 0; kind < 8; ++kind) all_kinds = all_kinds && answer(states(kind)) == "0800000000030317080000000000";
        check(all_kinds, "wumpa: retail states of the eight kinds answered empty");
        std::ifstream events_file(dir / "achievement_events.jsonl");
        std::string line, extra;
        std::getline(events_file, line);
        const Json row = parse_json(line);
        check(!std::getline(events_file, extra) && row.keys() == std::vector<std::string>{"account", "received", "event", "request_hex"} &&
              row.at("account").text == "42" && row.at("event").text == R"({"name": "login", "time": 1792073078})" &&
              row.at("request_hex").text == login_request.params[0].value.text,
              "wumpa: event summary and complete retail attribute bytes persisted");
        const auto blocked_path=dir / "event_file_as_directory";
        std::filesystem::create_directories(blocked_path);
        AchievementStore blocked_events(blocked_path);
        const auto failed_write=pitstop_task(login_request,42,market,blocked_events);
        check(failed_write && failed_write->size() >= 9 &&
              hex(Bytes(failed_write->begin() + 9, failed_write->end())) == "08010000000301",
              "wumpa: persistence failure returns task error instead of acknowledging lost events");
        check(answer("0403" "0801") == "none", "pit stop: other tasks left to the server");

        market.set_balance(42, 1, 500);
        market.set_balance(42, 2, -3);
        market.set_balance(43, 1, 9);
        check(answer(balances) == "080000000003f517080d0000000a05080110e8070a040802100500", "pit stop: balances per account, zigzag");
        check(MarketplaceStore(dir / "marketplace.txt").balances_message(parse_task_request(b(balances)).params, 43) ==
              b("0a0408011012"), "pit stop: balances reloaded from the state file");
        bool wide_currency = false;
        try { market.set_balance(42, 256, 1); } catch (const std::exception&) { wide_currency = true; }
        check(wide_currency, "pit stop: currency IDs above one byte refused");
        MarketplaceStore(dir / "marketplace.txt").set_balance(43, 1, 7500); // as ctr_online_server --set-balance
        check(answer(balances, 43) == "080000000003f51708070000000a05080110987500",
              "pit stop: balance set from outside reaches the running store");
        market.set_balance(43, 1, 0);
        check(answer(balances, 43) == "080000000003f517080000000000", "pit stop: zero balance removed");
        market.set_inventory(42, 7, 1);
        check(answer(inventory) == "080100000003a5", "pit stop: nonempty inventory refused (layout not validated)");
        check(answer(inventory, 43) == "080000000003a50800000000", "pit stop: other account's inventory unaffected");
        std::string wrong = balances;
        wrong.replace(wrong.find("6f6374616e655f70736e"), 20, hex(bytes_of("octane_xbx")));
        check(answer(wrong) == "080100000003f5", "pit stop: other balances context refused");
        check(answer("7d03031708120000000a0a6f6374616e655f70736e10081864220000") == "08010000000303", "wumpa: state kind 8 refused");
        check(answer("7d03031708130000000a0a6f6374616e655f70736e100018642201780000") == "08010000000303", "wumpa: paged states refused");
        check(answer("7d030117080c0000000a0a6f6374616e655f70736e00") == "08010000000301", "wumpa: report without events refused");
        check(entitlement_page("entitlement_type=service&service_label=0&start=0&size=100") &&
              dump_json_spaced(*entitlement_page("start=0&size=100&service_label=0&entitlement_type=service")) ==
                  R"({"total_results": 0, "entitlements": []})" &&
              !entitlement_page("start=0&size=100&service_label=0&entitlement_type=unified") &&
              !entitlement_page("start=0&size=0&service_label=0&entitlement_type=service") &&
              !entitlement_page("start=0&size=100&entitlement_type=service") &&
              !entitlement_page("start=0&size=100&service_label=0&entitlement_type=service&entitlement_type=service"),
              "entitlements: empty service list, other requests refused");
    }

    // Synthetic economy fixtures exercise persistence, not retail reward parity.
    {
        const auto economy = dir / "economy";
        std::filesystem::create_directories(economy);
        std::ofstream(economy / "retail_reward_catalog.json") <<
            R"({"schema":1,"eboot_sha256":"39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5","tracks":[{"id":101,"ranks":[40,30,20,10]}]})";
        auto add = [](Bytes& target, const Bytes& part) { target.insert(target.end(), part.begin(), part.end()); };
        auto field = [&](unsigned tag, const Bytes& value) {
            Bytes out = pb_varint(tag * 8 + 2); add(out, pb_varint(value.size())); add(out, value); return out;
        };
        auto race = [&](unsigned timestamp, unsigned mode = 1, unsigned duration = 120, unsigned rank = 2, unsigned laps = 3, unsigned online = 0, unsigned track = 101, unsigned golden = 0) {
            Bytes event = field(1, bytes_of("end_race")); add(event, pb_varint(16)); add(event, pb_varint(timestamp));
            for (const auto& [key, value] : std::map<std::string, unsigned>{
                    {"Track",track},{"Rank",rank},{"RaceMode",mode},{"CompletedLap",laps},
                    {"Online",online},{"RewardGoldenWumpa",golden},{"Time",duration}}) {
                Bytes attr = field(1, bytes_of(key)); add(attr, pb_varint(16)); add(attr, pb_varint(value));
                add(event, field(3, attr));
            }
            Bytes message = field(1, bytes_of("octane_psn")); add(message, field(2, event));
            return std::vector<Param>{{typed::STRUCT, Json::of(hex(message))}};
        };
        const auto balance_request = parse_task_request(b("5003f517080e0000000a0a6f6374616e655f70736e100300")).params;
        const auto wallet = economy / "marketplace.txt";
        MarketplaceStore market(wallet);
        market.award_races(race(999), 41);
        check(hex(market.balances_message(balance_request,41)) == "0a04080a103c", "reward: modifier defaults to normal base payout");
        market.award_races(race(999), 41,100);
        check(hex(market.balances_message(balance_request,41)) == "0a04080a103c", "reward: changing multiplier does not repay old event");
        market.award_races(race(1000), 42,100);
        check(hex(market.balances_message(balance_request,42)) == "0a05080a10f02e", "reward: base amount read from local catalogue");
        market.award_races(race(1000), 42,100);
        MarketplaceStore restarted(wallet);
        restarted.award_races(race(1000), 42,100);
        check(hex(restarted.balances_message(balance_request,42)) == "0a05080a10f02e", "reward: replay does not credit twice across restart");
        restarted.award_races(race(1000), 43,100);
        check(hex(restarted.balances_message(balance_request,43)) == "0a05080a10f02e", "reward: receipts scoped to account");
        bool reward_rejected = false;
        try { restarted.award_races(race(1001, 9, 0), 42,100); } catch (const std::exception&) { reward_rejected = true; }
        check(reward_rejected && hex(restarted.balances_message(balance_request,42)) == "0a05080a10f02e", "reward: zero-duration activity refused without wallet change");
        restarted.award_races(race(2000,9,120,2,0,1,101,1),44,100);
        check(hex(restarted.balances_message(balance_request,44)) == "0a05080a10f02e",
              "reward: alternate mode, zero laps, online and golden flag accept common x100 policy");
        restarted.award_races(race(2001,2,120,0,0),45,100);
        check(hex(restarted.balances_message(balance_request,45)) == "0a05080a10c03e",
              "reward: unranked activity uses first-place base x100");
        restarted.award_races(race(2002,8,120,1,0,0,999),46,100);
        check(hex(restarted.balances_message(balance_request,46)) == "0a05080a108032",
              "reward: unknown arena uses common 3200 payout");
        MarketplaceStore no_catalogue(dir / "economy_no_catalogue" / "marketplace.txt");
        no_catalogue.award_races(race(2003),47,100);
        check(hex(no_catalogue.balances_message(balance_request,47)) == "0a05080a108032",
              "reward: missing retail catalogue does not block activity credit");
        std::filesystem::create_directory(wallet.string() + ".tmp");
        reward_rejected = false;
        try { restarted.award_races(race(1001), 42,100); } catch (const std::exception&) { reward_rejected = true; }
        check(reward_rejected && hex(restarted.balances_message(balance_request,42)) == "0a05080a10f02e" &&
              hex(MarketplaceStore(wallet).balances_message(balance_request,42)) == "0a05080a10f02e",
              "reward: failed write rolls back balance and leaves disk intact");
        std::filesystem::remove(wallet.string() + ".tmp");
        restarted.award_races(race(1001), 42,100);
        check(hex(restarted.balances_message(balance_request,42)) == "0a05080a10e05d", "reward: failed transaction can retry successfully");
        restarted.set_balance(42,10,INT64_MAX);
        reward_rejected = false;
        try { restarted.award_races(race(1002),42,100); } catch (const std::exception&) { reward_rejected = true; }
        check(reward_rejected && restarted.balances_message(balance_request,42) == MarketplaceStore(wallet).balances_message(balance_request,42),
              "reward: balance overflow refused without mutation");
    }

    // Synthetic single-item purchase transaction and inventory wire record.
    {
        const auto shop = dir / "shop";
        std::filesystem::create_directories(shop);
        std::ofstream(shop / "retail_shop_catalog.json") << R"({"schema":1,"eboot_sha256":"39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5","skus":[{"id":21,"currency":10,"price":15,"item":31,"quantity":1}]})";
        const auto wallet = shop / "marketplace.txt";
        MarketplaceStore market(wallet);
        Writer request; request.string("octane_psn").u32(1).u32(21).u32(1).u32(1).boolean(false).u32(1).u64(0).string("").u32(0).u32(0).u8(0).u32(0);
        const auto purchase = Reader(request.data()).values();
        const auto balances = parse_task_request(b("5003f517080e0000000a0a6f6374616e655f70736e100300")).params;
        const auto page = parse_task_request(b("5003a5106f6374616e655f70736e000801000000085803000000")).params;
        market.set_balance(42,10,100);
        market.purchase(purchase,42);
        check(hex(market.balances_message(balances,42)) == "0a05080a10aa01", "shop: purchase debits catalogue price");
        MarketplaceStore restarted(wallet);
        restarted.purchase(purchase,42);
        const auto inventory_bytes = restarted.inventory_page(page,42);
        const auto record = Reader(inventory_bytes).values();
        check(record.size()==12 && record[0].value.as_u64()==1 && record[1].value.as_u64()==1 &&
              record[2].value.as_u64()==42 && record[3].value.text=="psn" && record[4].value.as_u64()==31 &&
              record[5].value.as_u64()==1 && record[9].value.as_i64()==-1 && record[10].kind==typed::UINT16,
              "shop: persisted inventory serializes account, item, quantity and expiry fields");
        check(hex(restarted.balances_message(balances,42)) == "0a05080a10aa01", "shop: retry after restart does not charge twice");
        restarted.set_balance(43,10,10);
        bool shop_rejected=false;
        try { restarted.purchase(purchase,43); } catch(const std::exception&) { shop_rejected=true; }
        check(shop_rejected && restarted.inventory_page(page,43).empty(), "shop: insufficient balance grants no item");
        auto unknown=purchase; unknown[2].value=Json::of(999);
        shop_rejected=false;
        try { restarted.purchase(unknown,42); } catch(const std::exception&) { shop_rejected=true; }
        check(shop_rejected, "shop: unknown SKU rejected");
        restarted.set_balance(44,10,100);
        std::filesystem::create_directory(wallet.string()+".tmp");
        shop_rejected=false;
        try { restarted.purchase(purchase,44); } catch(const std::exception&) { shop_rejected=true; }
        check(shop_rejected && restarted.inventory_page(page,44).empty() &&
              hex(restarted.balances_message(balances,44))=="0a05080a10c801", "shop: failed persistence rolls back money and item");
        std::filesystem::remove(wallet.string()+".tmp");
        restarted.purchase(purchase,44);
        check(!MarketplaceStore(wallet).inventory_page(page,44).empty(), "shop: failed purchase can retry successfully");
        check(std::filesystem::is_directory(shop / "backups/local") &&
              std::filesystem::directory_iterator(shop / "backups/local")!=std::filesystem::directory_iterator(),
              "backup: committed wallet versions archived before replacement");
        const auto bad_backup=dir / "blocked_backup";
        std::filesystem::create_directories(bad_backup);
        MarketplaceStore blocked(bad_backup / "marketplace.txt");
        blocked.set_balance(42,10,100);
        { std::ofstream deny(bad_backup / "backups"); deny << "block directory creation"; }
        bool backup_failed=false;
        try { blocked.set_balance(42,10,50); } catch(const std::exception&) { backup_failed=true; }
        check(backup_failed && blocked.snapshot(42).at("state").text=="balance 42 10 100\n" &&
              MarketplaceStore(bad_backup / "marketplace.txt").snapshot(42).at("state").text=="balance 42 10 100\n",
              "backup: disk failure prevents wallet overwrite and rolls back memory");
        std::ofstream(shop / "retail_shop_catalog.json") << R"({"schema":1,"eboot_sha256":"39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5","skus":[{"id":21,"currency":10,"price":15,"item":31,"quantity":1},{"id":22,"currency":10,"price":20,"item":32,"quantity":1},{"id":23,"components":[21,22]}]})";
        auto bundle_request=[](std::vector<unsigned> ids) {
            Writer w; w.string("octane_psn").u32(static_cast<unsigned>(ids.size()));
            for(auto id:ids) w.u32(id);
            w.u32(static_cast<unsigned>(ids.size())); for(std::size_t i=0;i<ids.size();++i) w.u32(1);
            w.boolean(false).u32(static_cast<unsigned>(ids.size())); for(std::size_t i=0;i<ids.size();++i) w.u64(0).string("");
            w.u32(0).u32(0).u8(0).u32(0); return Reader(w.data()).values();
        };
        const auto bundle=bundle_request({23,21,22});
        restarted.set_balance(45,10,100);
        auto rejects=[&](const std::vector<Param>& p, unsigned account) {
            try { restarted.purchase(p,account); return false; } catch(const std::exception&) { return true; }
        };
        check(rejects(bundle_request({23,21}),45) && restarted.inventory_page(page,45).empty(), "shop: incomplete bundle cannot partially grant");
        check(rejects(bundle_request({23,21,21}),45), "shop: duplicate bundle SKU rejected");
        restarted.purchase(bundle,45);
        check(Reader(restarted.inventory_page(page,45)).values()[0].value.as_u64()==2 &&
            hex(restarted.balances_message(balances,45))=="0a05080a108201", "shop: bundle debits sum and grants both items");
        MarketplaceStore(wallet).purchase(bundle,45);
        check(hex(restarted.balances_message(balances,45))=="0a05080a108201", "shop: bundle retry after restart is free");
        restarted.purchase(bundle_request({23,22}),42);
        check(hex(restarted.balances_message(balances,42))=="0a05080a108201", "shop: owned component excluded from bundle price");
        restarted.set_balance(46,10,30);
        check(rejects(bundle,46) && restarted.inventory_page(page,46).empty(), "shop: insufficient bundle balance grants nothing");
        restarted.set_balance(47,10,100);
        std::filesystem::create_directory(wallet.string()+".tmp");
        check(rejects(bundle,47) && restarted.inventory_page(page,47).empty() &&
            hex(restarted.balances_message(balances,47))=="0a05080a10c801", "shop: bundle write failure rolls back every item and debit");
        std::filesystem::remove(wallet.string()+".tmp");

    }

    // The launcher's hosting: server started in process on the default ports, then stopped.
    {
        const std::filesystem::path root = dir / "root";
        Host host;
        const HostStatus status = host.start(root, "192.0.2.10", false);
        std::string body;
        const int config = http_get("127.0.0.1", 8780, "/ctr/v1/config", body);
        check(status.running && status.address_source == "manual" && config == 200 &&
              body == R"({"discovery_port": 3078, "public_host": "192.0.2.10"})", "hosted server answers with the public address");
        std::string key_body;
        std::ifstream key_file(status.traffic_key, std::ios::binary);
        const std::string key_on_disk{std::istreambuf_iterator<char>(key_file), {}};
        check(http_get("127.0.0.1", 8780, "/ctr/v1/traffic-key", key_body) == 200 && key_body.size() == 0x126 && key_body == key_on_disk,
              "traffic key served equals the key file the host's game uses");
        check(!status.friend_code.empty(), "host assigns a valid friend code");
        std::string id_resp;
        const int id_code = http_get("127.0.0.1", 8780, "/ctr/v1/identity?nonce=0123456789abcdef0123456789abcdef", id_resp);
        check(id_code == 200, "identity endpoint responds with 200 for valid nonce");
        const Json id_json = parse_json(id_resp);
        check(id_json.at("protocol").text == "ctr-friend-auth-v1", "identity response protocol is ctr-friend-auth-v1");

        // Candidate verification via verify_candidate_endpoint
        const VerifyResult v_ok = verify_candidate_endpoint("127.0.0.1:8780", status.friend_code, 2000);
        check(v_ok.status == VerifyStatus::Verified, "candidate endpoint successfully verified against friend code");
        check(!v_ok.traffic_key.empty() && v_ok.traffic_key.size() == 0x126, "verified candidate returns matching traffic key");

        FriendCodeInfo expected_friend;
        parse_friend_code(status.friend_code, expected_friend);
        check(verify_friend_document("{}", expected_friend, "nonce").status == VerifyStatus::MalformedResponse,
              "missing identity fields return MalformedResponse without throwing");
        check(verify_friend_document(R"({"protocol":"ctr-friend-auth-v1","version":1,"timestamp":18446744073709551615})",
              expected_friend, "nonce").status == VerifyStatus::MalformedResponse,
              "extreme unsigned timestamp rejected without signed overflow");
        FriendStore host_book(root / "runtime/user/online/friends.json");
        host_book.add_or_update(status.friend_code, "Host");
        host_book.update_endpoint(status.friend_code, "127.0.0.1:8780", "saved");
        check(find_friend_server(root, status.friend_code, 50, 2000, {}, host.dht()).status == VerifyStatus::Verified,
              "friend search while hosting reuses DHT and verifies cached endpoint");
        check(host.dht()->is_running(), "friend search does not stop host DHT");

        const auto guest_root=dir/"party_guest_root";
        const auto guest=HostIdentity::load_or_create(guest_root/"runtime/user/identity/host_identity.json");
        FriendStore permitted(root/"runtime/user/online/friends.json");
        permitted.add_or_update(guest.friend_code(),"Guest");
        Json configure=Json::object();configure.set("action",Json::of("configure"));configure.set("name",Json::of("Owner"));
        const auto local_party=party_request(root,"127.0.0.1",status.friend_code,configure);
        Json join=Json::object();join.set("action",Json::of("join"));join.set("name",Json::of("Guest"));
        const auto remote_party=party_request(guest_root,"127.0.0.1",status.friend_code,join);
        check(remote_party.at("room").text==local_party.at("room").text && remote_party.at("count").as_u64()==2,
              "signed HTTP join assigns same retail network selector to two launchers");
        Json heartbeat=Json::object();heartbeat.set("action",Json::of("heartbeat"));
        check(party_request(guest_root,"127.0.0.1",status.friend_code,heartbeat).at("count").as_u64()==2,
              "signed HTTP heartbeat preserves party membership");
        MarketplaceStore backup_wallet(root / "runtime/user/online/marketplace.txt");
        backup_wallet.set_balance(user_id("Guest"),10,8500);
        backup_wallet.set_inventory(user_id("Guest"),139,1);
        backup_wallet.set_balance(user_id("Another"),10,999);
        auto copied = party_request(guest_root,"127.0.0.1",status.friend_code,heartbeat);
        if (const auto* error=copied.find("backup_error")) std::fprintf(stderr,"Backup error: %s\n",error->text.c_str());
        const auto saved = copied.at("wallet_backup");
        check(!copied.find("backup_error") && saved.at("account").as_u64()==user_id("Guest") &&
              saved.at("state").text.find("139 1")!=std::string::npos &&
              saved.at("state").text.find(std::to_string(user_id("Another")))==std::string::npos,
              "backup: signed host response contains only authenticated member wallet");
        const auto archives = backup_io_path(guest_root / "runtime/user/online/backups/hosts" /
            hex(sha256(bytes_of(status.friend_code))) / std::to_string(user_id("Guest")));
        const auto previous_copy = archives / (hex(sha256(bytes_of(dump_json(saved)))) + ".json");
        check(std::filesystem::exists(previous_copy), "backup: remote wallet actually persisted on guest disk");
        const auto count_files = [](const auto& folder) {
            return std::distance(std::filesystem::directory_iterator(folder), std::filesystem::directory_iterator());
        };
        const auto before_count = count_files(archives);
        party_request(guest_root,"127.0.0.1",status.friend_code,heartbeat);
        check(count_files(archives)==before_count, "backup: unchanged heartbeat deduplicates snapshot");
        backup_wallet.set_inventory(user_id("Guest"),139,0);
        backup_wallet.set_balance(user_id("Guest"),10,0);
        party_request(guest_root,"127.0.0.1",status.friend_code,heartbeat);
        check(count_files(archives)==before_count+1 && std::filesystem::exists(previous_copy),
              "backup: reset inventory cannot overwrite earlier ownership snapshot");
        auto forged=heartbeat;forged.set("account",Json::of(user_id("Another")));
        check(party_request(guest_root,"127.0.0.1",status.friend_code,forged).at("wallet_backup").at("account").as_u64()==user_id("Guest"),
              "backup: caller cannot select another account");
        const auto saves = guest_root / "runtime/user/savedata/test";
        std::filesystem::create_directories(saves);
        { std::ofstream f(saves / "save.bin",std::ios::binary); f << "original"; }
        const auto save_copy=backup_savedata(guest_root,"before_launch");
        { std::ofstream f(saves / "save.bin",std::ios::binary); f << "updated"; }
        const auto second_copy=backup_savedata(guest_root,"after_exit");
        std::ifstream old_save(backup_io_path(save_copy / "test/save.bin"),std::ios::binary);
        std::string old_data((std::istreambuf_iterator<char>(old_save)),{});
        check(old_data=="original" && save_copy!=second_copy && std::filesystem::exists(backup_io_path(second_copy / "test/save.bin")),
              "backup: before and after game saves remain independent immutable copies");

        // Invalid friend code verification fails with InvalidIdentity
        const HostIdentity other_host = HostIdentity::load_or_create(dir / "other_identity.json");
        const VerifyResult v_wrong = verify_candidate_endpoint("127.0.0.1:8780", other_host.friend_code(), 2000);
        check(v_wrong.status == VerifyStatus::InvalidIdentity, "verification against different friend code rejected");

        // Malformed / missing nonces rejected with 400
        std::string err_resp;
        check(http_get("127.0.0.1", 8780, "/ctr/v1/identity", err_resp) == 400, "identity endpoint rejects missing nonce");
        check(http_get("127.0.0.1", 8780, "/ctr/v1/identity?nonce=tooshort", err_resp) == 400, "identity endpoint rejects short nonce");
        check(http_get("127.0.0.1", 8780, "/ctr/v1/identity?nonce=not_hex_0123456789abcdef01234567", err_resp) == 400, "identity endpoint rejects non-hex nonce");

        Host second;
        const HostStatus busy = second.start(root, "192.0.2.10", false);
        check(!busy.running && !busy.error.empty(), "second host on the same ports refused with a reason");
        host.stop();
        check(http_get("127.0.0.1", 8780, "/ctr/v1/config", body, 1000) == 0, "ports released when hosting stops");

        const VerifyResult v_offline = verify_candidate_endpoint("127.0.0.1:8780", status.friend_code, 500);
        check(v_offline.status == VerifyStatus::NoResponse, "offline endpoint returns NoResponse");

        const HostStatus again = host.start(root, "192.0.2.10", false);
        check(again.running, "hosting starts again after a stop");
        host.stop();
    }

    // Two actual HTTP services: invite delivery, acceptance and identical game selector.
    {
        const auto aroot=dir/"party_net_a", broot=dir/"party_net_b";
        const auto acode=local_friend_code(aroot), bcode=local_friend_code(broot);
        FriendStore(aroot/"runtime/user/online/friends.json").add_or_update(bcode,"Bob");
        FriendStore(broot/"runtime/user/online/friends.json").add_or_update(acode,"Alice");
        ServerConfig ac;ac.host="127.0.0.1";ac.public_host="127.0.0.1";ac.party_enabled=true;
        ac.state_dir=aroot/"runtime/user/online";ac.log_path=aroot/"runtime/user/logs/party.jsonl";
        ServerConfig bc=ac;bc.host="127.0.0.2";bc.public_host="127.0.0.2";
        bc.state_dir=broot/"runtime/user/online";bc.log_path=broot/"runtime/user/logs/party.jsonl";
        Server aserver(ac), bserver(bc);aserver.start();bserver.start();
        auto req=[](const char* action){auto j=Json::object();j.set("action",Json::of(action));return j;};
        auto configure=req("configure");configure.set("name",Json::of("Alice"));
        const auto ap=party_request(aroot,"127.0.0.1",acode,configure);
        configure.set("name",Json::of("Bob"));party_request(broot,"127.0.0.2",bcode,configure);
        auto invitation=req("invite");invitation.set("target",Json::of(bcode));
        party_request(aroot,"127.0.0.1",acode,invitation);
        auto notify=req("notify");notify.set("owner",Json::of(acode));
        party_request(aroot,"127.0.0.2",bcode,notify);
        const auto inbox=party_request(broot,"127.0.0.2",bcode,req("status"));
        check(inbox.at("invitations").items.size()==1 && inbox.at("invitations").items[0].at("from").text==acode,
              "two HTTP servers: signed invitation delivered to friend inbox");
        auto join=req("join");join.set("name",Json::of("Bob"));
        const auto bp=party_request(broot,"127.0.0.1",acode,join);
        auto destination=req("destination");destination.set("owner",Json::of(acode));
        const auto moved=party_request(broot,"127.0.0.2",bcode,destination);
        check(bp.at("room").text==ap.at("room").text && moved.at("owner").text==acode && moved.at("invitations").items.empty(),
              "accept invite: same retail room, presence redirects to owner, invitation consumed");
        party_request(broot,"127.0.0.1",acode,req("leave"));
        check(party_request(aroot,"127.0.0.1",acode,req("status")).at("count").as_u64()==1,
              "two HTTP servers: leaving frees membership immediately");
        aserver.stop();bserver.stop();
    }

    // Friend identity: generation, persistence, friend code encoding and Crockford Base32 error tolerance
    {
        const std::filesystem::path id_path = dir / "friend_id.json";
        const HostIdentity ident1 = HostIdentity::load_or_create(id_path);
        check(ident1.public_key().size() == 64, "ECDSA P-256 public key is 64 bytes uncompressed (X, Y)");
        check(ident1.dht_info_hash().size() == 20, "DHT infohash is 20 bytes SHA-1");
        check(ident1.friend_code().rfind("CTR1-", 0) == 0, "friend code starts with CTR1- prefix");

        const HostIdentity ident2 = HostIdentity::load_or_create(id_path);
        check(ident1.public_key() == ident2.public_key() && ident1.friend_code() == ident2.friend_code() &&
              ident1.dht_info_hash() == ident2.dht_info_hash(), "host identity persists across loads");

        // Code parsing and validation
        FriendCodeInfo info;
        check(parse_friend_code(ident1.friend_code(), info) == CodeParseError::Ok && info.public_key == ident1.public_key(),
              "valid friend code parses correctly");

        // Case insensitivity and whitespace / hyphen tolerance
        std::string lower_code = ident1.friend_code();
        for (char& c : lower_code) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        FriendCodeInfo info_lower;
        check(parse_friend_code(lower_code, info_lower) == CodeParseError::Ok && info_lower.public_key == ident1.public_key(),
              "friend code parsing is case-insensitive");

        std::string spaced_code = ident1.friend_code();
        spaced_code.insert(10, "  ");
        spaced_code.erase(std::remove(spaced_code.begin(), spaced_code.end(), '-'), spaced_code.end());
        spaced_code = "CTR1-" + spaced_code.substr(4);
        FriendCodeInfo info_spaced;
        check(parse_friend_code(spaced_code, info_spaced) == CodeParseError::Ok && info_spaced.public_key == ident1.public_key(),
              "friend code parsing ignores formatting whitespace/hyphens");

        // Corrupted code checksum
        std::string corrupt_code = ident1.friend_code();
        std::size_t target_idx = corrupt_code.size() - 2;
        while (target_idx > 4 && corrupt_code[target_idx] == '-') {
            --target_idx;
        }
        corrupt_code[target_idx] = (corrupt_code[target_idx] == '0') ? '1' : '0';
        check(parse_friend_code(corrupt_code, info) == CodeParseError::ChecksumMismatch, "corrupted friend code caught by checksum");

        // Bad prefix, bad length, invalid characters
        check(parse_friend_code("CTR2" + ident1.friend_code().substr(4), info) == CodeParseError::InvalidPrefix, "bad prefix rejected");
        check(parse_friend_code(ident1.friend_code().substr(0, 30), info) == CodeParseError::InvalidLength, "truncated code rejected");
        std::string bad_char_code = ident1.friend_code();
        bad_char_code[12] = 'U'; // 'U' is excluded from Crockford Base32
        check(parse_friend_code(bad_char_code, info) == CodeParseError::InvalidCharacters, "invalid Crockford base32 character rejected");

        // Test Crockford 'O' -> 0 mapping if '0' is in friend code
        std::string mapped_code = ident1.friend_code();
        auto zero_pos = mapped_code.find('0');
        if (zero_pos != std::string::npos) {
            mapped_code[zero_pos] = 'O';
            FriendCodeInfo info_mapped;
            check(parse_friend_code(mapped_code, info_mapped) == CodeParseError::Ok && info_mapped.public_key == ident1.public_key(),
                  "friend code Crockford Base32 maps 'O' to '0'");
        }

        // ECDSA challenge signing & signature verification
        const std::string nonce = "0123456789abcdef0123456789abcdef";
        const Bytes tk = Bytes(0x126, 0x42);
        const std::uint64_t ts = 1700000000;
        const Bytes sig = ident1.sign_challenge(nonce, tk, ts);
        check(!sig.empty() && verify_friend_response(ident1.public_key(), nonce, tk, ts, sig),
              "ECDSA P-256 challenge signature verifies against public key");

        // Tampering checks
        check(!verify_friend_response(ident1.public_key(), "ffffffffffffffffffffffffffffffff", tk, ts, sig),
              "altered nonce fails signature verification");

        check(!verify_friend_response(ident1.public_key(), nonce, tk, ts + 1, sig),
              "altered timestamp fails signature verification");

        const Bytes other_tk = Bytes(0x126, 0x43);
        check(!verify_friend_response(ident1.public_key(), nonce, other_tk, ts, sig),
              "altered traffic key fails signature verification");

        Bytes tampered_sig = sig;
        tampered_sig[10] ^= 0x55;
        check(!verify_friend_response(ident1.public_key(), nonce, tk, ts, tampered_sig),
              "altered signature byte fails verification");

        const HostIdentity other_ident = HostIdentity::load_or_create(dir / "other2.json");
        check(!verify_friend_response(other_ident.public_key(), nonce, tk, ts, sig),
              "signature fails against different public key");
    }

    {
        const auto corrupt_path = dir / "corrupt_identity.json";
        std::ofstream(corrupt_path) << "broken";
        bool refused = false;
        try { HostIdentity::load_or_create(corrupt_path); } catch (const std::exception&) { refused = true; }
        std::ifstream in(corrupt_path);
        std::string unchanged; in >> unchanged;
        check(refused && unchanged == "broken", "corrupt identity refused without replacing existing friend code");
    }
    // Launcher lobby: signed identities, membership cap, invitations and room admission.
    {
        const auto owner=HostIdentity::load_or_create(dir/"party_owner.json");
        PartyLobby party(owner.friend_code());
        const auto now=party_now();
        auto request=[](const char* action,const std::string& name="") {
            Json req=Json::object();req.set("action",Json::of(action));req.set("name",Json::of(name));return req;
        };
        const Json configured=party.apply(owner.friend_code(),request("configure","Owner"),false,now);
        const auto room=configured.at("room").text;
        check(room.size()==64 && party.admits(room,"Owner") && !party.admits(room,"Unknown"),"party room admits only its registered names");
        std::vector<std::string> codes;
        for(int i=0;i<8;++i) codes.push_back(HostIdentity::load_or_create(dir/("party_guest"+std::to_string(i)+".json")).friend_code());
        bool refused=false;
        try {party.apply(codes[0],request("join","Guest0"),false,now);} catch (...) {refused=true;}
        check(refused,"unlisted uninvited identity cannot join party");
        for(int i=0;i<7;++i) party.apply(codes[i],request("join","Guest"+std::to_string(i)),true,now);
        check(party.expected_players().at(room)==8,"party holds exactly eight including owner");
        refused=false;
        try {party.apply(codes[7],request("join","Guest7"),true,now);} catch (...) {refused=true;}
        check(refused && party.expected_players().at(room)==8,"ninth party member rejected without modifying roster");
        party.apply(codes[0],request("leave"),true,now);
        check(!party.admits(room,"Guest0"),"leaving withdraws game admission");
        Json invite=request("invite");invite.set("target",Json::of(codes[7]));
        party.apply(owner.friend_code(),invite,false,now);
        party.apply(codes[7],request("join","Guest7"),false,now);
        check(party.admits(room,"Guest7"),"invited identity can join without sharing a password");
        refused=false;
        try {party.apply(codes[7],request("join","Owner"),true,now);} catch (...) {refused=true;}
        check(refused,"duplicate display name rejected to preserve retail account identity");
        PartyLobby inbox(codes[0]);
        Json notification=request("notify");notification.set("owner",Json::of(owner.friend_code()));
        inbox.apply(owner.friend_code(),notification,true,now);
        check(inbox.apply(codes[0],request("status"),false,now).at("invitations").items.size()==1,"authenticated invite reaches recipient inbox");
        party.apply(owner.friend_code(),request("status"),false,now+181);
        check(party.expected_players().at(room)==1,"offline guests expire and release capacity");
        Json message=request("status");message.set("nonce",Json::of("0123456789abcdef0123456789abcdef"));
        Json signed_message=sign_party_message(owner,message);
        check(verify_party_message(signed_message,owner.friend_code(),now)==message,"party signed request round trip");
        signed_message.set("payload",Json::of("{}"));refused=false;
        try {verify_party_message(signed_message,owner.friend_code(),now);}catch(...){refused=true;}
        check(refused,"party tampering rejected before mutation");
    }
    // Recovery is authorized by a fresh checkpoint signed by the previous owner.
    {
        const auto owner=HostIdentity::load_or_create(dir/"recovery_owner.json");
        const auto alice=HostIdentity::load_or_create(dir/"recovery_alice.json");
        const auto bob=HostIdentity::load_or_create(dir/"recovery_bob.json");
        const auto now=party_now();
        auto command=[](const char* action,const char* name="",const char* endpoint="") {
            Json j=Json::object();j.set("action",Json::of(action));j.set("name",Json::of(name));j.set("endpoint",Json::of(endpoint));return j;
        };
        PartyLobby source(owner.friend_code());
        source.apply(owner.friend_code(),command("configure","Owner","127.0.0.1"),false,now);
        source.apply(alice.friend_code(),command("join","Alice","127.0.0.2"),true,now);
        const auto joined=source.apply(bob.friend_code(),command("join","Bob","127.0.0.3"),true,now);
        const auto room=joined.at("room").text;
        const auto payload=source.recovery_checkpoint(alice.friend_code());
        const auto recovery_ticket=sign_party_message(owner,payload);
        const auto plan=verify_party_recovery(recovery_ticket,owner.friend_code(),room,now);
        const auto& successor=user_id("Alice")<user_id("Bob") ? alice : bob;
        const auto& other=user_id("Alice")<user_id("Bob") ? bob : alice;
        check(plan.successor==successor.friend_code() && plan.members.size()==2,"signed checkpoint deterministically elects one surviving member");
        auto destination=command("destination");destination.set("owner",Json::of(owner.friend_code()));destination.set("room",Json::of(room));
        PartyLobby target(successor.friend_code());target.apply(successor.friend_code(),destination,false,now);
        auto restore=command("restore");restore.set("ticket",recovery_ticket);
        auto rejects=[&](const Json& value,std::uint64_t at,const char* description) {
            bool refused=false;try{target.apply(successor.friend_code(),value,false,at);}catch(...){refused=true;}
            check(refused && !target.admits(room,"Alice"),description);
        };
        auto bad=restore;auto forged=recovery_ticket;forged.set("payload",Json::of("{}"));bad.set("ticket",forged);
        rejects(bad,now,"tampered checkpoint cannot mutate destination lobby");
        bad=restore;bad.set("ticket",sign_party_message(other,payload));
        rejects(bad,now,"checkpoint from another signer rejected");
        auto wrong_room=payload;wrong_room.set("room",Json::of(std::string(64,'a')));
        bad=restore;bad.set("ticket",sign_party_message(owner,wrong_room));
        rejects(bad,now,"checkpoint cannot replace a different selected room");
        rejects(restore,now+61,"expired checkpoint cannot restore membership");
        PartyLobby noncandidate(other.friend_code());noncandidate.apply(other.friend_code(),destination,false,now);
        bool refused=false;try{noncandidate.apply(other.friend_code(),restore,false,now);}catch(...){refused=true;}
        check(refused,"noncandidate cannot claim host role from same checkpoint");
        const auto restored=target.apply(successor.friend_code(),restore,false,now);
        check(restored.at("room").text==room && restored.at("owner").text==successor.friend_code() &&
              target.admits(room,"Alice") && target.admits(room,"Bob") && !target.admits(room,"Owner"),
              "successor restores same room and all surviving members without departed host");
        check(target.recovery_checkpoint(successor.friend_code()).at("generation").as_u64()==plan.generation+1,
              "restored room advances generation");
        refused=false;try{target.apply(successor.friend_code(),restore,false,now);}catch(...){refused=true;}
        check(refused && target.expected_players().at(room)==2,"checkpoint replay cannot reset an already restored room");
        refused=false;try{source.recovery_checkpoint(std::string("outsider"));}catch(...){refused=true;}
        check(refused,"checkpoint is not exposed to outsiders");
        auto duplicate=payload;duplicate.find("members")->items[1].set("name",duplicate.at("members").items[0].at("name"));
        refused=false;try{verify_party_recovery(sign_party_message(owner,duplicate),owner.friend_code(),room,now);}catch(...){refused=true;}
        check(refused,"duplicate account names in signed checkpoint rejected");
    }
    // Friend store (address book)
    {
        const std::filesystem::path book_path = dir / "friends.json";
        FriendStore store(book_path);
        const HostIdentity bob_id = HostIdentity::load_or_create(dir / "bob.json");
        std::string err;
        check(store.add_or_update(bob_id.friend_code(), "Bob", &err), "valid friend added to store");
        check(!store.add_or_update("CTR1-INVALID", "Bad", &err) && !err.empty(), "invalid friend code rejected by store");
        check(store.entries().size() == 1 && store.entries()[0].nickname == "Bob", "friend store has Bob");

        store.update_endpoint(bob_id.friend_code(), "198.51.100.5:8780", "verified");
        FriendStore reloaded(book_path);
        check(reloaded.entries().size() == 1 && reloaded.entries()[0].last_endpoint == "198.51.100.5:8780" &&
              reloaded.entries()[0].last_status == "verified", "friend store persists updates across reloads");

        check(reloaded.entries()[0].last_seen > 0, "friend last_seen survives integer JSON reload");
        check(reloaded.add_or_update(bob_id.friend_code(), "Robert", &err), "friend rename saves without manual mutex unlock");
        check(FriendStore(book_path).entries()[0].nickname == "Robert", "renamed friend persisted");
        reloaded.remove(bob_id.friend_code());
        check(reloaded.entries().empty(), "friend removed from store");
        FriendStore reloaded2(book_path);
        check(reloaded2.entries().empty(), "friend removal persists");
    }

    // DHT Service isolated single-node lifecycle
    {
        const std::filesystem::path dht_cache = dir / "dht_cache.dat";
        DhtService dht;
        check(dht.start(8792, dht_cache, {"127.0.0.1:1"}), "DhtService starts on UDP port");
        check(dht.is_running(), "DhtService reports running");
        DhtService second;
        check(!second.start(0, {}, {"127.0.0.1:1"}), "second process-global DHT instance rejected without corrupting first");
        check(dht.is_running(), "first DHT survives rejected second instance");
        dht.stop();
        check(!dht.is_running(), "DhtService reports stopped");
    }

    test_dht_discovery();

    check(resolve_ipv4("127.0.0.1") == "127.0.0.1" && resolve_ipv4("localhost") == "127.0.0.1" && resolve_ipv4("no such host.invalid").empty(),
          "server names resolved to IPv4");
    check(ipv4_scope("192.168.1.5") == "private" && ipv4_scope("100.70.1.1") == "cgnat" && ipv4_scope("203.0.113.9") == "public",
          "address scope classification");
    std::filesystem::remove_all(backup_io_path(dir));
    std::printf("{\"scope\": \"online_server_test\", \"passed\": %s, \"failures\": %d}\n", failures ? "false" : "true", failures);
    return failures ? 1 : 0;
}

int main(int argc, char* argv[]) {
    try { return run_tests(argc, argv); }
    catch (const std::exception& error) {
        std::fprintf(stderr, "online_server_test exception: %s\n", error.what());
        return 1;
    }
}
