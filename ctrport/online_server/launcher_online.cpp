// SPDX-License-Identifier: GPL-2.0-or-later
#include "launcher_online.hpp"
#include "hub_election.hpp"
#include "local_backup.hpp"
#include "protocol.hpp"
#include "server.hpp"
#include "dht_service.hpp"
#include "friend_identity.hpp"
#include "friend_store.hpp"
#include "party_lobby.hpp"
#include "party_migration.hpp"
#include "crypto.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

namespace ctr_online {
namespace fs = std::filesystem;
namespace {
struct Wsa {
    Wsa() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~Wsa() { WSACleanup(); }
};
} // namespace

std::vector<PortRequest> host_ports(std::uint16_t http_port, std::uint16_t discovery_port, std::uint16_t game_udp_port) {
    return {{"TCP", http_port, "server HTTP"},
            {"UDP", game_udp_port, "game"},
            {"UDP", discovery_port, "discovery"},
            {"UDP", static_cast<std::uint16_t>(discovery_port + 1), "discovery"},
            {"UDP", static_cast<std::uint16_t>(discovery_port + 2), "discovery"},
            {"UDP", static_cast<std::uint16_t>(discovery_port + 3), "discovery"},
            {"UDP", 3082, "relay"},
            {"UDP", 8788, "dht"}};
}

Host::Host() = default;
Host::~Host() { stop(); }

HostStatus Host::start(const std::filesystem::path& root, const std::string& manual_public_ip, bool use_upnp,
                       const std::string& local_name) {
    stop();
    HostStatus status;
    status.lan_ip = local_lan_address();
    auto mapper = std::make_unique<PortMapper>();
    if (use_upnp) {
        status.upnp_router = mapper->open(host_ports(), "CTR Native Port");
        status.ports = mapper->results();
        if (!status.upnp_router) status.warnings.push_back("UPnP: " + mapper->error());
        for (const PortResult& port : status.ports)
            if (!port.ok) status.warnings.push_back("port " + port.request.protocol + " " + std::to_string(port.request.port) + " not opened: " + port.error);
    }
    if (!manual_public_ip.empty()) {
        status.public_ip = manual_public_ip;
        status.address_source = "manual";
    } else if (status.upnp_router && ipv4_scope(mapper->external_ip()) == "public") {
        status.public_ip = mapper->external_ip();
        status.address_source = "upnp";
    } else {
        if (status.upnp_router)
            status.warnings.push_back("the router reports address " + mapper->external_ip() + " (" + ipv4_scope(mapper->external_ip()) +
                                      "): the connection is behind another NAT (such as CGNAT) and cannot be reached from the internet");
        status.public_ip = status.lan_ip.empty() ? "127.0.0.1" : status.lan_ip;
        status.address_source = "lan";
    }
    ServerConfig config;
    config.public_host = status.public_ip;
    config.party_enabled = true;
    config.state_dir = root / "runtime" / "user" / "online";
    config.log_path = default_log_path(root);
    config.identity_path = root / "runtime" / "user" / "identity" / "host_identity.json";
    config.friend_network = true;
    config.local_name = local_name;
    config.match_hub = [this] { return match_hub(); };
    config.hub_reachable = status.address_source != "lan";
    config.await_match_hub = [this](std::chrono::milliseconds budget) { return hub_gate_.acquire(budget); };
    // A friend's service: its last verified endpoint, else a DHT lookup (at most once per five seconds per
    // friend, so offline friends do not keep the DHT busy). A found endpoint is saved in the rubrica.
    const fs::path friends_path = config.state_dir / "friends.json";
    auto attempts = std::make_shared<std::pair<std::mutex, std::map<std::string, std::uint64_t>>>();
    config.friend_resolver = [this, root, friends_path, attempts](const std::string& code)
        -> std::optional<std::pair<std::string, std::uint16_t>> {
        for (const auto& entry : FriendStore(friends_path).entries()) {
            if (entry.friend_code != code || entry.last_endpoint.empty()) continue;
            const auto known = verify_candidate_endpoint(entry.last_endpoint, code, 1000);
            if (known.status == VerifyStatus::Verified) return std::make_pair(known.ip, known.port);
        }
        DhtService* shared = dht();
        if (!shared || !shared->is_running()) return std::nullopt;
        {
            std::lock_guard<std::mutex> lock(attempts->first);
            auto& last = attempts->second[code];
            const auto now = party_now();
            if (last && now - last < 5) return std::nullopt;
            last = now;
        }
        const auto found = find_friend_server(root, code, 1500, 1500, {}, shared);
        if (found.status != VerifyStatus::Verified) return std::nullopt;
        FriendStore(friends_path).update_endpoint(code, found.ip + ":" + std::to_string(found.port), "Verified service");
        return std::make_pair(found.ip, found.port);
    };
    status.log = config.log_path;
    auto server = std::make_unique<Server>(config);
    auto dht = std::make_unique<DhtService>();
    try {
        const std::vector<std::string> listening = server->start();
        status.listen_failures = server->failures();
        status.traffic_key = server->traffic_key_path();
        status.friend_code = server->friend_code();
        bool http = false, lobby = false;
        for (const std::string& name : listening) {
            if (name.rfind("http/", 0) == 0) http = true;
            if (name == "lobby/tcp/3074") lobby = true;
        }
        if (!http || !lobby) {
            status.error = "server ports are busy (is another online server already running?)";
            for (const std::string& failure : status.listen_failures) status.error += "\n" + failure;
            server->stop();
            mapper->close();
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = status;
            return status;
        }
        // Start DHT service and publish announce on public infohash
        const fs::path dht_cache = root / "runtime" / "user" / "online" / "dht_nodes.dat";
        if (dht->start(8788, dht_cache)) {
            if (const auto* ident = server->host_identity()) {
                dht->set_announce(ident->dht_info_hash(), 8780);
            }
            // Only a service the internet can reach may host public matchmaking for others.
            if (status.address_source != "lan") dht->add_announce(public_matchmaking_key(), 8780);
        }
        status.running = true;
    } catch (const std::exception& error) {
        status.error = error.what();
        mapper->close();
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = status;
        return status;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server_ = std::move(server);
        mapper_ = std::move(mapper);
        dht_ = std::move(dht);
        status_ = status;
    }
    hub_gate_.reset(status.address_source != "lan" ?
        std::optional<MatchHub>(MatchHub{status.friend_code,status.public_ip,8780}) : std::nullopt);
    hub_thread_ = std::thread([this, root, self = MatchHub{status.friend_code, status.public_ip, 8780},
                               reachable = status.address_source != "lan"] { elect_hubs(root, self, reachable); });
    return status;
}

Bytes public_matchmaking_key() {
    const Bytes digest = sha256(bytes_of("ctr-public-matchmaking-v1"));
    return Bytes(digest.begin(), digest.begin() + 20);
}

std::optional<MatchHub> choose_hub(const MatchHub& self, bool self_reachable, const std::vector<MatchHub>& verified) {
    std::optional<MatchHub> best;
    if (self_reachable) best = self;
    for (const MatchHub& candidate : verified)
        if (candidate.code != self.code && (!best || candidate.code < best->code)) best = candidate;
    return best;
}

std::optional<MatchHub> Host::match_hub() const { return hub_gate_.current(); }

void Host::elect_hubs(std::filesystem::path root, MatchHub self, bool self_reachable) {
    HubElection election(self,self_reachable);
    const auto path=root/"runtime/user/online/hub_candidates.json";
    election.load(path,party_now());
    HubEvents events;
    const auto record=[this](const char* event,Json fields){
        std::lock_guard lock(mutex_);if(server_)server_->record_event(event,std::move(fields));
    };
    const auto publish=[&](std::optional<MatchHub> chosen,const std::string& source,std::size_t count){
        events.observe(election,chosen,source,count,record);
        auto ttl=std::chrono::seconds(HubElection::max_age);
        if(chosen && chosen->code!=self.code){
            const auto verified=election.candidates.at(chosen->code).verified;
            const auto now=party_now();
            ttl=std::chrono::seconds(HubElection::recent(verified,now)?HubElection::max_age-(now-verified):0);
        }
        if(!hub_gate_.stopping())hub_gate_.publish(chosen,ttl);
    };
    while(!hub_gate_.stopping()){
        const auto started=std::chrono::steady_clock::now();
        election.round(party_now(),[&]{
            if(hub_gate_.stopping())return std::vector<std::string>{};
            if(auto shared=dht())return shared->search(public_matchmaking_key(),6000);
            return std::vector<std::string>{};
        },[&](const std::string& endpoint)->std::optional<MatchHub>{
            if(hub_gate_.stopping())return std::nullopt;
            std::string code;const auto result=identify_endpoint(endpoint,code,1500);
            if(result.status!=VerifyStatus::Verified)return std::nullopt;
            return MatchHub{code,result.ip,result.port};
        },publish,[]{return party_now();});
        if(!hub_gate_.stopping() && !election.save(path)){
            Json f=Json::object();f.set("reason",Json::of("candidate_cache_write_failed"));record("hub_cache_error",std::move(f));
        }
        const auto interval=match_hub()?std::chrono::seconds(60):std::chrono::seconds(5);
        // At least one second between rounds; demands during an active round are coalesced.
        const auto next=std::max(started+interval,std::chrono::steady_clock::now()+std::chrono::seconds(1));
        if(!hub_gate_.wait_until(next))return;
    }
}

void Host::stop() {
    hub_gate_.stop();
    if (hub_thread_.joinable()) hub_thread_.join();
    std::unique_ptr<Server> server;
    std::unique_ptr<PortMapper> mapper;
    std::unique_ptr<DhtService> dht;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server = std::move(server_);
        mapper = std::move(mapper_);
        dht = std::move(dht_);
        status_.running = false;
    }
    // The server first: its friend watcher may be using the DHT for a lookup.
    if (server) {
        server->stop();
    }
    if (dht) {
        dht->clear_announce();
        dht->stop();
    }
    if (mapper) mapper->close();
}

bool Host::running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_ != nullptr;
}
HostStatus Host::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}
std::size_t Host::players() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_ ? server_->connected_players() : 0;
}
DhtService* Host::dht() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dht_.get();
}
std::vector<FriendState> Host::friends() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_ ? server_->friends() : std::vector<FriendState>{};
}

std::string local_friend_code(const std::filesystem::path& root) {
    const std::filesystem::path path = root / "runtime" / "user" / "identity" / "host_identity.json";
    try {
        return HostIdentity::load_or_create(path).friend_code();
    } catch (...) {
        return {};
    }
}

VerifyResult find_friend_server(const std::filesystem::path& root,
                                const std::string& friend_code,
                                int dht_timeout_ms,
                                int verify_timeout_ms,
                                const std::vector<std::string>& extra_bootstrap_nodes, DhtService* shared_dht) {
    VerifyResult result;
    FriendCodeInfo info;
    const CodeParseError parse_err = parse_friend_code(friend_code, info);
    if (parse_err != CodeParseError::Ok) {
        result.status = VerifyStatus::InvalidIdentity;
        result.detail = code_parse_error_string(parse_err);
        return result;
    }

    const std::filesystem::path dht_cache = root / "runtime" / "user" / "online" / "dht_nodes.dat";
    const std::filesystem::path friends_path = root / "runtime" / "user" / "online" / "friends.json";
    FriendStore store(friends_path);

    // Collect candidate endpoints: first try cached endpoint if available, then DHT
    std::vector<std::string> candidates;
    for (const auto& entry : store.entries()) {
        if (entry.friend_code == info.code && !entry.last_endpoint.empty()) {
            auto cached=verify_candidate_endpoint(entry.last_endpoint,info,verify_timeout_ms);
            if(cached.status==VerifyStatus::Verified) return cached;
            candidates.push_back(entry.last_endpoint);
            break;
        }
    }

    // Perform DHT search
    DhtService dht;
    DhtStats lookup_stats;
    const bool dht_started = shared_dht ? shared_dht->is_running() : dht.start(0, dht_cache, extra_bootstrap_nodes);
    if (dht_started) {
        const Bytes info_hash = friend_dht_info_hash(info.public_key);
        const std::vector<std::string> dht_results = (shared_dht ? shared_dht : &dht)->search(info_hash, dht_timeout_ms);
        lookup_stats = (shared_dht ? shared_dht : &dht)->stats();
        for (const std::string& ep : dht_results) {
            if (std::find(candidates.begin(), candidates.end(), ep) == candidates.end()) {
                candidates.push_back(ep);
            }
        }
        dht.stop();
    }

    if (candidates.empty()) {
        result.status = VerifyStatus::NoResponse;
        if (!dht_started) {
            result.detail = "Local DHT not started: cannot look up the friend. Try again or use the IP address.";
        } else if (lookup_stats.good_nodes + lookup_stats.dubious_nodes == 0) {
            result.detail = "Nessun nodo DHT raggiunto entro " + std::to_string(dht_timeout_ms / 1000) +
                " s: bootstrap incomplete or UDP network unavailable. The friend's status is unknown.";
        } else {
            result.detail = "Friend not found in the DHT within " + std::to_string(dht_timeout_ms / 1000) +
                " s (nodi attivi: " + std::to_string(lookup_stats.good_nodes) +
                ", altri nodi noti: " + std::to_string(lookup_stats.dubious_nodes) +
                "). Check that the friend's launcher is online and try again: its announcement may not be available yet.";
        }
        store.update_status(info.code, result.detail);
        return result;
    }

    // Verify candidates
    for (const std::string& candidate : candidates) {
        VerifyResult v = verify_candidate_endpoint(candidate, info, verify_timeout_ms);
        if (v.status == VerifyStatus::Verified) {
            store.update_endpoint(info.code, v.ip + ":" + std::to_string(v.port), "Verified service");
            return v;
        } else if (v.status == VerifyStatus::InvalidIdentity) {
            result = v;
            store.update_endpoint(info.code, "", "Invalid identity");
            continue; // Reject this candidate, but verify the remaining DHT results.
        }
    }

    if (result.status == VerifyStatus::InvalidIdentity) return result;
    result.status = VerifyStatus::NoResponse;
    result.detail = "Candidates contacted but none responded or verified successfully";
    store.update_status(info.code, "Nessuna risposta dai candidati");
    return result;
}

std::string resolve_ipv4(const std::string& host) {
    if (host.empty()) return {};
    Wsa wsa;
    in_addr literal{};
    if (inet_pton(AF_INET, host.c_str(), &literal) == 1) return host;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) return {};
    char text[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr, text, sizeof(text));
    freeaddrinfo(result);
    return text;
}

int http_request(const std::string& ipv4, std::uint16_t port, const std::string& path, const std::string& payload, std::string& body, int timeout_ms) {
    Wsa wsa;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ipv4.c_str(), &address.sin_addr) != 1) return 0;
    const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
    connect(s, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    fd_set writable, failed;
    FD_ZERO(&writable); FD_SET(s, &writable);
    FD_ZERO(&failed); FD_SET(s, &failed);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (select(0, nullptr, &writable, &failed, &tv) <= 0 || !FD_ISSET(s, &writable)) { closesocket(s); return 0; }
    nonblocking = 0;
    ioctlsocket(s, FIONBIO, &nonblocking);
    const DWORD timeout = static_cast<DWORD>(timeout_ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    const std::string request = std::string(payload.empty() ? "GET " : "POST ") + path + " HTTP/1.1\r\nHost: " + ipv4 +
        "\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(payload.size()) + "\r\n\r\n" + payload;
    for (std::size_t sent=0; sent<request.size();) {
        const int n=send(s,request.data()+sent,static_cast<int>(request.size()-sent),0);
        if(n<=0){closesocket(s);return 0;} sent+=n;
    }
    std::string raw;
    char buffer[8192];
    for (int n; (n = recv(s, buffer, sizeof(buffer), 0)) > 0 && raw.size() < (1u << 20);) raw.append(buffer, static_cast<std::size_t>(n));
    closesocket(s);
    const std::size_t end = raw.find("\r\n\r\n");
    if (end == std::string::npos || raw.compare(0, 5, "HTTP/") != 0) return 0;
    body = raw.substr(end + 4);
    return std::atoi(raw.c_str() + raw.find(' ') + 1);
}
int http_get(const std::string& ip, std::uint16_t port, const std::string& path, std::string& body, int timeout_ms) {
    return http_request(ip,port,path,{},body,timeout_ms);
}
bool poll_party_invitations(const std::filesystem::path& root, const std::string& host_code,
                           DhtService* shared_dht, int lookup_timeout_ms) {
    const auto self=local_friend_code(root);
    if(host_code==self) return false;
    FriendStore friends(root / "runtime/user/online/friends.json");
    VerifyResult host;
    for(const auto& entry:friends.entries()) {
        if(entry.friend_code==host_code && !entry.last_endpoint.empty()) {
            host=verify_candidate_endpoint(entry.last_endpoint,host_code,1000);
            break;
        }
    }
    if(host.status!=VerifyStatus::Verified && lookup_timeout_ms>0)
        host=find_friend_server(root,host_code,lookup_timeout_ms,1000,{},shared_dht);
    if(host.status!=VerifyStatus::Verified) return false;
    Json request=Json::object();request.set("action",Json::of("invitations"));
    const auto response=party_request(root,host.ip,host_code,request);
    for(const auto& invite:response.at("invitations").items) {
        const auto sender=invite.at("from").text;
        if(invite.at("owner").text!=host_code) throw std::runtime_error("Invitation host does not match");
        bool trusted=false;
        for(const auto& entry:friends.entries()) if(entry.friend_code==sender) trusted=true;
        if(!trusted) continue;
        Json receive=Json::object();receive.set("action",Json::of("receive_invite"));
        receive.set("from",Json::of(sender));receive.set("owner",Json::of(host_code));
        party_request(root,"127.0.0.1",self,receive);
    }
    return true;
}
Json party_request(const std::filesystem::path& root, const std::string& ip, const std::string& code, Json request) {
    const auto identity=HostIdentity::load_or_create(root / "runtime/user/identity/host_identity.json");
    const auto nonce=hex(random_bytes(16));
    request.set("nonce",Json::of(nonce));
    std::string body;
    const int status=http_request(ip,8780,"/ctr/v1/party",dump_json(sign_party_message(identity,request)),body,3000);
    if(status!=200) {
        std::string reason="Lobby host unreachable";
        try { reason=parse_json(body).at("error").text; } catch (...) {}
        throw std::runtime_error(reason);
    }
    Json response=verify_party_message(parse_json(body),code,party_now());
    if(response.at("nonce").text!=nonce) throw std::runtime_error("Lobby reply does not match");
    const auto action = request.at("action").text;
    if (action == "heartbeat" || action == "configure" || action == "join") {
        // Store the authenticated response, partitioned by host and account. No
        // automatic merging: balances from two hosts are not additive progress.
        try {
            const auto* wallet = response.find("wallet_backup");
            if (!wallet) throw std::runtime_error("Host needs an update: remote Pit Stop backup unavailable");
            if (wallet->at("format").text != "ctr-marketplace-account-v1" ||
                !wallet->at("state").is_string() || wallet->at("state").text.size() > 8 * 1024 * 1024 ||
                wallet->at("account").as_u64() != player_account(code, response.at("backup_name").text))
                throw std::runtime_error("Invalid wallet backup response");
            const auto folder = root / "runtime/user/online/backups/hosts" /
                hex(sha256(bytes_of(code))) / std::to_string(wallet->at("account").as_u64());
            backup_blob(folder, body, dump_json(*wallet));
        } catch (const std::exception& error) {
            response.set("backup_error", Json::of(error.what()));
        }
    }
    return response;
}
PartySelection recover_party(const std::filesystem::path& root, const PartySelection& previous, const std::string& local_ip) {
    const auto* ticket=previous.snapshot.find("migration_ticket");
    if(!ticket)throw std::runtime_error("The host provided no migration checkpoint");
    const auto plan=verify_party_recovery(*ticket,previous.owner,previous.room,party_now());
    const auto self=local_friend_code(root);
    if(std::none_of(plan.members.begin(),plan.members.end(),[&](const auto& m){return m.code==self;}))
        throw std::runtime_error("This client is not in the room checkpoint");
    const auto candidate=std::find_if(plan.members.begin(),plan.members.end(),[&](const auto& m){return m.code==plan.successor;});
    const auto ip=self==plan.successor ? local_ip : candidate->endpoint;
    auto request=Json::object();
    if(self==plan.successor) {
        auto query=Json::object();query.set("action",Json::of("status"));
        const auto current=party_request(root,ip,self,query);
        const auto* current_room=current.find("room");
        if(current.at("owner").text==self && current_room && current_room->text==previous.room)
            request.set("action",Json::of("heartbeat")); // recover a lost restore response
        else {request.set("action",Json::of("restore"));request.set("ticket",*ticket);}
    } else request.set("action",Json::of("heartbeat"));
    auto snapshot=party_request(root,ip,plan.successor,request);
    if(snapshot.at("room").text!=previous.room || snapshot.at("owner").text!=plan.successor)
        throw std::runtime_error("The successor did not restore this room");
    if(self!=plan.successor) {
        auto destination=Json::object();destination.set("action",Json::of("destination"));
        destination.set("owner",Json::of(plan.successor));destination.set("room",Json::of(previous.room));
        party_request(root,local_ip,self,destination);
    }
    return {ip,plan.successor,previous.room,std::move(snapshot)};
}
} // namespace ctr_online
