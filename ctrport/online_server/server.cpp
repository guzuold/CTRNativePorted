// SPDX-License-Identifier: GPL-2.0-or-later
#include "server.hpp"
#include "crypto.hpp"
#include "json.hpp"
#include "protocol.hpp"
#include "relay.hpp"
#include "stores.hpp"
#include "friend_identity.hpp"
#include "friend_store.hpp"
#include "party_lobby.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#include <windows.h>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#pragma comment(lib, "ws2_32.lib")

namespace ctr_online {
namespace fs = std::filesystem;
namespace {
// --- text helpers ---------------------------------------------------------------------
std::string latin1_to_utf8(const std::string& raw) {
    std::string out;
    for (const unsigned char c : raw) {
        if (c < 0x80) out += static_cast<char>(c);
        else { out += static_cast<char>(0xC0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 0x3F)); }
    }
    return out;
}
// First `count` code points of valid UTF-8 text (Python str slicing).
std::string utf8_prefix(const std::string& text, std::size_t count, bool* cut = nullptr) {
    std::size_t i = 0, points = 0;
    while (i < text.size() && points < count) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        i += c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        ++points;
    }
    if (cut) *cut = i < text.size();
    return text.substr(0, std::min(i, text.size()));
}
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r\n\f\v");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n\f\v") - a + 1);
}
bool starts_with(const std::string& s, const std::string& prefix) { return s.compare(0, prefix.size(), prefix) == 0; }
std::string percent_decode(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '+') out += ' ';
        else if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
                 std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
            out += static_cast<char>(std::stoi(text.substr(i + 1, 2), nullptr, 16));
            i += 2;
        } else out += text[i];
    }
    return utf8_replace(out);
}
// urllib.parse.parse_qs(...).get(name, [fallback])[0]
std::string query_value(const std::string& query, const std::string& name, const std::string& fallback) {
    std::size_t start = 0;
    while (start <= query.size()) {
        const std::size_t end = std::min(query.find('&', start), query.size());
        const std::string item = query.substr(start, end - start);
        const std::size_t eq = item.find('=');
        if (eq != std::string::npos && eq + 1 < item.size() && percent_decode(item.substr(0, eq)) == name)
            return percent_decode(item.substr(eq + 1));
        start = end + 1;
    }
    return fallback;
}
// urllib.parse.parse_qs(...).get(name, []): every nonblank value of a name.
std::vector<std::string> query_values(const std::string& query, const std::string& name) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= query.size()) {
        const std::size_t end = std::min(query.find('&', start), query.size());
        const std::string item = query.substr(start, end - start);
        const std::size_t eq = item.find('=');
        if (eq != std::string::npos && eq + 1 < item.size() && percent_decode(item.substr(0, eq)) == name)
            out.push_back(percent_decode(item.substr(eq + 1)));
        start = end + 1;
    }
    return out;
}
// Python int(text) within 64 bits.
std::optional<std::int64_t> python_int(const std::string& raw) {
    const std::string text = trim(raw);
    std::size_t i = 0;
    bool negative = false;
    if (!text.empty() && (text[0] == '+' || text[0] == '-')) { negative = text[0] == '-'; i = 1; }
    if (i >= text.size()) return std::nullopt;
    std::int64_t value = 0;
    for (; i < text.size(); ++i) {
        if (text[i] == '_' && i + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[i + 1])) && i > 0 &&
            std::isdigit(static_cast<unsigned char>(text[i - 1]))) continue;
        if (!std::isdigit(static_cast<unsigned char>(text[i])) || value > (INT64_MAX - 9) / 10) return std::nullopt;
        value = value * 10 + (text[i] - '0');
    }
    return negative ? -value : value;
}
std::string now_stamp() {
    const std::time_t t = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &local);
    return buf;
}
Json pair_json(const Endpoint& e) {
    Json a = Json::array();
    a.items = {Json::of(e.ip), Json::of(static_cast<unsigned>(e.port))};
    return a;
}

// --- sockets --------------------------------------------------------------------------
std::string peer_ip(const sockaddr_in& a) {
    char buf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &a.sin_addr, buf, sizeof(buf));
    return buf;
}
Endpoint endpoint_of(const sockaddr_in& a) { return {peer_ip(a), ntohs(a.sin_port)}; }
sockaddr_in address_of(const std::string& ip, std::uint16_t port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &a.sin_addr) != 1) throw std::runtime_error("invalid IPv4 address " + ip);
    return a;
}
std::string socket_error(const char* what) { return std::string(what) + " failed (WSA " + std::to_string(WSAGetLastError()) + ")"; }
SOCKET open_socket(int type, const std::string& ip, std::uint16_t port) {
    const SOCKET s = socket(AF_INET, type, type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
    if (s == INVALID_SOCKET) throw std::runtime_error(socket_error("socket"));
    ::BOOL exclusive = TRUE;
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
    const sockaddr_in a = address_of(ip, port);
    if (bind(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a)) != 0 || (type == SOCK_STREAM && listen(s, SOMAXCONN) != 0)) {
        const std::string error = socket_error(type == SOCK_STREAM ? "bind/listen" : "bind");
        closesocket(s);
        throw std::runtime_error(error);
    }
    if (type == SOCK_DGRAM) {
        // A datagram that cannot be delivered must not end the receive loop (WSAECONNRESET).
        ::BOOL report = FALSE;
        DWORD ignored = 0;
        WSAIoctl(s, SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &ignored, nullptr, nullptr);
    }
    return s;
}
bool recv_exact(SOCKET s, Bytes& out, std::size_t size) {
    out.resize(size);
    std::size_t done = 0;
    while (done < size) {
        const int n = recv(s, reinterpret_cast<char*>(out.data() + done), static_cast<int>(std::min<std::size_t>(size - done, 1 << 20)), 0);
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}
bool send_all(SOCKET s, const Bytes& data) {
    std::size_t done = 0;
    while (done < data.size()) {
        const int n = send(s, reinterpret_cast<const char*>(data.data() + done), static_cast<int>(data.size() - done), 0);
        if (n <= 0) return false;
        done += static_cast<std::size_t>(n);
    }
    return true;
}
std::uint32_t le32(const Bytes& b) { return b[0] | b[1] << 8 | b[2] << 16 | static_cast<std::uint32_t>(b[3]) << 24; }
constexpr std::size_t kMaxFrame = 16u << 20;
constexpr std::size_t kMaxHttpHead = 64u << 10;
constexpr std::size_t kMaxHttpBody = 8u << 20;
} // namespace

struct Server::Impl {
    ServerConfig config;
    std::mutex log_mutex;
    std::ofstream log;
    std::unique_ptr<RsaKey> key;
    std::unique_ptr<HostIdentity> host_identity;
    std::unique_ptr<PartyLobby> party;
    std::mutex party_requests_mutex;
    std::map<std::string, std::uint64_t> party_nonces;
    std::vector<std::string> listening, failed;

    // State shared by connections (the reference server's module globals), under `state`.
    std::mutex state;
    std::vector<AuthSession> sessions; // auth3 logins, in order
    struct Client {
        SOCKET socket;
        std::mutex send_mutex;
        std::unique_ptr<LobbySession> crypto;
        AuthSession user;
        std::string ip;
    };
    std::map<std::uint64_t, std::shared_ptr<Client>> live_clients;
    std::map<std::uint64_t, LobbyPlan> pending_lobbies;
    std::unique_ptr<StatsStore> stats;
    std::unique_ptr<MarketplaceStore> marketplace;
    std::unique_ptr<AchievementStore> achievements;
    SessionStore session_store;
    MatchmakingStore matchmaking;
    DiscoveryLayout discovery;
    std::map<std::string, SOCKET> discovery_sockets; // advertised endpoint text -> socket
    Relay relay;
    SOCKET relay_socket = INVALID_SOCKET;
    std::mutex relay_log_mutex;
    std::set<std::string> relay_refusals_logged; // "<endpoint>/<reason>" already logged

    // Threads and sockets.
    std::atomic<bool> stopping{false};
    std::vector<SOCKET> listeners;
    std::vector<std::thread> listener_threads;
    std::mutex workers_mutex;
    struct Worker { std::thread thread; std::shared_ptr<std::atomic<bool>> done; };
    std::vector<Worker> workers;
    std::set<SOCKET> connections;
    bool wsa = false;

    // --- log ------------------------------------------------------------------------
    void record(const char* name, Json fields = Json::object()) {
        Json event = Json::object();
        Json time;
        time.kind = Json::Kind::Double;
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        time.number = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count()) / 1000.0;
        event.set("time", time);
        event.set("event", Json::of(name));
        for (auto& member : fields.members) event.set(member.first, std::move(member.second));
        Json summary = Json::object();
        for (const auto& [k, v] : event.members) {
            if (v.is_string()) {
                bool cut = false;
                const std::string head = utf8_prefix(v.text, 96, &cut);
                summary.set(k, Json::of(cut ? head + "..." : v.text));
            } else summary.set(k, v);
        }
        const std::string line = dump_json_spaced(event), brief = dump_json_spaced(summary);
        std::lock_guard<std::mutex> lock(log_mutex);
        if (log.is_open()) { log << line << '\n'; log.flush(); }
        if (config.on_event) config.on_event(brief);
    }

    // --- workers ----------------------------------------------------------------------
    void spawn(std::function<void()> body) {
        std::lock_guard<std::mutex> lock(workers_mutex);
        for (auto it = workers.begin(); it != workers.end();) {
            if (it->done->load()) { it->thread.join(); it = workers.erase(it); } else ++it;
        }
        auto done = std::make_shared<std::atomic<bool>>(false);
        workers.push_back({std::thread([body = std::move(body), done] { body(); done->store(true); }), done});
    }
    void track(SOCKET s) { std::lock_guard<std::mutex> lock(workers_mutex); connections.insert(s); }
    void release(SOCKET s) {
        { std::lock_guard<std::mutex> lock(workers_mutex); connections.erase(s); }
        closesocket(s);
    }
    void accept_loop(SOCKET listener, std::function<void(SOCKET, std::string)> handler) {
        while (!stopping) {
            sockaddr_in peer{};
            int size = sizeof(peer);
            const SOCKET s = accept(listener, reinterpret_cast<sockaddr*>(&peer), &size);
            if (s == INVALID_SOCKET) { if (stopping) return; continue; }
            const std::string name = endpoint_of(peer).text();
            track(s);
            spawn([this, s, name, handler] {
                try { handler(s, name); } catch (const std::exception& error) {
                    Json f = Json::object();
                    f.set("peer", Json::of(name));
                    f.set("error", Json::of(error.what()));
                    record("connection_error", std::move(f));
                }
                release(s);
            });
        }
    }

    // --- internet helpers -------------------------------------------------------------
    std::string reachable_address(const std::string& ip) const {
        if (!config.public_host.empty() && starts_with(ip, "127.")) return config.public_host;
        return ip;
    }
    std::set<std::string> room_addresses(const std::string& room) {
        std::set<std::string> found;
        for (const auto& [account, client] : live_clients) {
            if (client->user.room_token != room) continue;
            if (!client->ip.empty()) found.insert(client->ip);
            if (const std::string* info = matchmaking.player_info(account)) {
                try {
                    const Bytes block = base64_decode_lenient(parse_json(*info).at("listen_server").at("local_address").text);
                    if (block.size() >= 64)
                        found.insert(std::to_string(block[60]) + "." + std::to_string(block[61]) + "." +
                                     std::to_string(block[62]) + "." + std::to_string(block[63]));
                } catch (const std::exception&) {}
            }
        }
        return found;
    }

    // Players of a room online now, with the endpoints of their advertisement block
    // (local: bytes 0..5, public as seen by discovery: bytes 60..65).
    std::vector<RelayMember> relay_roster(const std::string& room) {
        std::vector<RelayMember> members;
        std::lock_guard<std::mutex> lock(state);
        for (const auto& [account, client] : live_clients) {
            if (client->user.room_token != room) continue;
            RelayMember m;
            m.account = account;
            if (const std::string* info = matchmaking.player_info(account)) {
                try {
                    const Bytes block = base64_decode_lenient(parse_json(*info).at("listen_server").at("local_address").text);
                    m.local_endpoint = block_endpoint(block, 0);
                    m.public_endpoint = block_endpoint(block, 60);
                } catch (const std::exception&) {}
            }
            members.push_back(std::move(m));
        }
        return members;
    }

    // --- HTTP -------------------------------------------------------------------------
    struct HttpAnswer {
        int status;
        std::vector<std::pair<std::string, std::string>> headers;
        std::string body;
        std::string event;
        Json fields = Json::object();
    };
    static HttpAnswer json_answer(int status, std::string body, std::string event, bool typed = true) {
        HttpAnswer a{status, {}, std::move(body), std::move(event)};
        if (typed) a.headers.emplace_back("Content-Type", "application/json");
        return a;
    }
    HttpAnswer http_answer(const std::string& method, const std::string& target,
                           const std::map<std::string, std::string>& headers, const Bytes& body,
                           const std::string& peer_address) {
        auto header = [&](const char* name) {
            const auto it = headers.find(name);
            return it == headers.end() ? std::string() : it->second;
        };
        const std::string host = lower(header("host").substr(0, header("host").find(':')));
        const std::string online_id = header("x-ctr-online-id");
        if (method == "GET" && target == "/ctr/v1/config") {
            Json payload = Json::object();
            payload.set("discovery_port", Json::of(static_cast<unsigned>(config.discovery_port)));
            payload.set("public_host", config.public_host.empty() ? Json::null() : Json::of(config.public_host));
            return json_answer(200, dump_json_spaced(payload), "config");
        }
        if (method == "GET" && starts_with(target, "/ctr/v1/peer-allowed?")) {
            const std::string ip = query_value(target.substr(target.find('?') + 1), "ip", "");
            const std::string room = header("x-ctr-room-token");
            bool allowed = false;
            {
                std::lock_guard<std::mutex> lock(state);
                const auto requester = online_id.empty() ? live_clients.end() : live_clients.find(user_id(online_id));
                allowed = requester != live_clients.end() && valid_room_token(room) &&
                          requester->second->user.room_token == room && room_addresses(room).count(ip);
            }
            Json payload = Json::object();
            payload.set("allowed", Json::of(allowed));
            HttpAnswer a = json_answer(allowed ? 200 : 403, dump_json_spaced(payload), "peer_allowed");
            a.fields.set("ip", Json::of(ip));
            a.fields.set("allowed", Json::of(allowed));
            return a;
        }
        if (method == "POST" && target == "/ctr/v1/relay-session" && relay_socket != INVALID_SOCKET) {
            // Relay token for a player online in the lobby, asked from the lobby connection's IP.
            const std::string room = header("x-ctr-room-token");
            std::uint64_t account = 0;
            std::string refusal;
            {
                std::lock_guard<std::mutex> lock(state);
                const auto requester = online_id.empty() ? live_clients.end() : live_clients.find(user_id(online_id));
                if (requester == live_clients.end()) refusal = "not online in the lobby";
                else if (!valid_room_token(room) || requester->second->user.room_token != room) refusal = "room mismatch";
                else if (requester->second->ip != reachable_address(peer_address)) refusal = "address differs from the lobby connection";
                else account = requester->first;
            }
            if (!refusal.empty()) {
                Json payload = Json::object();
                payload.set("error", Json::of(refusal));
                HttpAnswer a = json_answer(403, dump_json_spaced(payload), "relay_session_refused");
                a.fields.set("reason", Json::of(refusal));
                return a;
            }
            const Bytes token = relay.issue(account, room);
            Json payload = Json::object();
            payload.set("token", Json::of(hex(token)));
            payload.set("port", Json::of(static_cast<unsigned>(config.relay_port)));
            payload.set("lease", Json::of(static_cast<unsigned>(relay.limits().binding_timeout)));
            HttpAnswer a = json_answer(200, dump_json_spaced(payload), "relay_session");
            a.fields.set("account", Json::of(account));
            return a;
        }
        if (method == "POST" && target == "/ctr/v1/party") {
            if (!party) return json_answer(404, R"({"error":"Lobby launcher non disponibile"})", "party_unavailable");
            try {
                if (body.size()>16384) throw std::runtime_error("Messaggio troppo grande");
                const Json packet=parse_json(std::string(body.begin(),body.end()));
                const std::string caller=packet.at("code").text;
                const auto now=party_now();
                Json request=verify_party_message(packet,caller,now);
                const std::string nonce=request.at("nonce").text;
                if (nonce.size()!=32 || nonce.find_first_not_of("0123456789abcdef")!=std::string::npos)
                    throw std::runtime_error("Nonce non valido");
                {
                    std::lock_guard<std::mutex> guard(party_requests_mutex);
                    for (auto it=party_nonces.begin();it!=party_nonces.end();) {
                        if (now>it->second && now-it->second>120) it=party_nonces.erase(it); else ++it;
                    }
                    if (party_nonces.size()>=4096 || !party_nonces.emplace(caller+nonce,now).second)
                        throw std::runtime_error("Messaggio duplicato o limite richieste");
                }
                FriendStore friends(config.state_dir / "friends.json");
                bool allowed=false;
                for (const auto& entry : friends.entries()) if (entry.friend_code==caller) allowed=true;
                if(caller==host_identity->friend_code() && request.at("action").text=="configure")
                    request.set("endpoint",Json::of(config.public_host));
                Json response=party->apply(caller,request,allowed,now);
                if (const auto* name = response.find("backup_name")) {
                    std::lock_guard<std::mutex> guard(state);
                    response.set("wallet_backup", marketplace->snapshot(user_id(name->text)));
                }
                char migration[2]{};
                if(response.find("backup_name") && GetEnvironmentVariableA("CTR_EXPERIMENTAL_HOST_MIGRATION",migration,2)==1 && migration[0]=='1')
                    response.set("migration_ticket",sign_party_message(*host_identity,party->recovery_checkpoint(caller)));
                response.set("nonce",Json::of(nonce));
                return json_answer(200,dump_json(sign_party_message(*host_identity,response)),"party_"+request.at("action").text);
            } catch (const std::exception& error) {
                Json response=Json::object(); response.set("error",Json::of(error.what()));
                return json_answer(403,dump_json(response),"party_rejected");
            }
        }
        if (method == "GET" && target == "/ctr/v1/traffic-key") {
            HttpAnswer a{200, {{"Content-Type", "application/octet-stream"}}, std::string(key->spki().begin(), key->spki().end()), "traffic_key"};
            return a;
        }
        if (method == "GET" && starts_with(target, "/ctr/v1/identity")) {
            const std::string query = target.find('?') == std::string::npos ? "" : target.substr(target.find('?') + 1);
            const std::string nonce_hex = query_value(query, "nonce", "");
            if (nonce_hex.size() < 16 || nonce_hex.size() > 64 || nonce_hex.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
                return json_answer(400, R"({"error":"invalid nonce"})", "identity_rejected");
            }
            if (!host_identity) {
                return json_answer(500, R"({"error":"host identity unavailable"})", "identity_rejected");
            }
            const auto ts = static_cast<std::uint64_t>(std::time(nullptr));
            const Bytes sig = host_identity->sign_challenge(nonce_hex, key->spki(), ts);
            Json payload = Json::object();
            payload.set("protocol", Json::of("ctr-friend-auth-v1"));
            payload.set("version", Json::of(1));
            payload.set("friend_code", Json::of(host_identity->friend_code()));
            payload.set("traffic_key", Json::of(base64_encode(key->spki())));
            payload.set("traffic_key_sha256", Json::of(hex(sha256(key->spki()))));
            payload.set("signature", Json::of(hex(sig)));
            payload.set("timestamp", Json::of(ts));

            HttpAnswer a = json_answer(200, dump_json_spaced(payload), "identity_verified");
            a.fields.set("nonce", Json::of(nonce_hex));
            a.fields.set("friend_code", Json::of(host_identity->friend_code()));
            return a;
        }
        if (method == "POST" && starts_with(target, "/auth/") && host.find("-auth3.") != std::string::npos) {
            const std::string room = header("x-ctr-room-token");
            if (!valid_room_token(room)) return json_answer(400, R"({"error":"project room password required"})", "auth3_room_rejected", false);
            if (party && !party->admits(room,online_id)) return json_answer(403, R"({"error":"Join a launcher lobby first"})", "auth3_membership_rejected");
            Json request;
            try { request = parse_json(std::string(body.begin(), body.end())); } catch (const std::exception&) {
                return json_answer(400, R"({"error":"bad json"})", "auth3", false);
            }
            const auto multiplier_header = header("x-ctr-wumpa-multiplier");
            if (!multiplier_header.empty() && multiplier_header != "1" && multiplier_header != "100")
                return json_answer(400, R"({"error":"unsupported Wumpa multiplier"})", "auth3", false);
            Auth3Reply reply;
            try { reply = auth3_reply(request, online_id, *key); } catch (const std::exception&) {
                return json_answer(400, R"({"error":"bad auth3 request"})", "auth3", false);
            }
            HttpAnswer a{reply.status, {{"Content-Type", "application/json"}}, reply.body, "auth3"};
            for (const auto& h : reply.headers) a.headers.push_back(h);
            Json session_user = Json::null();
            if (reply.session) {
                reply.session->room_token = room;
                reply.session->wumpa_multiplier = multiplier_header == "100" ? 100u : 1u;
                session_user = Json::of(reply.session->user_id);
                std::lock_guard<std::mutex> lock(state);
                bool replaced = false;
                for (AuthSession& s : sessions) if (s.session_key == reply.session->session_key) { s = *reply.session; replaced = true; }
                if (!replaced) sessions.push_back(*reply.session);
            }
            a.fields.set("request", request);
            a.fields.set("reply", Json::of(reply.body));
            a.fields.set("session_user_id", session_user);
            return a;
        }
        if (method == "POST" && starts_with(target, "/v1.0/tokens/lsg/") && host.find(".umbrella.") != std::string::npos) {
            // Umbrella login reply as the client reads it (retail v01.21): umbrellaID,
            // accessToken, expires, accounts [{provider, username, accountID, authorized}].
            std::optional<AuthSession> user;
            try {
                const Json request = parse_json(std::string(body.begin(), body.end()));
                const Bytes ticket = base64_decode_lenient(request.at("ticket").text);
                std::lock_guard<std::mutex> lock(state);
                for (const AuthSession& s : sessions)
                    if (ticket.size() >= 24 && Bytes(ticket.begin(), ticket.begin() + 24) == s.session_key) { user = s; break; }
            } catch (const std::exception&) {}
            if (!user) return json_answer(401, R"({"error":"unknown ticket"})", "umbrella");
            Json account = Json::object();
            account.set("provider", Json::of("psn"));
            account.set("username", Json::of(user->online_id));
            account.set("accountID", Json::of(user->user_id));
            account.set("authorized", Json::of(true));
            Json payload = Json::object();
            payload.set("umbrellaID", Json::of(user->user_id));
            payload.set("accessToken", Json::of("ctr." + token_urlsafe(32)));
            payload.set("expires", Json::of(static_cast<std::uint64_t>(std::time(nullptr)) + 86400));
            Json accounts = Json::array();
            accounts.items.push_back(account);
            payload.set("accounts", accounts);
            HttpAnswer a = json_answer(200, dump_json_spaced(payload), "umbrella");
            a.fields.set("reply", payload);
            return a;
        }
        if (starts_with(target, "/webapi/")) {
            const std::string path = target.substr(8);
            if (method == "GET" && path.substr(0, path.find('?')) == "sdk:entitlement/v1/users/me/entitlements") {
                // Service entitlements bought on the PS Store: the project sells nothing, so the
                // list is empty. Field names from the retail libSceNpToolkit2.prx parser strings.
                if (online_id.empty()) return json_answer(403, R"({"error":{"message":"project identity required"}})", "webapi_entitlements", false);
                const std::optional<Json> payload = entitlement_page(path.find('?') == std::string::npos ? "" : path.substr(path.find('?') + 1));
                if (!payload) return json_answer(400, R"({"error":{"message":"invalid entitlement request"}})", "webapi_entitlements", false);
                HttpAnswer a = json_answer(200, dump_json_spaced(*payload), "webapi_entitlements");
                a.fields.set("reply", *payload);
                return a;
            }
            if (method == "POST" && path == "sdk:sessionInvitation/v1/sessions") {
                if (online_id.empty()) return json_answer(403, R"({"error":{"message":"project identity required"}})", "webapi_session_create", false);
                const std::uint64_t account = user_id(online_id);
                Json payload;
                try {
                    std::lock_guard<std::mutex> lock(state);
                    payload = session_store.create(account, header("content-type"), body);
                } catch (const std::exception& error) {
                    fs::path capture = config.log_path;
                    capture.replace_extension(".session_rejected.bin");
                    if (body.size() <= 2 * 1024 * 1024)
                        std::ofstream(capture, std::ios::binary).write(reinterpret_cast<const char*>(body.data()), static_cast<std::streamsize>(body.size()));
                    HttpAnswer a = json_answer(400, R"({"error":{"message":"invalid project session"}})", "webapi_session_create", false);
                    a.fields.set("reason", Json::of(error.what()));
                    a.fields.set("content_type", Json::of(header("content-type")));
                    a.fields.set("body_length", Json::of(static_cast<std::uint64_t>(body.size())));
                    a.fields.set("capture", Json::of(capture.string()));
                    return a;
                }
                HttpAnswer a = json_answer(201, dump_json_spaced(payload), "webapi_session_create");
                a.fields.set("account", Json::of(account));
                a.fields.set("reply", payload);
                return a;
            }
            // The API group contains a colon: split the query without interpreting a scheme.
            const std::string route = path.substr(0, path.find('?'));
            const std::string prefix = "sdk:userProfile/v1/users/", suffix = "/friendList";
            if (method == "GET" && route.size() > prefix.size() + suffix.size() && starts_with(route, prefix) &&
                route.compare(route.size() - suffix.size(), suffix.size(), suffix) == 0) {
                const std::string digits = route.substr(prefix.size(), route.size() - prefix.size() - suffix.size());
                if (digits.find_first_not_of("0123456789") == std::string::npos) {
                    const std::string stripped = digits.find_first_not_of('0') == std::string::npos ? "0" : digits.substr(digits.find_first_not_of('0'));
                    if (online_id.empty() || stripped != std::to_string(user_id(online_id)))
                        return json_answer(403, R"({"error":{"message":"project identity mismatch"}})", "webapi_friends", false);
                    const std::string query = path.find('?') == std::string::npos ? "" : path.substr(path.find('?') + 1);
                    const auto offset = python_int(query_value(query, "offset", "0"));
                    const auto limit = python_int(query_value(query, "limit", "500"));
                    Json payload;
                    try {
                        if (!offset || !limit) throw std::invalid_argument("invalid friend request");
                        payload = friend_page(*offset, *limit);
                    } catch (const std::exception&) {
                        return json_answer(400, R"({"error":{"message":"invalid friend request"}})", "webapi_friends", false);
                    }
                    HttpAnswer a = json_answer(200, dump_json_spaced(payload), "webapi_friends");
                    a.fields.set("account", Json::of(user_id(online_id)));
                    a.fields.set("reply", payload);
                    return a;
                }
            }
            return json_answer(404, R"({"error":{"message":"not served by the CTR project server"}})", "webapi");
        }
        return json_answer(404, R"({"error":"not served"})", "http");
    }
    void handle_http(SOCKET s, const std::string& peer) {
        std::string head;
        char c;
        while (head.size() < 4 || head.compare(head.size() - 4, 4, "\r\n\r\n") != 0) {
            if (head.size() >= kMaxHttpHead || recv(s, &c, 1, 0) != 1) return;
            head += c;
        }
        std::vector<std::string> lines;
        for (std::size_t start = 0;;) {
            const std::size_t end = head.find("\r\n", start);
            if (end == std::string::npos) { lines.push_back(head.substr(start)); break; }
            lines.push_back(head.substr(start, end - start));
            start = end + 2;
        }
        const std::string first = latin1_to_utf8(lines[0]);
        const std::size_t space = first.find(' ');
        const std::string method = first.substr(0, space);
        std::string target;
        if (space != std::string::npos) target = first.substr(space + 1, first.find(' ', space + 1) - space - 1);
        std::map<std::string, std::string> headers;
        for (std::size_t i = 1; i < lines.size(); ++i) {
            const std::size_t colon = lines[i].find(':');
            if (colon == std::string::npos) continue;
            headers[lower(trim(latin1_to_utf8(lines[i].substr(0, colon))))] = trim(latin1_to_utf8(lines[i].substr(colon + 1)));
        }
        std::int64_t length = 0;
        if (const auto it = headers.find("content-length"); it != headers.end() && !it->second.empty()) {
            const auto parsed = python_int(it->second);
            if (!parsed || *parsed < 0 || static_cast<std::uint64_t>(*parsed) > kMaxHttpBody) return;
            length = *parsed;
        }
        Bytes body;
        if (!recv_exact(s, body, static_cast<std::size_t>(length))) return;
        HttpAnswer answer = http_answer(method, target, headers, body, peer.substr(0, peer.rfind(':')));
        Json fields = Json::object();
        auto optional_header = [&](const char* name) {
            const auto it = headers.find(name);
            return it == headers.end() ? Json::null() : Json::of(it->second);
        };
        fields.set("peer", Json::of(peer));
        fields.set("method", Json::of(method));
        fields.set("host", optional_header("host"));
        fields.set("path", Json::of(target));
        fields.set("online_id", optional_header("x-ctr-online-id"));
        fields.set("status", Json::of(answer.status));
        fields.set("body", Json::of(utf8_prefix(utf8_replace(std::string(body.begin(), body.end())), 4096)));
        for (auto& member : answer.fields.members) fields.set(member.first, std::move(member.second));
        record(answer.event.c_str(), std::move(fields));
        const char* reason = answer.status == 200 ? "OK" : answer.status == 400 ? "Bad Request" : answer.status == 404 ? "Not Found" : "Status";
        std::string response = "HTTP/1.1 " + std::to_string(answer.status) + " " + reason + "\r\n";
        for (const auto& [name, value] : answer.headers) response += name + ": " + value + "\r\n";
        response += "Content-Length: " + std::to_string(answer.body.size()) + "\r\nConnection: close\r\n\r\n";
        response += answer.body;
        send_all(s, bytes_of(response));
        shutdown(s, SD_SEND);
    }

    // --- lobby ------------------------------------------------------------------------
    void send_lobby(Client& client, const Bytes& payload, unsigned payload_type) {
        std::lock_guard<std::mutex> lock(client.send_mutex);
        if (!send_all(client.socket, client.crypto->seal_record(payload, payload_type)))
            throw std::system_error(WSAGetLastError(), std::system_category(), "lobby send");
    }
    // Pushes to other players: a broken connection there must not end this one.
    void push(const std::shared_ptr<Client>& client, const Bytes& payload) {
        try { send_lobby(*client, payload, PUSH_RECORD); } catch (const std::exception& error) {
            Json f = Json::object();
            f.set("online_id", Json::of(client->user.online_id));
            f.set("error", Json::of(error.what()));
            record("lobby_push_failed", std::move(f));
        }
    }
    void create_ready_lobbies() { // state held
        std::map<std::uint64_t, std::string> available;
        for (const auto& [account, client] : live_clients)
            if (!party || party->admits(client->user.room_token,client->user.online_id)) available[account] = client->user.room_token;
        for (LobbyPlan& plan : matchmaking.match_ready(available, party ? party->expected_players() : std::map<std::string, std::size_t>{})) {
            const Json advertisement = parse_json(*matchmaking.player_info(plan.host));
            const auto [host_doc, backend_doc] = lobby_documents(plan, advertisement, static_cast<std::uint64_t>(std::time(nullptr)));
            matchmaking.save_documents(plan.lobby_id, host_doc, backend_doc, 1);
            const PlanPlayer& player = *std::find_if(plan.players.begin(), plan.players.end(),
                                                     [&](const PlanPlayer& p) { return p.account == plan.host; });
            const auto* saved=matchmaking.documents(plan.lobby_id);
            const Bytes event = room_create_event(player.search_id, plan.lobby_id, saved->host, saved->backend);
            Json members = Json::array();
            for (const PlanPlayer& p : plan.players) members.items.push_back(Json::of(p.account));
            const std::uint64_t lobby = plan.lobby_id, host = plan.host;
            pending_lobbies[lobby] = std::move(plan);
            if (const auto it = live_clients.find(host); it != live_clients.end()) push(it->second, event);
            Json f = Json::object();
            f.set("lobby", Json::of(lobby));
            f.set("host", Json::of(host));
            f.set("members", members);
            record("room_host_notified", std::move(f));
        }
    }
    void publish_host_document(std::uint64_t lobby) { // state held
        const auto found = pending_lobbies.find(lobby);
        const LobbyDocuments* docs = matchmaking.documents(lobby);
        if (found == pending_lobbies.end() || !docs) return;
        LobbyPlan& plan = found->second;
        for (const PlanPlayer& player : plan.players) {
            const auto it = live_clients.find(player.account);
            if (it == live_clients.end() || it->second->user.room_token != plan.selector) continue;
            if (player.account == plan.host || plan.guests_notified.count(player.account)) {
                push(it->second, room_update_event(lobby, docs->host, docs->backend));
            } else {
                push(it->second, room_join_event(player.search_id, lobby, docs->host, docs->backend));
                plan.guests_notified.insert(player.account);
                Json f = Json::object();
                f.set("lobby", Json::of(lobby));
                f.set("account", Json::of(player.account));
                f.set("host", Json::of(plan.host));
                record("room_guest_notified", std::move(f));
            }
        }
        Json f = Json::object();
        f.set("lobby", Json::of(lobby));
        f.set("version", docs->host.at("update_id"));
        record("room_host_document_synced", std::move(f));
    }
    Bytes handle_task(const TaskRequest& request, const AuthSession& user, bool& synced) { // state held
        const unsigned service = request.service;
        const std::uint64_t task = request.task;
        const std::vector<Param>& params = request.params;
        synced = false;
        if (request.rest) {
            Writer w;
            w.u64(next_transaction()).u32(0).u8(static_cast<unsigned>(task)).raw(rest_response(*request.rest));
            return w.data();
        }
        auto guarded = [&](auto body) -> Bytes {
            try { return body(); } catch (const std::exception&) { return task_reply(task, nullptr, UNSERVED_ERROR); }
        };
        if (service == 38 && task == 6) return task_reply(task);
        if (service == 12 && task == 6 && params.empty()) {
            // One result, one total result, UTC seconds.
            Writer w;
            w.u32(1).u32(1).u32(static_cast<std::uint32_t>(std::time(nullptr)));
            return task_reply(task, &w);
        }
        if (auto answer = pitstop_task(request, user.user_id, *marketplace, *achievements, user.wumpa_multiplier)) return *answer;
        if (service == 4 && task == 1) return guarded([&] { stats->write(params, user.user_id); return task_reply(task); });
        if (service == 145 && task == 2)
            return guarded([&] { matchmaking.advertise(params, user.user_id, user.room_token); return task_reply(task); });
        if (service == 145 && task == 3 && params.empty())
            return guarded([&] { Writer w; w.u32(1).u32(1).u64(matchmaking.player_token(user.user_id)); return task_reply(task, &w); });
        if (service == 138 && task == 1)
            return guarded([&] { Writer w; w.u32(1).u32(1).blob(matchmaking.register_session(params, user.user_id, user.room_token)); return task_reply(task, &w); });
        if (service == 138 && task == 12)
            return guarded([&] { matchmaking.update_session_players(params, user.user_id, user.room_token); return task_reply(task); });
        if (service == 145 && task == 18)
            return guarded([&] {
                const std::uint64_t id = matchmaking.start_search(params, user.user_id, user.room_token);
                Writer w;
                w.u32(1).u32(1).string("{\"mm_id\":" + std::to_string(id) + "}");
                return task_reply(task, &w);
            });
        if (service == 145 && task == 15)
            return guarded([&] {
                const bool success = matchmaking.sync_document(params, user.user_id, user.room_token);
                synced = success;
                Writer w;
                w.u32(1).u32(1).boolean(success);
                return task_reply(task, &w);
            });
        Writer w;
        w.u64(next_transaction()).u32(UNSERVED_ERROR).u8(static_cast<unsigned>(task));
        return w.data();
    }
    void handle_lobby(SOCKET s, const std::string& peer, const std::string& ip) {
        std::shared_ptr<Client> client;
        auto peer_fields = [&] { Json f = Json::object(); f.set("peer", Json::of(peer)); return f; };
        try {
            Bytes header;
            if (!recv_exact(s, header, LOBBY_CLIENT_HEADER_SIZE)) throw std::ios_base::failure("IncompleteReadError");
            const LobbyHandshake handshake(header);
            if (!send_all(s, handshake.server_header())) throw std::ios_base::failure("ConnectionResetError");
            Bytes size_bytes, rest;
            if (!recv_exact(s, size_bytes, 4)) throw std::ios_base::failure("IncompleteReadError");
            const std::uint32_t size = le32(size_bytes);
            if (size > kMaxFrame) throw ProtocolError("oversize lobby frame");
            if (!recv_exact(s, rest, size)) throw std::ios_base::failure("IncompleteReadError");
            Bytes auth = size_bytes;
            auth.insert(auth.end(), rest.begin(), rest.end());
            std::optional<LobbyHandshake::Result> found;
            AuthSession user;
            {
                std::lock_guard<std::mutex> lock(state);
                std::vector<Bytes> keys;
                for (const AuthSession& session : sessions) keys.push_back(session.session_key);
                found = handshake.complete(auth, keys);
                if (found) user = sessions[found->key_index];
            }
            if (!found) {
                Json f = peer_fields();
                f.set("frame", Json::of(hex(auth)));
                record("lobby_auth_failed", std::move(f));
                send_all(s, lobby_frame(0x84, Bytes{1, 0, 0, 0}));
                return;
            }
            {
                std::lock_guard<std::mutex> lock(state);
                if (live_clients.count(user.user_id)) {
                    Json f = peer_fields();
                    f.set("online_id", Json::of(user.online_id));
                    record("lobby_identity_in_use", std::move(f));
                    return;
                }
                client = std::make_shared<Client>();
                client->socket = s;
                client->crypto = std::make_unique<LobbySession>(found->keys);
                client->user = user;
                client->ip = reachable_address(ip);
                live_clients[user.user_id] = client;
            }
            Json connected = peer_fields();
            connected.set("online_id", Json::of(user.online_id));
            connected.set("connection_id", Json::of(handshake.connection_id()));
            record("lobby_connected", std::move(connected));
            {
                std::lock_guard<std::mutex> lock(client->send_mutex);
                if (!send_all(s, found->done_frame)) throw std::ios_base::failure("ConnectionResetError");
            }
            while (true) {
                if (!recv_exact(s, size_bytes, 4)) throw std::ios_base::failure("IncompleteReadError");
                const std::uint32_t frame_size = le32(size_bytes);
                if (frame_size == 0) {
                    // Public protocol reference: h1-mod lobby_server.cpp. A zero frame is a
                    // keepalive and needs a zero frame back, not task dispatch.
                    {
                        std::lock_guard<std::mutex> lock(client->send_mutex);
                        if (!send_all(s, Bytes(4, 0))) throw std::ios_base::failure("ConnectionResetError");
                    }
                    Json f = peer_fields();
                    f.set("online_id", Json::of(user.online_id));
                    record("lobby_keepalive", std::move(f));
                    continue;
                }
                if (frame_size > kMaxFrame) throw ProtocolError("oversize lobby frame");
                Bytes data;
                if (!recv_exact(s, data, frame_size)) throw std::ios_base::failure("IncompleteReadError");
                if (data.size() < 2 || data[0] != 0xab || data[1] != 0x85) {
                    Json f = peer_fields();
                    f.set("hex", Json::of(hex(data)));
                    record("lobby_frame", std::move(f));
                    continue;
                }
                const auto [payload_type, payload] = client->crypto->open_record(data);
                if (payload_type != 0x86) {
                    Json f = peer_fields();
                    f.set("online_id", Json::of(user.online_id));
                    f.set("payload_type", Json::of(payload_type));
                    f.set("length", Json::of(static_cast<std::uint64_t>(payload.size())));
                    f.set("hex", Json::of(hex(payload)));
                    record("lobby_record", std::move(f));
                    continue;
                }
                const TaskRequest request = parse_task_request(payload);
                std::lock_guard<std::mutex> lock(state);
                bool synced = false;
                const Bytes answer = handle_task(request, user, synced);
                Json f = peer_fields();
                f.set("online_id", Json::of(user.online_id));
                f.set("service", Json::of(request.service));
                f.set("service_name", Json::of(service_name(request.service)));
                f.set("task", Json::of(request.task));
                f.set("task_name", Json::of(task_name(request.service, request.task)));
                f.set("params", request.params_json());
                f.set("request_hex", Json::of(hex(payload)));
                f.set("reply", Json::of(hex(answer)));
                record("lobby_task", std::move(f));
                send_lobby(*client, answer, LSG_TASK_REPLY);
                if (request.service == 145 && request.task == 18) create_ready_lobbies();
                else if (request.service == 145 && request.task == 15 && synced) publish_host_document(request.params[0].value.as_u64());
            }
        } catch (const std::ios_base::failure& error) {
            Json f = peer_fields();
            const std::string what = error.what();
            f.set("error", Json::of(what.find("IncompleteReadError") != std::string::npos ? "IncompleteReadError" : "ConnectionResetError"));
            record("lobby_closed", std::move(f));
        } catch (const std::system_error&) {
            Json f = peer_fields();
            f.set("error", Json::of("ConnectionResetError"));
            record("lobby_closed", std::move(f));
        } catch (const std::exception& error) {
            Json f = peer_fields();
            f.set("error", Json::of(error.what()));
            record("lobby_error", std::move(f));
        }
        if (client) {
            std::lock_guard<std::mutex> lock(state);
            const auto it = live_clients.find(client->user.user_id);
            if (it != live_clients.end() && it->second == client) {
                live_clients.erase(it);
                // Paired with the opt-in retail live-session adapter. Older clients
                // retain their original behavior until the full failover is ready.
                char migration[2]{};
                if (GetEnvironmentVariableA("CTR_EXPERIMENTAL_HOST_MIGRATION",migration,2)==1 && migration[0]=='1') {
                    std::map<std::uint64_t,std::string> remaining;
                    for(const auto& [account,live]:live_clients)remaining[account]=live->user.room_token;
                    for(const auto& promoted:matchmaking.promote_disconnected(client->user.user_id,remaining)) {
                        auto pending=pending_lobbies.find(promoted.lobby_id);
                        if(pending!=pending_lobbies.end())pending->second.host=promoted.host;
                        publish_host_document(promoted.lobby_id);
                        Json evidence=Json::object();
                        evidence.set("lobby",Json::of(promoted.lobby_id));
                        evidence.set("host",Json::of(promoted.host));
                        record("race_host_promoted",std::move(evidence));
                    }
                }
                matchmaking.cancel_pending(client->user.user_id, client->user.room_token);
                relay.forget(client->user.user_id);
            }
        }
    }

    // --- TCP/UDP recorders and discovery -----------------------------------------------
    void handle_tcp_record(SOCKET s, const std::string& peer, std::uint16_t port) {
        Json f = Json::object();
        f.set("port", Json::of(static_cast<unsigned>(port)));
        f.set("peer", Json::of(peer));
        record("tcp_accept", f);
        std::uint64_t total = 0;
        char buffer[65536];
        while (true) {
            const int n = recv(s, buffer, sizeof(buffer), 0);
            if (n == 0) break;
            if (n < 0) {
                Json e = f;
                e.set("error", Json::of(socket_error("recv")));
                record("tcp_error", std::move(e));
                break;
            }
            total += static_cast<std::uint64_t>(n);
            Json d = f;
            d.set("length", Json::of(n));
            d.set("hex", Json::of(hex(Bytes(buffer, buffer + n))));
            record("tcp_data", std::move(d));
        }
        f.set("total", Json::of(total));
        record("tcp_close", std::move(f));
    }
    void udp_loop(SOCKET s, std::uint16_t port, bool is_discovery) {
        std::vector<char> buffer(65536);
        while (!stopping) {
            sockaddr_in peer{};
            int size = sizeof(peer);
            const int n = recvfrom(s, buffer.data(), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&peer), &size);
            if (n < 0) { if (stopping) return; continue; }
            const Bytes data(buffer.begin(), buffer.begin() + n);
            const Endpoint from = endpoint_of(peer);
            Json f = Json::object();
            f.set("port", Json::of(static_cast<unsigned>(port)));
            f.set("peer", Json::of(from.text()));
            f.set("length", Json::of(n));
            f.set("hex", Json::of(hex(data)));
            record("udp_data", std::move(f));
            if (!is_discovery) continue;
            std::optional<std::pair<Endpoint, Bytes>> result;
            try { result = discovery_answer(data, from, discovery); } catch (const std::exception&) { continue; }
            if (!result) continue;
            const auto it = discovery_sockets.find(result->first.text());
            if (it == discovery_sockets.end()) {
                Json u = Json::object();
                u.set("source", pair_json(result->first));
                record("discovery_unavailable", std::move(u));
                continue;
            }
            sendto(it->second, reinterpret_cast<const char*>(result->second.data()), static_cast<int>(result->second.size()), 0,
                   reinterpret_cast<const sockaddr*>(&peer), sizeof(peer));
            Json r = Json::object();
            r.set("peer", Json::of(from.text()));
            r.set("source", pair_json(result->first));
            r.set("request", Json::of(hex(data)));
            r.set("reply", Json::of(hex(result->second)));
            record("discovery_reply", std::move(r));
        }
    }

    void relay_loop(SOCKET s) {
        std::vector<char> buffer(65536);
        const RelayRoster roster = [this](const std::string& room) { return relay_roster(room); };
        while (!stopping) {
            sockaddr_in peer{};
            int size = sizeof(peer);
            const int n = recvfrom(s, buffer.data(), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&peer), &size);
            if (n < 0) { if (stopping) return; continue; }
            const double now = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now().time_since_epoch()).count()) / 1000.0;
            const Endpoint from = endpoint_of(peer);
            Relay::Outcome outcome = relay.handle(Bytes(buffer.begin(), buffer.begin() + n), from, now, roster);
            for (const Relay::Output& o : outcome.out) {
                sockaddr_in to{};
                try { to = address_of(o.to.ip, o.to.port); } catch (const std::exception&) { continue; }
                sendto(s, reinterpret_cast<const char*>(o.data.data()), static_cast<int>(o.data.size()), 0,
                       reinterpret_cast<const sockaddr*>(&to), sizeof(to));
                if (o.data.size() != relay::kAddressed + 1 || o.data[5] != relay::REFUSED || !outcome.event.empty()) continue;
                // Refusals: logged once per sender endpoint and reason.
                const char* reason = relay::reason_name(o.data.back());
                {
                    std::lock_guard<std::mutex> lock(relay_log_mutex);
                    if (!relay_refusals_logged.insert(from.text() + "/" + reason).second) continue;
                }
                Json f = Json::object();
                f.set("peer", Json::of(from.text()));
                f.set("reason", Json::of(reason));
                f.set("destination", Json::of(std::to_string(o.data[6]) + "." + std::to_string(o.data[7]) + "." +
                                              std::to_string(o.data[8]) + "." + std::to_string(o.data[9]) + ":" +
                                              std::to_string(o.data[10] << 8 | o.data[11])));
                record("relay_refused", std::move(f));
            }
            if (!outcome.event.empty()) record(outcome.event.c_str(), std::move(outcome.fields));
        }
    }

    // --- start/stop ---------------------------------------------------------------------
    bool open(const std::string& name, int type, const std::string& ip, std::uint16_t port, SOCKET& out) {
        try {
            out = open_socket(type, ip, port);
        } catch (const std::exception& error) {
            Json f = Json::object();
            f.set("port", Json::of(name));
            f.set("error", Json::of(error.what()));
            record("listen_failed", std::move(f));
            failed.push_back(name + ": " + error.what());
            return false;
        }
        listeners.push_back(out);
        listening.push_back(name);
        return true;
    }
    std::vector<std::string> start() {
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
        wsa = true;
        if (!config.public_host.empty()) {
            address_of(config.public_host, 0); // validates the IPv4 text
            if (!config.discovery_port) config.discovery_port = 3078;
        } else {
            config.discovery_port = 3074;
        }
        fs::create_directories(config.log_path.parent_path());
        log.open(config.log_path, std::ios::out | std::ios::trunc | std::ios::binary);
        fs::create_directories(config.state_dir);
        key = std::make_unique<RsaKey>(config.state_dir / "server_key.cng");
        std::ofstream(config.state_dir / "server_key.der", std::ios::binary)
            .write(reinterpret_cast<const char*>(key->spki().data()), static_cast<std::streamsize>(key->spki().size()));
        fs::path ident_path = config.identity_path;
        if (ident_path.empty()) ident_path = config.state_dir.parent_path() / "identity" / "host_identity.json";
        host_identity = std::make_unique<HostIdentity>(HostIdentity::load_or_create(ident_path));
        if (config.party_enabled) party = std::make_unique<PartyLobby>(host_identity->friend_code());
        stats = std::make_unique<StatsStore>(config.state_dir / "stats.txt");
        marketplace = std::make_unique<MarketplaceStore>(config.state_dir / "marketplace.txt");
        achievements = std::make_unique<AchievementStore>(config.state_dir / "achievement_events.jsonl");

        SOCKET s;
        if (open("http/" + std::to_string(config.http_port), SOCK_STREAM, config.host, config.http_port, s))
            listener_threads.emplace_back([this, s] { accept_loop(s, [this](SOCKET c, std::string peer) { handle_http(c, peer); }); });
        if (open("lobby/tcp/3074", SOCK_STREAM, config.host, 3074, s))
            listener_threads.emplace_back([this, s] {
                accept_loop(s, [this](SOCKET c, std::string peer) { handle_lobby(c, peer, peer.substr(0, peer.rfind(':'))); });
            });
        for (const std::uint16_t port : config.tcp_record_ports)
            if (open("tcp/" + std::to_string(port), SOCK_STREAM, config.host, port, s))
                listener_threads.emplace_back([this, s, port] {
                    accept_loop(s, [this, port](SOCKET c, std::string peer) { handle_tcp_record(c, peer, port); });
                });
        std::vector<std::pair<Endpoint, Endpoint>> bindings; // advertised, bound
        if (!config.public_host.empty()) {
            const std::uint16_t base = config.discovery_port;
            discovery = DiscoveryLayout{{config.public_host, base}, {config.public_host, static_cast<std::uint16_t>(base + 2)},
                                        static_cast<std::uint16_t>(base + 1), static_cast<std::uint16_t>(base + 3), config.public_host};
            for (const Endpoint& advertised : discovery.endpoints()) bindings.push_back({advertised, {config.host, advertised.port}});
        } else if (!config.peer_host.empty()) {
            discovery = DiscoveryLayout{{config.peer_host, 3074}, {config.stun_secondary_host, 3074}, config.stun_alternate_port, std::nullopt, ""};
            for (const Endpoint& endpoint : discovery.endpoints()) bindings.push_back({endpoint, endpoint});
        }
        for (const auto& [advertised, bound] : bindings) {
            if (!open("discovery/" + bound.text(), SOCK_DGRAM, bound.ip, bound.port, s)) continue;
            discovery_sockets[advertised.text()] = s;
        }
        for (const auto& [advertised, socket_handle] : discovery_sockets) {
            const SOCKET u = socket_handle;
            const std::uint16_t port = static_cast<std::uint16_t>(std::stoi(advertised.substr(advertised.rfind(':') + 1)));
            listener_threads.emplace_back([this, u, port] { udp_loop(u, port, true); });
        }
        if (config.relay_port && open("relay/udp/" + std::to_string(config.relay_port), SOCK_DGRAM, config.host, config.relay_port, s)) {
            relay_socket = s;
            listener_threads.emplace_back([this, s] { relay_loop(s); });
        }
        for (const std::uint16_t port : config.udp_record_ports)
            if (open("udp/" + std::to_string(port), SOCK_DGRAM, config.host, port, s))
                listener_threads.emplace_back([this, s, port] { udp_loop(s, port, false); });

        Json ports = Json::array();
        for (const std::string& name : listening) ports.items.push_back(Json::of(name));
        Json f = Json::object();
        f.set("host", Json::of(config.host));
        f.set("ports", ports);
        f.set("log", Json::of(config.log_path.string()));
        f.set("traffic_key", Json::of((config.state_dir / "server_key.der").string()));
        f.set("public_host", config.public_host.empty() ? Json::null() : Json::of(config.public_host));
        f.set("discovery_port", Json::of(static_cast<unsigned>(config.discovery_port)));
        f.set("relay_port", relay_socket == INVALID_SOCKET ? Json::null() : Json::of(static_cast<unsigned>(config.relay_port)));
        f.set("implementation", Json::of("c++"));
        record("listening", std::move(f));
        return listening;
    }
    void stop() {
        if (stopping.exchange(true)) return;
        for (const SOCKET s : listeners) closesocket(s);
        {
            std::lock_guard<std::mutex> lock(workers_mutex);
            for (const SOCKET s : connections) shutdown(s, SD_BOTH);
        }
        for (std::thread& t : listener_threads) if (t.joinable()) t.join();
        std::vector<Worker> remaining;
        {
            std::lock_guard<std::mutex> lock(workers_mutex);
            remaining.swap(workers);
        }
        for (Worker& w : remaining) if (w.thread.joinable()) w.thread.join();
        record("stopped");
        std::lock_guard<std::mutex> lock(log_mutex);
        log.close();
    }
    ~Impl() {
        stop();
        if (wsa) WSACleanup();
    }
};

Server::Server(ServerConfig config) : impl_(std::make_unique<Impl>()) { impl_->config = std::move(config); }
Server::~Server() = default;
std::vector<std::string> Server::start() { return impl_->start(); }
void Server::stop() { impl_->stop(); }
const std::vector<std::string>& Server::failures() const { return impl_->failed; }
fs::path Server::traffic_key_path() const { return impl_->config.state_dir / "server_key.der"; }
std::size_t Server::connected_players() const {
    std::lock_guard<std::mutex> lock(impl_->state);
    return impl_->live_clients.size();
}
std::string Server::friend_code() const {
    return impl_->host_identity ? impl_->host_identity->friend_code() : std::string();
}
const HostIdentity* Server::host_identity() const {
    return impl_->host_identity.get();
}

std::optional<Json> entitlement_page(const std::string& query) {
    const auto first = [&](const char* name) -> std::optional<std::int64_t> {
        const std::vector<std::string> values = query_values(query, name);
        return values.empty() ? std::nullopt : python_int(values[0]);
    };
    const auto start = first("start"), size = first("size"), label = first("service_label");
    if (!start || !size || !label || query_values(query, "entitlement_type") != std::vector<std::string>{"service"} ||
        *start < 0 || *size <= 0 || *size > 1000)
        return std::nullopt;
    Json page = Json::object();
    page.set("total_results", Json::of(0));
    page.set("entitlements", Json::array());
    return page;
}

fs::path find_project_root(const fs::path& exe_dir) {
    for (fs::path p = exe_dir; !p.empty(); p = p.parent_path()) {
        std::error_code ec;
        if (fs::is_directory(p / "runtime", ec) || fs::is_directory(p / "ctrport", ec)) return p;
        if (p == p.parent_path()) break;
    }
    return exe_dir;
}
fs::path default_log_path(const fs::path& root) {
    return root / "runtime" / "user" / "logs" / ("online_server_" + now_stamp() + ".jsonl");
}
} // namespace ctr_online
