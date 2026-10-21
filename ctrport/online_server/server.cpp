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
#include <sstream>
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
// Refresh the socket timeout against an absolute deadline, including slow-drip clients.
bool read_deadline(SOCKET s, std::chrono::steady_clock::time_point deadline) {
    const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
    if(remaining<=0) return false;
    const DWORD timeout=static_cast<DWORD>(remaining);
    return setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<const char*>(&timeout),sizeof(timeout))==0;
}
bool recv_exact(SOCKET s, Bytes& out, std::size_t size,
                std::chrono::steady_clock::time_point deadline=std::chrono::steady_clock::time_point::max()) {
    out.resize(size);
    std::size_t done = 0;
    while (done < size) {
        if(deadline!=std::chrono::steady_clock::time_point::max() && !read_deadline(s,deadline)) return false;
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
    std::unique_ptr<FriendWatcher> friend_watcher;
    PeerNonces peer_nonces;
    // Players of this PC whose public matchmaking runs on another service (the hub), under
    // `state`; hub_leaving: players whose game disconnected, to be signed off at the hub.
    struct HubLeave { std::string name; MatchHub hub; };
    std::map<std::uint64_t, MatchHub> hub_seats;
    // Players of this PC whose current public sequence is handled here (under `state`).
    std::set<std::uint64_t> local_seats;
    std::vector<HubLeave> hub_leaving;
    std::thread hub_poller;
    // Invitations a friend's service delivered to this PC's player. Their session stays on that
    // service (the origin), where accepting, dismissing and the session routes are forwarded.
    struct RemoteInvitation {
        std::string session, sender, origin;
        std::uint64_t recipient = 0, expires = 0;
        bool accepted = false;
    };
    std::map<std::string, RemoteInvitation> remote_invitations; // under `state`
    std::map<std::string, std::string> remote_sessions;         // PSN session ID -> origin code, under `state`
    // JOIN from the friends list goes through a friend's Delta record (138/14): the game
    // contacts the friend right away, before any session request names it. Guest side:
    // friends whose Delta record this PC's game received (code -> expiry); host side:
    // friends' players announced as joining this PC's player (account -> expiry). Under `state`.
    std::map<std::string, std::uint64_t> delta_offers;
    std::map<std::uint64_t, std::uint64_t> delta_joiners;
    std::vector<std::string> listening, failed;

    // State shared by connections (the reference server's module globals), under `state`.
    std::mutex state;
    std::vector<AuthSession> sessions; // auth3 logins, in order
    struct Client {
        SOCKET socket = INVALID_SOCKET;
        std::mutex send_mutex;
        std::unique_ptr<LobbySession> crypto;
        AuthSession user;
        std::string ip;
        // A player of another PC matching on this hub: no socket; its service (friend code
        // hub_origin) collects the pushed events from the outbox (peer action mm_poll).
        std::string hub_origin;
        std::vector<Bytes> outbox;
        std::uint64_t last_seen = 0;
    };
    std::map<std::uint64_t, std::shared_ptr<Client>> live_clients;
    std::map<std::uint64_t, LobbyPlan> pending_lobbies;
    std::unique_ptr<StatsStore> stats;
    std::unique_ptr<MarketplaceStore> marketplace;
    std::unique_ptr<AchievementStore> achievements;
    SessionStore session_store;
    std::map<std::uint64_t, Json> game_presence;
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
    std::map<SOCKET,std::uint32_t> connection_ips;
    std::map<std::uint32_t,unsigned> connections_per_ip;
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
    // The local game connects from 127.0.0.1: its own quota, so remote sources holding every
    // remote slot cannot cut it off from its service.
    unsigned local_connections = 0;
    static bool local_game(std::uint32_t ip) { return ip == htonl(INADDR_LOOPBACK); }
    bool track(SOCKET s, std::uint32_t ip) {
        std::lock_guard<std::mutex> lock(workers_mutex);
        if(stopping) return false;
        if(local_game(ip)) {
            if(local_connections>=64) return false;
            ++local_connections;
        } else {
            if(connections.size()-local_connections>=128) return false;
            auto it=connections_per_ip.find(ip);
            if(it!=connections_per_ip.end() && it->second>=8) return false;
            ++connections_per_ip[ip];
        }
        connection_ips[s]=ip; connections.insert(s); return true;
    }
    void release(SOCKET s) {
        { std::lock_guard<std::mutex> lock(workers_mutex);
          if(auto it=connection_ips.find(s);it!=connection_ips.end()) {
              if(local_game(it->second)) --local_connections;
              else if(auto count=connections_per_ip.find(it->second);count!=connections_per_ip.end() && --count->second==0)
                  connections_per_ip.erase(count);
              connection_ips.erase(it);
          }
          connections.erase(s); }
        closesocket(s);
    }
    void accept_loop(SOCKET listener, std::function<void(SOCKET, std::string)> handler) {
        while (!stopping) {
            sockaddr_in peer{};
            int size = sizeof(peer);
            const SOCKET s = accept(listener, reinterpret_cast<sockaddr*>(&peer), &size);
            if (s == INVALID_SOCKET) { if (stopping) return; continue; }
            const std::string name = endpoint_of(peer).text();
            if(!track(s,peer.sin_addr.s_addr)) { closesocket(s); continue; }
            // No idle read timeout here: HTTP sets its own deadlines, and the lobby and recorders
            // listen on loopback only, where a game left in a menu must not be disconnected.
            const DWORD send_timeout=5000;
            setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,reinterpret_cast<const char*>(&send_timeout),sizeof(send_timeout));
            try { spawn([this, s, name, handler] {
                try { handler(s, name); } catch (const std::exception& error) {
                    Json f = Json::object();
                    f.set("peer", Json::of(name));
                    f.set("error", Json::of(error.what()));
                    record("connection_error", std::move(f));
                }
                release(s);
            }); } catch(...) { release(s); }
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

    // Called under state. Membership comes from an assigned lobby or a readable session
    // being joined, not from sharing the public matchmaking selector or a friend code.
    std::set<std::string> connection_addresses(std::uint64_t account, const std::string& room) {
        auto peers = matchmaking.connection_peers(account, room);
        const auto session_peers = session_store.connection_peers(account, party_now());
        peers.insert(session_peers.begin(), session_peers.end());
        if (is_local(account))
            for (const auto& [joiner, until] : delta_joiners) if (until > party_now()) peers.insert(joiner);
        peers.insert(account); // discovery may contact our own public address
        std::set<std::string> result;
        for (const auto peer : peers) {
            if (const auto it = live_clients.find(peer); it != live_clients.end()) {
                if (!it->second->ip.empty()) result.insert(reachable_address(it->second->ip));
            } else if (friend_watcher) {
                const auto known = friend_watcher->by_account(peer);
                if (known && known->online && known->in_game && friend_watcher->is_friend(known->code))
                    result.insert(known->ip);
            }
        }
        return result;
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

    // Experiment only (CTR_ONLINE_EXPERIMENT_FRIENDS=1): every other player connected to this
    // server is listed as an online friend. It exists to record what the game sends next
    // (Delta lookups, join); it is not a friend graph. With friend_network the list is the
    // rubrica instead.
    static bool experiment_friends() {
        static const bool on = [] {char v[4]{}; return GetEnvironmentVariableA("CTR_ONLINE_EXPERIMENT_FRIENDS", v, sizeof(v)) == 1 && v[0] == '1';}();
        return on;
    }
    bool friends_listed() const { return friend_watcher || experiment_friends(); }
    struct ListedFriend {
        std::string name;
        std::uint64_t account = 0;
        bool online = false;
        Json context = Json::object(); // stored gameStatus/gameData
    };
    // Friends of `requester` for the in-game list. friend_network: the rubrica's friends whose
    // online ID is known, online while their game is connected to their own service.
    std::vector<ListedFriend> listed_friends(const std::string& requester) {
        std::vector<ListedFriend> result;
        if (friend_watcher) {
            for (const auto& known : friend_watcher->friends()) {
                if (known.name.empty() || known.name == requester) continue;
                ListedFriend row;
                row.name = known.name;
                row.account = known.account;
                row.online = known.online && known.in_game;
                if (const Json* saved = known.report.find("game_presence"); saved && saved->is_object()) row.context = *saved;
                result.push_back(std::move(row));
            }
            return result;
        }
        // CTR_ONLINE_EXPERIMENT_FRIEND_NAMES (comma separated) gives a fixed, persistent list in which
        // connected players are online and the others offline; without it, connected players only.
        std::vector<std::string> names;
        std::set<std::string> online;
        std::map<std::uint64_t, Json> presence_snapshot;
        {
            std::lock_guard<std::mutex> lock(state);
            for (const auto& [account, client] : live_clients) online.insert(client->user.online_id);
            presence_snapshot = game_presence;
        }
        char fixed[512]{};
        const DWORD fixed_length = GetEnvironmentVariableA("CTR_ONLINE_EXPERIMENT_FRIEND_NAMES", fixed, sizeof(fixed));
        if (fixed_length && fixed_length < sizeof(fixed)) {
            std::stringstream list(fixed);
            for (std::string name; std::getline(list, name, ',');) if (!name.empty() && name != requester) names.push_back(name);
        } else {
            for (const auto& name : online) if (name != requester) names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        for (const auto& name : names) {
            ListedFriend row;
            row.name = name;
            row.account = local_account(name);
            row.online = online.count(name) > 0;
            if (const auto saved = presence_snapshot.find(row.account); saved != presence_snapshot.end()) row.context = saved->second;
            result.push_back(std::move(row));
        }
        return result;
    }
    // The friend page in the shape read by the retail NpToolkit2 module (key names next to its
    // friendList request template).
    Json listed_friend_page(const std::string& requester, std::int64_t offset, std::int64_t limit) {
        if (offset < 0 || limit < 1 || limit > 500) throw std::invalid_argument("invalid friend pagination");
        const auto rows = listed_friends(requester);
        Json list = Json::array();
        for (std::size_t i = static_cast<std::size_t>(offset); i < rows.size() && list.items.size() < static_cast<std::size_t>(limit); ++i) {
            const auto& row = rows[i];
            Json user = Json::object();
            user.set("accountId", Json::of(std::to_string(row.account)));
            user.set("onlineId", Json::of(row.name));
            Json detail = Json::object();
            detail.set("displayName", Json::of(row.name));
            Json title = Json::object();
            title.set("npTitleId", Json::of("CUSA14876_00"));
            title.set("titleName", Json::of("Crash Team Racing Nitro-Fueled"));
            Json context = Json::object();
            context.set("gameStatus", Json::of(""));
            context.set("gameData", Json::of(""));
            for (const auto& [field_name, value] : row.context.members) context.set(field_name, value);
            context.set("gameTitleInfo", title);
            Json contexts = Json::array();
            contexts.items.push_back(context);
            Json presence = Json::object();
            const bool is_online = row.online;
            presence.set("onlineStatus", Json::of(is_online ? "online" : "offline"));
            presence.set("platform", Json::of("PS4"));
            if (is_online) presence.set("incontextInfoList", contexts);
            Json entry = Json::object();
            entry.set("user", user);
            entry.set("avatarUrl", Json::of(""));
            entry.set("region", Json::of("it"));
            entry.set("isOfficiallyVerified", Json::of(false));
            entry.set("personalDetail", detail);
            entry.set("presence", presence);
            list.items.push_back(entry);
        }
        Json page = Json::object();
        page.set("start", Json::of(static_cast<std::uint64_t>(offset)));
        page.set("size", Json::of(static_cast<std::uint64_t>(list.items.size())));
        page.set("totalResults", Json::of(static_cast<std::uint64_t>(rows.size())));
        page.set("friendList", list);
        return page;
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
    // A game connected to this service itself (not a player of another PC matching on this hub).
    bool is_local(std::uint64_t account) const {
        const auto it = live_clients.find(account);
        return it != live_clients.end() && it->second->hub_origin.empty();
    }
    bool any_local() const {
        for (const auto& [account, client] : live_clients) if (client->hub_origin.empty()) return true;
        return false;
    }
    // Online ID this service reports to friends: the configured one, else the first game connected.
    std::string local_player_name() {
        if (!config.local_name.empty()) return config.local_name;
        std::lock_guard<std::mutex> lock(state);
        for (const auto& [account, client] : live_clients) if (client->hub_origin.empty()) return client->user.online_id;
        return {};
    }
    static std::string text_field(const Json& object, const char* key) {
        const Json* value = object.find(key);
        return value && value->is_string() ? value->text : std::string();
    }
    static bool lower_hex(const std::string& text, std::size_t size) {
        return text.size() == size && text.find_first_not_of("0123456789abcdef") == std::string::npos;
    }
    // This PC's friend code, empty without a friend identity (then accounts are the name alone).
    std::string own_code() const { return host_identity ? host_identity->friend_code() : std::string(); }
    std::uint64_t local_account(const std::string& name) const { return player_account(own_code(), name); }
    // The account the game should see for a player it knows only by name: this PC's player,
    // a friend, or a public matchmaking player; otherwise the name-only account. `state` held.
    std::uint64_t account_by_name(const std::string& name, const AuthSession& asking) {
        if (name == asking.online_id) return asking.user_id;
        if (friend_watcher)
            for (const auto& known : friend_watcher->friends())
                if (known.name == name && known.account) return known.account;
        if (!config.local_name.empty() && name == config.local_name) return local_account(name);
        for (const auto& [account, client] : live_clients) if (client->user.online_id == name) return account;
        return user_id(name);
    }
    // A friend's player as its own service names it; never a player of this PC.
    void check_friend_player(const std::string& caller, const std::string& name) {
        if (name.empty() || name.size() > 32) throw std::runtime_error("Friend player not specified");
        if (const auto known = friend_watcher->by_code(caller); known && !known->name.empty() && known->name != name)
            throw std::runtime_error("Friend player does not match");
        if (name == local_player_name()) throw std::runtime_error("Player on this PC");
        std::lock_guard<std::mutex> lock(state);
        if (is_local(local_account(name))) throw std::runtime_error("Player on this PC");
    }
    // Actions a friend's service may ask for (POST /ctr/v1/peer, already authenticated).
    // presence: this PC's player, whether its game is connected, its game status, the PSN
    //   sessions it is in that the caller's player may see, and its published Delta sessions;
    // invitation: a friend's player invites this PC's player to a session on the friend's service;
    // webapi: a session or invitation request of the caller's player for a session kept here.
    Json peer_action(const std::string& caller, const std::string& action, const Json& request,
                     const std::string& source_ip) {
        Json response = Json::object();
        if (action == "connection_peers") {
            const auto name = text_field(request, "name");
            check_friend_player(caller, name);
            std::lock_guard<std::mutex> lock(state);
            // This action is a session authority query only; never forwards to another service.
            std::set<std::string> addresses = connection_addresses(player_account(caller, name), "");
            // A friend's player joining through this PC's Delta record: admitted for a minute,
            // reaching this PC's players (it is a verified friend, see check_friend_player).
            if (const Json* joining = request.find("joining"); joining && joining->is_bool() && joining->boolean) {
                const auto now = party_now();
                for (auto it = delta_joiners.begin(); it != delta_joiners.end();)
                    it = it->second <= now ? delta_joiners.erase(it) : std::next(it);
                delta_joiners[player_account(caller, name)] = now + 60;
                for (const auto& [account, client] : live_clients)
                    if (client->hub_origin.empty() && !client->ip.empty()) addresses.insert(reachable_address(client->ip));
            }
            Json peers = Json::array();
            for (const auto& ip : addresses) peers.items.push_back(Json::of(ip));
            response.set("peers", std::move(peers));
            return response;
        }
        if (action == "presence") {
            const std::string name = local_player_name(), viewer = text_field(request, "name");
            std::lock_guard<std::mutex> lock(state);
            response.set("name", Json::of(name));
            response.set("in_game", Json::of(any_local()));
            const auto account = name.empty() ? 0 : local_account(name);
            const auto saved = account ? game_presence.find(account) : game_presence.end();
            response.set("game_presence", saved == game_presence.end() ? Json::object() : saved->second);
            Json delta = Json::array();
            if (account) {
                for (const auto& [context, session] : matchmaking.owned_sessions(account)) {
                    Json row = Json::object();
                    row.set("context", Json::of(context));
                    row.set("address", Json::of(hex(session.address)));
                    row.set("id", Json::of(hex(session.id)));
                    row.set("game_type", Json::of(session.game_type));
                    row.set("max_players", Json::of(session.max_players));
                    row.set("players", Json::of(session.players));
                    row.set("custom", Json::of(hex(session.custom)));
                    delta.items.push_back(std::move(row));
                }
            }
            response.set("sessions", account && !viewer.empty() ? session_store.user_sessions(player_account(caller, viewer), account) : Json::array());
            response.set("delta", std::move(delta));
            return response;
        }
        if (action == "invitation") {
            const std::string invitation = text_field(request, "invitation"), session = text_field(request, "session"),
                              sender = text_field(request, "sender");
            const Json* recipient = request.find("recipient");
            if (!lower_hex(invitation, 32) || !lower_hex(session, 32) || !recipient || !recipient->is_unsigned())
                throw std::runtime_error("Invalid invitation");
            check_friend_player(caller, sender);
            const std::string local = local_player_name();
            const auto now = static_cast<std::uint64_t>(std::time(nullptr));
            std::lock_guard<std::mutex> lock(state);
            if (!is_local(recipient->as_u64()) && (local.empty() || local_account(local) != recipient->as_u64()))
                throw std::runtime_error("Recipient not on this PC");
            for (auto it = remote_invitations.begin(); it != remote_invitations.end();)
                it = it->second.expires <= now ? remote_invitations.erase(it) : std::next(it);
            if (remote_invitations.size() >= 64 && !remote_invitations.count(invitation))
                throw std::runtime_error("Troppi inviti in attesa");
            // Retrying delivery must not turn an accepted invitation back into a pending one.
            const auto existing = remote_invitations.find(invitation);
            if (existing != remote_invitations.end()) {
                const auto& previous = existing->second;
                if (previous.session != session || previous.sender != sender || previous.origin != caller ||
                    previous.recipient != recipient->as_u64())
                    throw std::runtime_error("Invitation ID already used");
            } else {
                remote_invitations.emplace(invitation,
                    RemoteInvitation{session, sender, caller, recipient->as_u64(), now + 300, false});
            }
            remote_sessions[session] = caller;
            response.set("delivered", Json::of(true));
            return response;
        }
        if (action == "mm_task" || action == "mm_poll" || action == "mm_leave" || action == "mm_peers")
            return hub_action(caller, action, request, source_ip);
        if (action == "webapi") {
            const std::string method = text_field(request, "method"), target = text_field(request, "target"),
                              name = text_field(request, "name");
            const bool allowed_target = starts_with(target, "/webapi/sdk:sessionInvitation/v1/sessions/") ||
                                        starts_with(target, "/webapi/sdk:sessionInvitation/v1/users/me/invitations/") ||
                                        starts_with(target, "/ctr/v1/session-invitations/");
            if (!allowed_target || (method != "GET" && method != "PUT" && method != "POST" && method != "DELETE"))
                throw std::runtime_error("Request cannot be forwarded");
            check_friend_player(caller, name);
            std::map<std::string, std::string> headers{{"host", "ctr-online"}, {"x-ctr-online-id", name}};
            if (const auto type = text_field(request, "content_type"); !type.empty()) headers.emplace("content-type", type);
            const HttpAnswer answer = http_answer(method, target, headers, base64_decode(text_field(request, "body")), "peer:" + caller);
            std::string type;
            for (const auto& [header_name, value] : answer.headers) if (lower(header_name) == "content-type") type = value;
            response.set("status", Json::of(answer.status));
            response.set("content_type", Json::of(type));
            response.set("body", Json::of(base64_encode(Bytes(answer.body.begin(), answer.body.end()))));
            return response;
        }
        throw std::runtime_error("Unsupported action: " + action);
    }
    // --- public matchmaking hub ---------------------------------------------------------
    // Hub side: players of other PCs, identified by their service's signed code, run their
    // public search here. Only the matchmaking tasks are accepted; no friendship is required.
    Json hub_action(const std::string& caller, const std::string& action, const Json& request,
                    const std::string& source_ip) {
        const std::string name = text_field(request, "name");
        if (name.empty() || name.size() > 32) throw std::runtime_error("Player not specified");
        const auto account = player_account(caller, name);
        const auto now = static_cast<std::uint64_t>(std::time(nullptr));
        Json response = Json::object();
        std::lock_guard<std::mutex> lock(state);
        expire_hub_clients(now);
        auto found = live_clients.find(account);
        if (found != live_clients.end() && found->second->hub_origin != caller)
            throw std::runtime_error("Player already on this service");
        if (found != live_clients.end()) found->second->ip = source_ip;
        if (action == "mm_peers") {
            if (found == live_clients.end()) throw std::runtime_error("Player not present");
            Json peers = Json::array();
            for (const auto& ip : connection_addresses(account, public_room_token())) peers.items.push_back(Json::of(ip));
            response.set("peers", std::move(peers));
            return response;
        }
        if (action == "mm_leave") {
            if (found != live_clients.end()) forget_client(account, "left");
            response.set("left", Json::of(true));
            return response;
        }
        if (action == "mm_poll") {
            if (found == live_clients.end()) throw std::runtime_error("Player not present");
            found->second->last_seen = now;
            Json events = Json::array();
            for (const Bytes& event : found->second->outbox) events.items.push_back(Json::of(base64_encode(event)));
            found->second->outbox.clear();
            response.set("events", std::move(events));
            return response;
        }
        const TaskRequest task = parse_task_request(base64_decode(text_field(request, "task")));
        if (task.service != 145 || (task.task != 2 && task.task != 3 && task.task != 15 && task.task != 18))
            throw std::runtime_error("Matchmaking task not allowed");
        if (found == live_clients.end()) {
            std::size_t from_caller = 0, remote = 0;
            for (const auto& [id, client] : live_clients) {
                if (client->hub_origin.empty()) continue;
                ++remote;
                if (client->hub_origin == caller) ++from_caller;
            }
            if (from_caller >= 4 || remote >= 64) throw std::runtime_error("Hub full");
            auto client = std::make_shared<Client>();
            client->user.online_id = name;
            client->user.user_id = account;
            client->user.room_token = public_room_token();
            client->hub_origin = caller;
            client->ip = source_ip;
            found = live_clients.emplace(account, std::move(client)).first;
            Json f = Json::object();
            f.set("online_id", Json::of(name));
            f.set("origin", Json::of(caller));
            record("hub_player_joined", std::move(f));
        }
        const auto client = found->second;
        client->last_seen = now;
        bool synced = false;
        const Bytes answer = handle_task(task, client->user, synced);
        Json f = Json::object();
        f.set("online_id", Json::of(name));
        f.set("origin", Json::of(caller));
        f.set("service", Json::of(task.service));
        f.set("task", Json::of(task.task));
        f.set("params", task.params_json());
        f.set("reply", Json::of(hex(answer)));
        record("hub_task", std::move(f));
        if (task.task == 18) create_ready_lobbies();
        else if (task.task == 15 && synced) publish_host_document(task.params[0].value.as_u64());
        response.set("reply", Json::of(base64_encode(answer)));
        return response;
    }
    // Players of other PCs whose service stopped polling (`state` held).
    void expire_hub_clients(std::uint64_t now) {
        std::vector<std::uint64_t> gone;
        for (const auto& [account, client] : live_clients)
            if (!client->hub_origin.empty() && now > client->last_seen + 20) gone.push_back(account);
        for (const auto account : gone) forget_client(account, "expired");
    }
    // Member side: a public matchmaking task of a local player, answered by the hub when one is
    // in use. nullopt: handle it here (no hub, this service is the hub, or the hub failed).
    std::optional<Bytes> hub_task(const TaskRequest& request, const Bytes& payload, const AuthSession& user) {
        if (!config.match_hub || request.service != 145 || user.room_token != public_room_token() ||
            (request.task != 2 && request.task != 3 && request.task != 15 && request.task != 18))
            return std::nullopt;
        // A sequence stays on the side that answered its advertise (task 2): the hub refuses a
        // search from a player it never registered. Task 2 starts a new sequence.
        // A PC that cannot be reached would queue the search where no other player can join, so
        // it refuses the search instead: the game gives up and the next attempt starts over.
        auto stay_local = [&]() -> std::optional<Bytes> {
            {
                std::lock_guard<std::mutex> lock(state);
                local_seats.insert(user.user_id);
            }
            if (config.hub_reachable || request.task != 18) return std::nullopt;
            Json refused = Json::object();
            refused.set("service", Json::of(request.service));
            refused.set("task", Json::of(request.task));
            refused.set("reason", Json::of("search_refused"));
            refused.set("reachable", Json::of(false));
            record("hub_none", std::move(refused));
            return task_reply(request.task, nullptr, UNSERVED_ERROR);
        };
        std::optional<MatchHub> hub;
        bool local_sequence = false;
        {
            std::lock_guard<std::mutex> lock(state);
            if (const auto seat = hub_seats.find(user.user_id); seat != hub_seats.end()) hub = seat->second;
            else if (request.task == 2) local_seats.erase(user.user_id);
            else local_sequence = local_seats.count(user.user_id) != 0;
        }
        if (local_sequence) return stay_local();
        if (!hub) hub = config.match_hub();
        if (!hub && !config.hub_reachable && config.await_match_hub)
            hub = config.await_match_hub(config.hub_wait_budget);
        if (!hub) {
            Json none=Json::object();none.set("service",Json::of(request.service));none.set("task",Json::of(request.task));
            none.set("reason",Json::of(!config.hub_reachable && config.await_match_hub ? "wait_expired" : "no_candidates"));
            none.set("reachable",Json::of(config.hub_reachable));
            none.set("candidates",Json::of(0));
            none.set("wait_ms",Json::of(!config.hub_reachable && config.await_match_hub ? static_cast<unsigned>(config.hub_wait_budget.count()) : 0u));
            record("hub_none",std::move(none));return stay_local();
        }
        if (hub->code == host_identity->friend_code()) return stay_local();
        Json message = Json::object();
        message.set("action", Json::of("mm_task"));
        message.set("name", Json::of(user.online_id));
        message.set("task", Json::of(base64_encode(payload)));
        Json f = Json::object();
        f.set("online_id", Json::of(user.online_id));
        f.set("hub", Json::of(hub->code));
        f.set("service", Json::of(request.service));
        f.set("task", Json::of(request.task));
        try {
            const Json reply = peer_request(*host_identity, hub->ip, hub->port, hub->code, std::move(message), 5000);
            {
                std::lock_guard<std::mutex> lock(state);
                hub_seats[user.user_id] = *hub;
            }
            record("hub_task_forwarded", std::move(f));
            return base64_decode(text_field(reply, "reply"));
        } catch (const std::exception& error) {
            {
                std::lock_guard<std::mutex> lock(state);
                hub_seats.erase(user.user_id);
            }
            f.set("reason", Json::of(error.what()));
            record("hub_unreachable", std::move(f));
            return stay_local();
        }
    }
    // Member side: delivers the events the hub pushed for this PC's players, and signs off
    // players whose game disconnected.
    void hub_poll_loop() {
        std::map<std::uint64_t, unsigned> failures;
        while (!stopping) {
            std::map<std::uint64_t, MatchHub> seats;
            std::vector<HubLeave> leaving;
            {
                std::lock_guard<std::mutex> lock(state);
                seats = hub_seats;
                leaving.swap(hub_leaving);
            }
            for (const auto& leave : leaving) {
                Json message = Json::object();
                message.set("action", Json::of("mm_leave"));
                message.set("name", Json::of(leave.name));
                try { peer_request(*host_identity, leave.hub.ip, leave.hub.port, leave.hub.code, std::move(message), 3000); } catch (...) {}
            }
            for (const auto& [account, hub] : seats) {
                std::string name;
                {
                    std::lock_guard<std::mutex> lock(state);
                    if (const auto it = live_clients.find(account); it != live_clients.end()) name = it->second->user.online_id;
                }
                if (name.empty()) continue;
                Json message = Json::object();
                message.set("action", Json::of("mm_poll"));
                message.set("name", Json::of(name));
                try {
                    const Json reply = peer_request(*host_identity, hub.ip, hub.port, hub.code, std::move(message), 3000);
                    failures.erase(account);
                    std::lock_guard<std::mutex> lock(state);
                    const auto it = live_clients.find(account);
                    if (it == live_clients.end()) continue;
                    for (const auto& event : reply.at("events").items) {
                        push(it->second, base64_decode(event.text));
                        Json f = Json::object();
                        f.set("online_id", Json::of(name));
                        f.set("hub", Json::of(hub.code));
                        record("hub_event_delivered", std::move(f));
                    }
                } catch (const std::exception& error) {
                    if (++failures[account] < 25) continue;
                    failures.erase(account);
                    std::lock_guard<std::mutex> lock(state);
                    hub_seats.erase(account);
                    Json f = Json::object();
                    f.set("online_id", Json::of(name));
                    f.set("hub", Json::of(hub.code));
                    f.set("reason", Json::of(error.what()));
                    record("hub_lost", std::move(f));
                }
            }
            for (int i = 0; i < 4 && !stopping; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    // A player leaves this service: connection closed, hub sign-off or expiry (`state` held).
    void forget_client(std::uint64_t account, const char* why) {
        const auto it = live_clients.find(account);
        if (it == live_clients.end()) return;
        const auto client = it->second;
        game_presence.erase(account);
        live_clients.erase(it);
        // Paired with the opt-in retail live-session adapter. Older clients
        // retain their original behavior until the full failover is ready.
        char migration[2]{};
        if (GetEnvironmentVariableA("CTR_EXPERIMENTAL_HOST_MIGRATION",migration,2)==1 && migration[0]=='1') {
            std::map<std::uint64_t,std::string> remaining;
            for(const auto& [other,live]:live_clients)remaining[other]=live->user.room_token;
            for(const auto& promoted:matchmaking.promote_disconnected(account,remaining)) {
                auto pending=pending_lobbies.find(promoted.lobby_id);
                if(pending!=pending_lobbies.end())pending->second.host=promoted.host;
                publish_host_document(promoted.lobby_id);
                Json evidence=Json::object();
                evidence.set("lobby",Json::of(promoted.lobby_id));
                evidence.set("host",Json::of(promoted.host));
                record("race_host_promoted",std::move(evidence));
            }
        }
        matchmaking.cancel_pending(account, client->user.room_token);
        relay.forget(account);
        if (const auto seat = hub_seats.find(account); seat != hub_seats.end()) {
            hub_leaving.push_back(HubLeave{client->user.online_id, seat->second});
            hub_seats.erase(seat);
        }
        local_seats.erase(account);
        if (!client->hub_origin.empty()) {
            Json f = Json::object();
            f.set("online_id", Json::of(client->user.online_id));
            f.set("origin", Json::of(client->hub_origin));
            f.set("reason", Json::of(why));
            record("hub_player_left", std::move(f));
        }
    }
    // A request of this PC's player about an invitation or a PSN session kept on a friend's
    // service: forwarded there with the player's name. nullopt when the target is local.
    std::optional<HttpAnswer> forward_to_origin(const std::string& method, const std::string& target,
                                                const std::string& content_type, const Bytes& body, const std::string& online_id) {
        if (!friend_watcher || online_id.empty()) return std::nullopt;
        const std::string path = target.substr(0, target.find('?'));
        const auto id_after = [&](const std::string& prefix) {
            if (!starts_with(path, prefix)) return std::string();
            const auto end = path.find('/', prefix.size());
            return path.substr(prefix.size(), end == std::string::npos ? std::string::npos : end - prefix.size());
        };
        std::string origin, invitation;
        {
            std::lock_guard<std::mutex> lock(state);
            const auto account = local_account(online_id);
            std::string id = id_after("/ctr/v1/session-invitations/");
            if (id.empty()) id = id_after("/webapi/sdk:sessionInvitation/v1/users/me/invitations/");
            if (!id.empty()) {
                const auto it = remote_invitations.find(id);
                if (it != remote_invitations.end() && it->second.recipient == account) { origin = it->second.origin; invitation = id; }
            } else if (const auto session = id_after("/webapi/sdk:sessionInvitation/v1/sessions/");
                       !session.empty() && !session_store.contains(session)) {
                if (const auto it = remote_sessions.find(session); it != remote_sessions.end()) origin = it->second;
                // A session ID the game found in a friend's Delta record (138/14): the friend
                // whose service reported that session.
                else for (const auto& known : friend_watcher->friends()) {
                    const Json* reported = known.online ? known.report.find("sessions") : nullptr;
                    if (!reported || !reported->is_array()) continue;
                    for (const auto& row : reported->items)
                        if (text_field(row, "sessionId") == session) origin = known.code;
                    if (!origin.empty()) { remote_sessions[session] = origin; break; }
                }
            }
        }
        if (origin.empty()) return std::nullopt;
        Json request = Json::object();
        request.set("action", Json::of("webapi"));
        request.set("method", Json::of(method));
        request.set("target", Json::of(target));
        request.set("name", Json::of(online_id));
        request.set("content_type", Json::of(content_type));
        request.set("body", Json::of(base64_encode(body)));
        try {
            const Json reply = friend_watcher->request(origin, request, 4000);
            const auto status = static_cast<int>(reply.at("status").as_u64());
            const Bytes data = base64_decode(text_field(reply, "body"));
            HttpAnswer answer{status, {}, std::string(data.begin(), data.end()), "peer_forward"};
            if (const auto type = text_field(reply, "content_type"); !type.empty()) answer.headers.emplace_back("Content-Type", type);
            answer.fields.set("origin", Json::of(origin));
            answer.fields.set("target", Json::of(target));
            if (!invitation.empty() && status < 300 && (method == "PUT" || method == "DELETE")) {
                std::lock_guard<std::mutex> lock(state);
                if (method == "DELETE") remote_invitations.erase(invitation);
                else if (const auto it = remote_invitations.find(invitation); it != remote_invitations.end()) it->second.accepted = true;
            }
            return answer;
        } catch (const std::exception& error) {
            HttpAnswer answer = json_answer(502, R"({"error":"friend service unreachable"})", "peer_forward", false);
            answer.fields.set("origin", Json::of(origin));
            answer.fields.set("target", Json::of(target));
            answer.fields.set("reason", Json::of(error.what()));
            return answer;
        }
    }
    // Remote invitations of `account` in the project poll format (`state` held).
    void append_remote_invitations(Json& rows, std::uint64_t account, std::uint64_t now) {
        for (auto it = remote_invitations.begin(); it != remote_invitations.end();)
            it = it->second.expires <= now ? remote_invitations.erase(it) : std::next(it);
        for (const auto& [id, invite] : remote_invitations) {
            if (invite.recipient != account || invite.accepted) continue;
            Json row = Json::object();
            row.set("invitationId", Json::of(id));
            row.set("sessionId", Json::of(invite.session));
            row.set("onlineId", Json::of(invite.sender));
            row.set("accountId", Json::of(std::to_string(player_account(invite.origin, invite.sender))));
            rows.items.push_back(std::move(row));
        }
    }
    // PSN sessions a remote friend reported for this PC's player; registers them as remote.
    Json remote_user_sessions(std::uint64_t account) {
        Json rows = Json::array();
        if (!friend_watcher || is_local(account)) return rows;
        const auto known = friend_watcher->by_account(account);
        if (!known || !known->online) return rows;
        if (const Json* reported = known->report.find("sessions"); reported && reported->is_array()) {
            for (const auto& row : reported->items) {
                const auto id = text_field(row, "sessionId");
                if (!lower_hex(id, 32) || session_store.contains(id)) continue;
                remote_sessions[id] = known->code;
                rows.items.push_back(row);
            }
        }
        return rows;
    }
    HttpAnswer http_answer(const std::string& method, const std::string& target,
                           const std::map<std::string, std::string>& headers, const Bytes& body,
                           const std::string& peer_address) {
        const bool verified_peer=starts_with(peer_address,"peer:");
        in_addr source{};
        const bool loopback=InetPtonA(AF_INET,peer_address.c_str(),&source)==1 && (ntohl(source.s_addr)>>24)==127;
        const bool public_route=(method=="POST" && (target=="/ctr/v1/peer" || target=="/ctr/v1/party")) ||
            (method=="GET" && (target=="/ctr/v1/identity" || starts_with(target,"/ctr/v1/identity?")));
        if(!verified_peer && !loopback && !public_route)
            return json_answer(403,R"({"error":"local service route"})","http_source_rejected",false);
        auto header = [&](const char* name) {
            const auto it = headers.find(name);
            return it == headers.end() ? std::string() : it->second;
        };
        const std::string host = lower(header("host").substr(0, header("host").find(':')));
        const std::string online_id = header("x-ctr-online-id");
        const std::string requester_code = starts_with(peer_address, "peer:") ? peer_address.substr(5) : own_code();
        const std::uint64_t requester_account = player_account(requester_code, online_id);
        // Without a room token the player uses public matchmaking.
        const std::string request_room = header("x-ctr-room-token").empty() ? public_room_token() : header("x-ctr-room-token");
        // A request a friend's service forwarded here is answered here, never forwarded again.
        if (!starts_with(peer_address, "peer:"))
            if (auto forwarded = forward_to_origin(method, target, header("content-type"), body, online_id)) return *forwarded;
        if (target == "/ctr/v1/session-invitations" ||
            starts_with(target, "/ctr/v1/session-invitations/")) {
            if (online_id.empty()) return json_answer(403, R"({"error":"project identity required"})", "game_invitation", false);
            try {
                const auto account = requester_account;
                const auto now = static_cast<std::uint64_t>(std::time(nullptr));
                std::unique_lock<std::mutex> lock(state);
                if (target == "/ctr/v1/session-invitations" && method == "GET") {
                    Json rows = session_store.invitations(account, now);
                    append_remote_invitations(rows, account, now);
                    return json_answer(200, dump_json(rows), "game_invitation_poll");
                }
                if (target == "/ctr/v1/session-invitations" && method == "POST") {
                    const auto request = parse_json(std::string(body.begin(), body.end()));
                    if (!request.is_object() || request.members.size() != 2 ||
                        !request.at("sessionId").is_string() || !request.at("recipients").is_array())
                        throw std::invalid_argument("invalid invitation request");
                    std::vector<std::uint64_t> recipients;
                    for (const auto& value : request.at("recipients").items) {
                        if (!value.is_unsigned()) throw std::invalid_argument("recipient must be an account ID");
                        recipients.push_back(value.as_u64());
                    }
                    const auto session = request.at("sessionId").text;
                    const Json sent = session_store.invite(account, online_id, session, recipients, now);
                    // Recipients playing on a friend's PC get the invitation through that PC's service.
                    std::vector<std::pair<std::string, Json>> deliveries;
                    for (const auto& row : sent.items) {
                        const auto recipient = std::stoull(row.at("accountId").text);
                        if (!friend_watcher || is_local(recipient)) continue;
                        const auto known = friend_watcher->by_account(recipient);
                        if (!known) continue;
                        Json notice = Json::object();
                        notice.set("action", Json::of("invitation"));
                        notice.set("invitation", row.at("invitationId"));
                        notice.set("session", Json::of(session));
                        notice.set("sender", Json::of(online_id));
                        notice.set("recipient", Json::of(recipient));
                        deliveries.emplace_back(known->code, std::move(notice));
                    }
                    lock.unlock();
                    HttpAnswer answer = json_answer(201, dump_json(sent), "game_invitation_send");
                    Json delivered = Json::array();
                    for (auto& [code, notice] : deliveries) {
                        Json result = Json::object();
                        result.set("friend", Json::of(code));
                        try { friend_watcher->request(code, std::move(notice), 4000); result.set("ok", Json::of(true)); }
                        catch (const std::exception& error) { result.set("ok", Json::of(false)); result.set("reason", Json::of(error.what())); }
                        delivered.items.push_back(std::move(result));
                    }
                    if (!delivered.items.empty()) answer.fields.set("delivered", std::move(delivered));
                    return answer;
                }
                if (starts_with(target, "/ctr/v1/session-invitations/") && method == "PUT")
                    return json_answer(200, dump_json(session_store.accept(account,
                        target.substr(std::string("/ctr/v1/session-invitations/").size()), now)), "game_invitation_accept");
                if (starts_with(target, "/ctr/v1/session-invitations/") && method == "DELETE") {
                    session_store.dismiss(account, target.substr(std::string("/ctr/v1/session-invitations/").size()), now);
                    return HttpAnswer{204, {}, "", "game_invitation_dismiss"};
                }
                return json_answer(405, R"({"error":"unsupported invitation operation"})", "game_invitation", false);
            } catch (const std::exception& error) {
                HttpAnswer answer = json_answer(400, R"({"error":"invalid invitation request"})", "game_invitation", false);
                answer.fields.set("reason", Json::of(error.what()));
                return answer;
            }
        }
        if (method == "GET" && target == "/ctr/v1/config") {
            Json payload = Json::object();
            payload.set("discovery_port", Json::of(static_cast<unsigned>(config.discovery_port)));
            payload.set("public_host", config.public_host.empty() ? Json::null() : Json::of(config.public_host));
            return json_answer(200, dump_json_spaced(payload), "config");
        }
        if (method == "GET" && starts_with(target, "/ctr/v1/peer-allowed?")) {
            const std::string ip = query_value(target.substr(target.find('?') + 1), "ip", "");
            const std::string& room = request_room;
            bool allowed = false;
            bool local = false;
            std::optional<MatchHub> hub;
            std::set<std::string> origins, joining;
            {
                std::lock_guard<std::mutex> lock(state);
                const auto requester = online_id.empty() ? live_clients.end() : live_clients.find(requester_account);
                local = requester != live_clients.end() && is_local(requester->first) && valid_room_token(room) &&
                        requester->second->user.room_token == room;
                if (local) {
                    allowed = connection_addresses(requester->first, room).count(ip) != 0;
                    if (const auto seat = hub_seats.find(requester->first); seat != hub_seats.end()) hub = seat->second;
                    for (const auto& [session, origin] : remote_sessions) origins.insert(origin);
                    const auto now = party_now();
                    for (auto it = delta_offers.begin(); it != delta_offers.end();)
                        it = it->second <= now ? delta_offers.erase(it) : std::next(it);
                    for (const auto& [code, until] : delta_offers) { origins.insert(code); joining.insert(code); }
                }
            }
            // Query the current authority outside state: no allow-all fallback on timeout.
            auto permits = [&](const Json& reply) {
                const auto* peers = reply.find("peers");
                if (peers && peers->is_array()) for (const auto& entry : peers->items) if (entry.text == ip) return true;
                return false;
            };
            if (local && !allowed && hub) {
                Json request = Json::object(); request.set("action", Json::of("mm_peers"));
                request.set("name", Json::of(online_id));
                try { allowed = permits(peer_request(*host_identity, hub->ip, hub->port, hub->code, request, 1500)); }
                catch (const std::exception&) {}
            }
            if (local && !allowed && friend_watcher) for (const auto& origin : origins) {
                if (!friend_watcher->is_friend(origin)) continue;
                Json request = Json::object(); request.set("action", Json::of("connection_peers"));
                request.set("name", Json::of(online_id));
                if (joining.count(origin)) request.set("joining", Json::of(true));
                try { allowed = permits(friend_watcher->request(origin, request, 1500)); }
                catch (const std::exception&) {}
                if (allowed) break;
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
            const std::string& room = request_room;
            std::uint64_t account = 0;
            std::string refusal;
            {
                std::lock_guard<std::mutex> lock(state);
                const auto requester = online_id.empty() ? live_clients.end() : live_clients.find(requester_account);
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
        if (method == "POST" && target == "/ctr/v1/peer") {
            try {
                if (body.size() > 65536) throw std::runtime_error("Messaggio troppo grande");
                const Json packet = parse_json(std::string(body.begin(), body.end()));
                const std::string caller = peer_message_sender(packet);
                const auto now = party_now();
                const Json request = verify_peer_message(packet, caller, now);
                const Json* to = request.find("to");
                if (!to || to->text != host_identity->friend_code()) throw std::runtime_error("Message for another service");
                const Json* nonce = request.find("nonce");
                if (!nonce || !peer_nonces.fresh(caller, nonce->text, now)) throw std::runtime_error("Messaggio duplicato");
                const Json* action = request.find("action");
                const std::string name = action ? action->text : std::string();
                // Public matchmaking (mm_*) is open to every signed service; the rest to friends.
                if (name.rfind("mm_", 0) != 0) {
                    if (!friend_watcher) throw std::runtime_error("Friend network not active");
                    if (!friend_watcher->is_friend(caller)) throw std::runtime_error("Not in the friends list");
                    if (name == "presence") {
                        const auto* port = request.find("contact_port");
                        const auto contact = port && port->is_unsigned() ? port->as_u64() : 8780;
                        if (contact > 0 && contact <= 65535)
                            friend_watcher->observe_endpoint(caller, peer_address, static_cast<std::uint16_t>(contact));
                    }
                }
                Json response = peer_action(caller, name, request, peer_address);
                response.set("nonce", Json::of(nonce->text));
                response.set("to", Json::of(caller));
                HttpAnswer a = json_answer(200, dump_json(sign_peer_message(*host_identity, response)), "peer_" + name);
                a.fields.set("from", Json::of(caller));
                return a;
            } catch (const std::exception& error) {
                Json response = Json::object();
                response.set("error", Json::of(error.what()));
                HttpAnswer a = json_answer(403, dump_json(response), "peer_rejected");
                a.fields.set("reason", Json::of(error.what()));
                return a;
            }
        }
        if (method == "POST" && target == "/ctr/v1/party") {
            if (!party) return json_answer(404, R"({"error":"Launcher lobby unavailable"})", "party_unavailable");
            try {
                if (body.size()>16384) throw std::runtime_error("Messaggio troppo grande");
                const Json packet=parse_json(std::string(body.begin(),body.end()));
                const std::string caller=packet.at("code").text;
                const auto now=party_now();
                Json request=verify_party_message(packet,caller,now);
                const std::string nonce=request.at("nonce").text;
                if (nonce.size()!=32 || nonce.find_first_not_of("0123456789abcdef")!=std::string::npos)
                    throw std::runtime_error("Invalid nonce");
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
                    response.set("wallet_backup", marketplace->snapshot(local_account(name->text)));
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
            const std::string& room = request_room;
            if (!valid_room_token(room)) return json_answer(400, R"({"error":"invalid project room token"})", "auth3_room_rejected", false);
            if (party && room != public_room_token() && !party->admits(room,online_id))
                return json_answer(403, R"({"error":"Join a launcher lobby first"})", "auth3_membership_rejected");
            Json request;
            try { request = parse_json(std::string(body.begin(), body.end())); } catch (const std::exception&) {
                return json_answer(400, R"({"error":"bad json"})", "auth3", false);
            }
            const auto multiplier_header = header("x-ctr-wumpa-multiplier");
            if (!multiplier_header.empty() && multiplier_header != "1" && multiplier_header != "100")
                return json_answer(400, R"({"error":"unsupported Wumpa multiplier"})", "auth3", false);
            Auth3Reply reply;
            // A wallet of this player from before v0.5.2, when the account was the name alone.
            if (!starts_with(peer_address, "peer:") && marketplace && !online_id.empty())
                try { marketplace->adopt(user_id(online_id), requester_account); } catch (const std::exception&) {}
            try { reply = auth3_reply(request, online_id, *key, requester_account); } catch (const std::exception&) {
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
                const std::uint64_t account = requester_account;
                Json payload;
                try {
                    std::lock_guard<std::mutex> lock(state);
                    payload = session_store.create(account, header("content-type"), body);
                    session_store.join(account, online_id, payload.at("sessionId").text);
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
            if (method == "PUT" && starts_with(path, "sdk:sessionInvitation/v1/sessions/") &&
                path.find('/', std::string("sdk:sessionInvitation/v1/sessions/").size()) == std::string::npos) {
                if (online_id.empty()) return json_answer(403, R"({"error":{"message":"project identity required"}})", "webapi_session_update", false);
                const std::string id = path.substr(std::string("sdk:sessionInvitation/v1/sessions/").size());
                try {
                    std::lock_guard<std::mutex> lock(state);
                    session_store.update(requester_account, id, body);
                } catch (const std::exception& error) {
                    HttpAnswer a = json_answer(400, R"({"error":{"message":"invalid session update"}})", "webapi_session_update", false);
                    a.fields.set("reason", Json::of(error.what()));
                    return a;
                }
                HttpAnswer a{204, {}, "", "webapi_session_update"};
                a.fields.set("session", Json::of(id));
                return a;
            }
            // Original Toolkit request templates use these routes for invitation-driven joining.
            const std::string session_route = path.substr(0, path.find('?'));
            const std::string session_prefix = "sdk:sessionInvitation/v1/sessions/";
            const std::string invite_prefix = "sdk:sessionInvitation/v1/users/me/invitations/";
            if (method == "GET" && session_route == "sdk:sessionInvitation/v1/users/me/invitations") {
                if (online_id.empty()) return json_answer(403, R"({"error":"identity required"})", "webapi_invitations", false);
                try {
                    const auto account = requester_account, now = static_cast<std::uint64_t>(std::time(nullptr));
                    Json rows = Json::array(), result = Json::object(), remote = Json::array();
                    {
                        std::lock_guard<std::mutex> lock(state);
                        const auto pending = session_store.invitations(account, now);
                        for (const auto& row : pending.items)
                            rows.items.push_back(session_store.invitation(account, row.at("invitationId").text, now));
                        append_remote_invitations(remote, account, now);
                    }
                    // Invitations kept on a friend's service: their details come from there.
                    for (const auto& row : remote.items) {
                        const auto reply = forward_to_origin("GET", "/webapi/sdk:sessionInvitation/v1/users/me/invitations/" +
                                                             row.at("invitationId").text, "", {}, online_id);
                        if (reply && reply->status == 200) rows.items.push_back(parse_json(reply->body));
                    }
                    result.set("size", Json::of(static_cast<unsigned>(rows.items.size())));
                    result.set("invitations", rows);
                    return json_answer(200, dump_json(result), "webapi_invitations");
                } catch (const std::exception&) {
                    return json_answer(400, R"({"error":"invalid invitation list"})", "webapi_invitations", false);
                }
            }
            if (method == "GET" && session_route == "sdk:sessionInvitation/v1/users/me/friends/sessions") {
                if (online_id.empty()) return json_answer(403, R"({"error":"identity required"})", "webapi_friend_sessions", false);
                try {
                    const auto query = path.find('?') == std::string::npos ? "" : path.substr(path.find('?') + 1);
                    const auto offset = python_int(query_value(query, "offset", "0"));
                    const auto limit = python_int(query_value(query, "limit", "100"));
                    if (!offset || !limit || *offset < 0 || *limit < 1 || *limit > 500)
                        throw std::invalid_argument("invalid pagination");
                    const auto friends = friends_listed() ? listed_friend_page(online_id, 0, 500) : friend_page(0, 500);
                    std::map<std::string, Json> found;
                    std::lock_guard<std::mutex> lock(state);
                    for (const auto& entry : friends.at("friendList").items) {
                        const auto& user = entry.at("user");
                        const auto friend_account = std::stoull(user.at("accountId").text);
                        Json listed = session_store.user_sessions(requester_account, friend_account);
                        for (auto& row : remote_user_sessions(friend_account).items) listed.items.push_back(std::move(row));
                        for (auto row : listed.items) {
                            const auto id = row.at("sessionId").text;
                            if (!found.count(id)) { row.set("friends", Json::array()); found.emplace(id, row); }
                            found.at(id).find("friends")->items.push_back(user);
                        }
                    }
                    Json rows = Json::array(), result = Json::object();
                    std::int64_t index = 0;
                    for (const auto& [id, row] : found)
                        if (index++ >= *offset && rows.items.size() < static_cast<std::size_t>(*limit)) rows.items.push_back(row);
                    result.set("size", Json::of(static_cast<unsigned>(rows.items.size())));
                    result.set("sessions", rows);
                    return json_answer(200, dump_json(result), "webapi_friend_sessions");
                } catch (const std::exception&) {
                    return json_answer(400, R"({"error":"invalid friend session list"})", "webapi_friend_sessions", false);
                }
            }
            if (method == "GET" && session_route == "sdk:sessionInvitation/v1/users/@users/sessions") {
                if (online_id.empty()) return json_answer(403, R"({"error":"identity required"})", "webapi_sessions", false);
                try {
                    const auto query = path.find('?') == std::string::npos ? "" : path.substr(path.find('?') + 1);
                    std::stringstream accounts(query_value(query, "accountIds", ""));
                    Json users = Json::array();
                    std::lock_guard<std::mutex> lock(state);
                    for (std::string text; std::getline(accounts, text, ',');) {
                        if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos || users.items.size() >= 100)
                            throw std::invalid_argument("invalid session accounts");
                        const auto account = std::stoull(text);
                        Json row = Json::object();
                        row.set("accountId", Json::of(std::to_string(account)));
                        Json listed = session_store.user_sessions(requester_account, account);
                        for (auto& remote : remote_user_sessions(account).items) listed.items.push_back(std::move(remote));
                        row.set("sessions", std::move(listed));
                        users.items.push_back(row);
                    }
                    Json result = Json::object(); result.set("users", users);
                    return json_answer(200, dump_json(result), "webapi_sessions");
                } catch (const std::exception&) {
                    return json_answer(400, R"({"error":"invalid session accounts"})", "webapi_sessions", false);
                }
            }
            if (starts_with(session_route, session_prefix) || starts_with(session_route, invite_prefix)) {
                if (online_id.empty()) return json_answer(403, R"({"error":"identity required"})", "webapi_session", false);
                try {
                    std::lock_guard<std::mutex> lock(state);
                    const auto account = requester_account;
                    if (starts_with(session_route, invite_prefix)) {
                        const auto id = session_route.substr(invite_prefix.size());
                        if (method == "GET" && id.ends_with("/invitationData")) {
                            session_store.invitation(account, id.substr(0, id.size() - 15),
                                static_cast<std::uint64_t>(std::time(nullptr)));
                            // Project dialog sends no invitation attachment. The actual payload is empty.
                            return HttpAnswer{200, {{"Content-Type", "application/octet-stream"}}, "", "webapi_invitation_data"};
                        }
                        if (method == "GET") return json_answer(200, dump_json(session_store.invitation(
                            account, id, static_cast<std::uint64_t>(std::time(nullptr)))), "webapi_invitation");
                        if (method == "PUT") {
                            const auto request = parse_json(std::string(body.begin(), body.end()));
                            if (request.members.size() != 1 || !request.at("usedFlag").is_bool() || !request.at("usedFlag").boolean)
                                throw std::invalid_argument("invalid invitation update");
                            session_store.accept(account, id, static_cast<std::uint64_t>(std::time(nullptr)));
                            return HttpAnswer{204, {}, "", "webapi_invitation_used"};
                        }
                        if (method == "DELETE") {
                            session_store.dismiss(account, id, static_cast<std::uint64_t>(std::time(nullptr)));
                            return HttpAnswer{204, {}, "", "webapi_invitation_dismiss"};
                        }
                    } else {
                        const auto tail = session_route.substr(session_prefix.size());
                        const auto slash = tail.find('/');
                        const auto id = tail.substr(0, slash);
                        const auto suffix = slash == std::string::npos ? "" : tail.substr(slash + 1);
                        if (method == "GET" && suffix.empty()) {
                            session_store.begin_connection(account, id, party_now());
                            return json_answer(200, dump_json(session_store.describe(account, id)), "webapi_session_details");
                        }
                        if (method == "GET" && (suffix == "sessionData" || suffix == "changeableSessionData" || suffix == "sessionImage")) {
                            const auto part = session_store.attachment(account, id, suffix == "sessionData" ? "session-data" :
                                suffix == "changeableSessionData" ? "changeable-session-data" : "session-image");
                            return HttpAnswer{200, {{"Content-Type", part.media}}, std::string(part.data.begin(), part.data.end()), "webapi_session_attachment"};
                        }
                        if (method == "PUT" && (suffix == "changeableSessionData" || suffix == "sessionImage")) {
                            session_store.replace_attachment(account, id, Part{
                                suffix == "changeableSessionData" ? "changeable-session-data" : "session-image",
                                header("content-type"), body});
                            return HttpAnswer{204, {}, "", "webapi_session_attachment_update"};
                        }
                        if (method == "POST" && suffix == "members") {
                            const auto query = path.find('?') == std::string::npos ? "" : path.substr(path.find('?') + 1);
                            const auto priority = python_int(query_value(query, "priority", "0"));
                            if (!priority || *priority < 0 || *priority > UINT32_MAX) throw std::invalid_argument("invalid member priority");
                            session_store.join(account, online_id, id);
                            session_store.member_priority(account, id, static_cast<std::uint64_t>(*priority));
                            return HttpAnswer{204, {}, "", "webapi_session_join"};
                        }
                        if (method == "PUT" && suffix == "members/me") {
                            const auto request = parse_json(std::string(body.begin(), body.end()));
                            if (request.members.size() != 1) throw std::invalid_argument("invalid member update");
                            session_store.member_priority(account, id, request.at("priority").as_u64());
                            return HttpAnswer{204, {}, "", "webapi_session_priority"};
                        }
                        if (method == "DELETE" && suffix == "members/me") {
                            session_store.leave(account, id);
                            return HttpAnswer{204, {}, "", "webapi_session_leave"};
                        }
                        if (method == "DELETE" && suffix.empty()) {
                            session_store.erase(account, id);
                            return HttpAnswer{204, {}, "", "webapi_session_delete"};
                        }
                    }
                } catch (const std::exception& error) {
                    auto answer = json_answer(400, R"({"error":"invalid session operation"})", "webapi_session", false);
                    answer.fields.set("reason", Json::of(error.what()));
                    return answer;
                }
            }
            // The API group contains a colon: split the query without interpreting a scheme.
            const std::string route = path.substr(0, path.find('?'));
            const std::string prefix = "sdk:userProfile/v1/users/", suffix = "/friendList";
            const auto presence_at = route.find("/presence", prefix.size());
            if (starts_with(route, prefix) && presence_at != std::string::npos) {
                if (online_id.empty()) return json_answer(403, R"({"error":"identity required"})", "webapi_presence", false);
                try {
                    const auto digits = route.substr(prefix.size(), presence_at - prefix.size());
                    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos)
                        throw std::invalid_argument("invalid presence account");
                    const auto account = std::stoull(digits);
                    const auto suffix_path = route.substr(presence_at + 9);
                    if (suffix_path != "" && suffix_path != "/inGamePresence" && suffix_path != "/gameData" && suffix_path != "/gameStatus")
                        throw std::invalid_argument("unsupported presence route");
                    std::lock_guard<std::mutex> lock(state);
                    if (method == "PUT" && !suffix_path.empty()) {
                        if (account != requester_account) throw std::invalid_argument("presence owner mismatch");
                        const auto value = parse_json(std::string(body.begin(), body.end()));
                        if (!value.is_object() || value.members.empty()) throw std::invalid_argument("invalid presence body");
                        for (const auto& [field_name, data] : value.members) {
                            if (field_name == "gameData") {
                                if (!data.is_string() || data.text.size() > 172 || base64_decode(data.text).size() > 128)
                                    throw std::invalid_argument("invalid gameData");
                            } else if (field_name == "gameStatus") {
                                if (!data.is_string() || data.text.size() > 191) throw std::invalid_argument("invalid gameStatus");
                            } else if (field_name == "localizedGameStatus") {
                                if (!data.is_array() || data.items.size() > 30) throw std::invalid_argument("invalid localized status");
                                for (const auto& localized : data.items)
                                    if (!localized.at("npLanguage").is_string() || localized.at("npLanguage").text.size() > 8 ||
                                        !localized.at("gameStatus").is_string() || localized.at("gameStatus").text.size() > 191)
                                        throw std::invalid_argument("invalid localized status entry");
                            } else throw std::invalid_argument("unsupported presence field");
                            if ((suffix_path == "/gameData" && field_name != "gameData") || (suffix_path == "/gameStatus" && field_name == "gameData"))
                                throw std::invalid_argument("presence field does not match endpoint");
                        }
                        auto& stored = game_presence[account];
                        if (!stored.is_object()) stored = Json::object();
                        for (const auto& [field_name, data] : value.members) stored.set(field_name, data);
                        return HttpAnswer{204, {}, "", "webapi_presence_update"};
                    }
                    if (method == "GET") {
                        Json context = Json::object();
                        context.set("gameStatus", Json::of("")); context.set("gameData", Json::of(""));
                        if (game_presence.count(account)) for (const auto& [field_name, value] : game_presence.at(account).members) context.set(field_name, value);
                        // A friend on another PC: presence as its own service last reported it.
                        std::optional<FriendState> remote;
                        if (friend_watcher && !is_local(account)) remote = friend_watcher->by_account(account);
                        if (remote)
                            if (const Json* saved = remote->report.find("game_presence"); saved && saved->is_object())
                                for (const auto& [field_name, value] : saved->members) context.set(field_name, value);
                        if (suffix_path == "/gameData" || suffix_path == "/gameStatus") {
                            Json result = Json::object(); const auto field_name = suffix_path.substr(1);
                            result.set(field_name, context.at(field_name)); return json_answer(200, dump_json(result), "webapi_presence_data");
                        }
                        Json title = Json::object(); title.set("npTitleId", Json::of("CUSA14876_00"));
                        title.set("titleName", Json::of("Crash Team Racing Nitro-Fueled")); context.set("gameTitleInfo", title);
                        Json presence = Json::object(), contexts = Json::array(), result = Json::object();
                        const bool active = is_local(account) || (remote && remote->online && remote->in_game);
                        presence.set("onlineStatus", Json::of(active ? "online" : "offline")); presence.set("platform", Json::of("PS4"));
                        if (active) contexts.items.push_back(context);
                        presence.set("incontextInfoList", contexts); result.set("presence", presence);
                        return json_answer(200, dump_json(result), "webapi_presence");
                    }
                } catch (const std::exception& error) {
                    auto answer = json_answer(400, R"({"error":"invalid presence operation"})", "webapi_presence", false);
                    answer.fields.set("reason", Json::of(error.what())); return answer;
                }
            }

            if (method == "GET" && route.size() > prefix.size() + suffix.size() && starts_with(route, prefix) &&
                route.compare(route.size() - suffix.size(), suffix.size(), suffix) == 0) {
                const std::string digits = route.substr(prefix.size(), route.size() - prefix.size() - suffix.size());
                if (digits.find_first_not_of("0123456789") == std::string::npos) {
                    const std::string stripped = digits.find_first_not_of('0') == std::string::npos ? "0" : digits.substr(digits.find_first_not_of('0'));
                    if (online_id.empty() || stripped != std::to_string(requester_account))
                        return json_answer(403, R"({"error":{"message":"project identity mismatch"}})", "webapi_friends", false);
                    const std::string query = path.find('?') == std::string::npos ? "" : path.substr(path.find('?') + 1);
                    const auto offset = python_int(query_value(query, "offset", "0"));
                    const auto limit = python_int(query_value(query, "limit", "500"));
                    Json payload;
                    try {
                        if (!offset || !limit) throw std::invalid_argument("invalid friend request");
                        payload = friends_listed() ? listed_friend_page(online_id, *offset, *limit) : friend_page(*offset, *limit);
                    } catch (const std::exception&) {
                        return json_answer(400, R"({"error":{"message":"invalid friend request"}})", "webapi_friends", false);
                    }
                    HttpAnswer a = json_answer(200, dump_json_spaced(payload), "webapi_friends");
                    a.fields.set("account", Json::of(requester_account));
                    a.fields.set("reply", payload);
                    return a;
                }
            }
            return json_answer(404, R"({"error":{"message":"not served by the CTR project server"}})", "webapi");
        }
        return json_answer(404, R"({"error":"not served"})", "http");
    }
    void handle_http(SOCKET s, const std::string& peer) {
        const auto header_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        std::string head;
        char c;
        while (head.size() < 4 || head.compare(head.size() - 4, 4, "\r\n\r\n") != 0) {
            if (head.size() >= kMaxHttpHead || !read_deadline(s,header_deadline) || recv(s, &c, 1, 0) != 1) return;
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
        if (!recv_exact(s, body, static_cast<std::size_t>(length),std::chrono::steady_clock::now()+std::chrono::seconds(5))) return;
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
        if (!client->hub_origin.empty()) {
            if (client->outbox.size() < 64) client->outbox.push_back(payload);
            return;
        }
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
            if (!party || client->user.room_token == public_room_token() || party->admits(client->user.room_token,client->user.online_id))
                available[account] = client->user.room_token;
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
        if (service == 12 && task == 10 && !params.empty() && params[0].kind == typed::UINT32 &&
            params.size() == 1 + params[0].value.as_u64()) {
            // getUserIDs: online IDs to account IDs, asked by the retail client for the friends of
            // its friend list (observed 2026-10-20: [u32 1][string "FriendA"]). One result per
            // name: u64 account, string online ID (layout as documented for the other port's
            // server, 12/9 and 12/10). The account of a friend comes from the friend's code
            // (player_account); a name that is not a friend keeps the name-only account.
            return guarded([&] {
                Writer w;
                const auto count = static_cast<std::uint32_t>(params.size() - 1);
                w.u32(count).u32(count);
                for (std::size_t i = 1; i < params.size(); ++i) {
                    if (params[i].kind != typed::STRING || params[i].value.text.empty()) throw std::invalid_argument("online ID expected");
                    w.u64(account_by_name(params[i].value.text, user)).string(params[i].value.text);
                }
                return task_reply(task, &w);
            });
        }
        if (service == 138 && task == 14 && !params.empty() && params[0].kind == typed::STRING) {
            // Friends' sessions of a context ("Delta"): the friend list page asks it with the
            // friends' account IDs. Record per session: blob 67-byte host address, blob 8-byte
            // session ID, u32 game type, u32 maximum players, u32 players, then the attribute
            // fields from the 138/1 attribute blob, without another blob wrapper.
            // Retail 1.21 0x401a00 reads the base record then an 8-byte security ID
            // at 0x401a7c, a 16-byte key, and the remaining typed fields directly.
            return guarded([&] {
                std::vector<std::uint64_t> accounts;
                for (std::size_t i = 1; i < params.size(); ++i) {
                    if (params[i].kind == typed::UINT64) accounts.push_back(params[i].value.as_u64());
                    else if (params[i].kind == typed::UNPARSED) {
                        // Typed u64 array: tag 110, typed u32 byte size, raw u32 count, raw u64 values.
                        const Bytes raw = from_hex(params[i].value.text);
                        if (raw.size() < 10 || raw[0] != 110 || raw[1] != typed::UINT32) throw std::invalid_argument("unsupported friend ID list");
                        std::size_t at = 6;
                        const std::uint32_t n = raw[at] | raw[at + 1] << 8 | raw[at + 2] << 16 | static_cast<std::uint32_t>(raw[at + 3]) << 24;
                        at += 4;
                        if (n > 500 || raw.size() < at + std::size_t{n} * 8) throw std::invalid_argument("friend ID list truncated");
                        for (std::uint32_t k = 0; k < n; ++k, at += 8) {
                            std::uint64_t v = 0;
                            for (int b = 7; b >= 0; --b) v = v << 8 | raw[at + static_cast<std::size_t>(b)];
                            accounts.push_back(v);
                        }
                    } else throw std::invalid_argument("unsupported friend ID parameter");
                }
                auto sessions = matchmaking.friend_sessions(params[0].value.text, accounts, user.user_id);
                // Friends playing on another PC: the Delta sessions their own service reported.
                for (const std::uint64_t account : accounts) {
                    if (!friend_watcher || account == user.user_id || is_local(account)) continue;
                    const auto known = friend_watcher->by_account(account);
                    const Json* delta = known && known->online ? known->report.find("delta") : nullptr;
                    if (!delta || !delta->is_array()) continue;
                    for (const auto& row : delta->items) {
                        if (text_field(row, "context") != params[0].value.text) continue;
                        delta_offers[known->code] = party_now() + 120;
                        sessions.push_back({from_hex(text_field(row, "address")), from_hex(text_field(row, "id")),
                                            static_cast<std::uint32_t>(row.at("game_type").as_u64()),
                                            static_cast<std::uint32_t>(row.at("max_players").as_u64()),
                                            static_cast<std::uint32_t>(row.at("players").as_u64()),
                                            from_hex(text_field(row, "custom"))});
                    }
                }
                Writer w;
                w.u32(static_cast<std::uint32_t>(sessions.size())).u32(static_cast<std::uint32_t>(sessions.size()));
                for (const auto& s : sessions)
                    w.blob(s.address).blob(s.id).u32(s.game_type).u32(s.max_players).u32(s.players).raw(s.custom);
                return task_reply(task, &w);
            });
        }
        if (auto answer = pitstop_task(request, user.user_id, *marketplace, *achievements, user.wumpa_multiplier)) return *answer;
        if (service == 4 && task == 1) return guarded([&] { stats->write(params, user.user_id); return task_reply(task); });
        if (service == 145 && task == 2)
            return guarded([&] { matchmaking.advertise(params, user.user_id, user.room_token); return task_reply(task); });
        if (service == 145 && task == 3 && params.empty())
            return guarded([&] { Writer w; w.u32(1).u32(1).u64(matchmaking.player_token(user.user_id)); return task_reply(task, &w); });
        if (service == 138 && task == 1)
            return guarded([&] { Writer w; w.u32(1).u32(1).blob(matchmaking.register_session(params, user.user_id, user.room_token)); return task_reply(task, &w); });
        if (service == 138 && task == 19)
            return guarded([&] { Writer w; w.u32(1).u32(1).blob(matchmaking.reserve_session(params, user.user_id, user.room_token)); return task_reply(task, &w); });
        if (service == 138 && (task == 20 || task == 2))
            return guarded([&] { matchmaking.update_session(params, user.user_id, task == 20); return task_reply(task); });
        if (service == 138 && task == 3)
            return guarded([&] { matchmaking.delete_session(params, user.user_id); return task_reply(task); });
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
                if (const auto forwarded = hub_task(request, payload, user)) {
                    send_lobby(*client, *forwarded, LSG_TASK_REPLY);
                    continue;
                }
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
            if (it != live_clients.end() && it->second == client) forget_client(client->user.user_id, "closed");
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

        const std::string local_host=starts_with(config.host,"127.") ? config.host : "127.0.0.1";
        SOCKET s;
        if (open("http/" + std::to_string(config.http_port), SOCK_STREAM, config.host, config.http_port, s))
            listener_threads.emplace_back([this, s] { accept_loop(s, [this](SOCKET c, std::string peer) { handle_http(c, peer); }); });
        if (open("lobby/tcp/" + std::to_string(config.lobby_port), SOCK_STREAM, local_host, config.lobby_port, s))
            listener_threads.emplace_back([this, s] {
                accept_loop(s, [this](SOCKET c, std::string peer) { handle_lobby(c, peer, peer.substr(0, peer.rfind(':'))); });
            });
        for (const std::uint16_t port : config.tcp_record_ports)
            if (open("tcp/" + std::to_string(port), SOCK_STREAM, local_host, port, s))
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
            if (open("udp/" + std::to_string(port), SOCK_DGRAM, local_host, port, s))
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
        if (config.friend_network) {
            const fs::path friends_path = config.state_dir / "friends.json";
            friend_watcher = std::make_unique<FriendWatcher>(*host_identity, friends_path,
                config.friend_resolver ? config.friend_resolver : rubrica_resolver(friends_path), config.friend_poll_period);
            friend_watcher->local_name = [this] { return local_player_name(); };
            friend_watcher->contact_port = config.http_port;
            friend_watcher->on_failure = [this](const std::string& code, const std::string& reason) {
                Json fields = Json::object();
                fields.set("friend", Json::of(code)); fields.set("reason", Json::of(reason));
                record("friend_presence_failed", std::move(fields));
            };
            friend_watcher->on_change = [this] {
                Json fields = Json::object(), rows = Json::array();
                for (const auto& known : friend_watcher->friends()) {
                    Json row = Json::object();
                    row.set("friend", Json::of(known.code)); row.set("online", Json::of(known.online));
                    row.set("in_game", Json::of(known.in_game)); row.set("address", Json::of(known.ip));
                    rows.items.push_back(std::move(row));
                }
                fields.set("friends", rows);
                record("friend_presence_changed", std::move(fields));
            };
            friend_watcher->start();
        }
        if (config.match_hub) hub_poller = std::thread([this] { hub_poll_loop(); });
        return listening;
    }
    void stop() {
        if (stopping.exchange(true)) return;
        if (friend_watcher) friend_watcher->stop();
        if (hub_poller.joinable()) hub_poller.join();
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
    std::size_t count = 0;
    for (const auto& [account, client] : impl_->live_clients) if (client->hub_origin.empty()) ++count;
    return count;
}
std::string Server::friend_code() const {
    return impl_->host_identity ? impl_->host_identity->friend_code() : std::string();
}
std::vector<FriendState> Server::friends() const {
    return impl_->friend_watcher ? impl_->friend_watcher->friends() : std::vector<FriendState>{};
}
void Server::record_event(const char* name, Json fields) { impl_->record(name,std::move(fields)); }
void Server::poll_friends() {
    if (impl_->friend_watcher) impl_->friend_watcher->poll_now();
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
