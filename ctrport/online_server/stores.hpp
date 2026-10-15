// SPDX-License-Identifier: GPL-2.0-or-later
// Project-server state, ported from tools/online local_matchmaking.py, local_stats.py,
// local_friends.py, local_sessions.py and lobby_documents.py. Only the observed retail
// schemas are accepted; every other input throws (the reference server's ValueError) before
// any state changes. Matchmaking, lobbies and sessions live in memory for the life of the
// server (a restart drops them, as every connected game has to log in again anyway);
// statistics are written to a file. Callers serialize access (Server holds one lock).
#pragma once
#include "json.hpp"
#include "protocol.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ctr_online {
class StatsStore {
public:
    explicit StatsStore(std::filesystem::path path);
    // Replacement writes (mode 0) with the observed four typed fields only.
    void write(const std::vector<Param>& params, std::uint64_t account);
    std::size_t size() const { return ratings_.size(); }
private:
    void save() const;
    std::filesystem::path path_;
    std::map<std::pair<std::uint32_t, std::string>, std::int64_t> ratings_;
};

// Project friend graph: always empty (no social service). Nonempty pages are not served.
Json friend_page(std::int64_t offset, std::int64_t limit);

class SessionStore {
public:
    // Returns {"sessionId": ...}.
    Json create(std::uint64_t account, const std::string& content_type, const Bytes& body);
private:
    struct Session { std::uint64_t owner; Json metadata; std::vector<Part> parts; };
    std::map<std::string, Session> sessions_;
};

struct PlanPlayer { std::uint64_t account; std::uint64_t search_id; Json search; };
struct LobbyPlan {
    std::uint64_t lobby_id = 0;
    std::string selector;
    std::uint64_t host = 0;
    std::vector<PlanPlayer> players;
    std::set<std::uint64_t> guests_notified;
};
struct LobbyDocuments { Json host, backend; std::uint64_t update_id = 0; };

class MatchmakingStore {
public:
    void advertise(const std::vector<Param>& params, std::uint64_t account, const std::string& selector);
    std::uint64_t player_token(std::uint64_t account);
    Bytes register_session(const std::vector<Param>& params, std::uint64_t account, const std::string& selector);
    void update_session_players(const std::vector<Param>& params, std::uint64_t account, const std::string& selector);
    std::uint64_t start_search(const std::vector<Param>& params, std::uint64_t account, const std::string& selector);
    // Selects connected pending players with equal room and retail filters.
    std::vector<LobbyPlan> match_ready(const std::map<std::uint64_t, std::string>& available);
    void save_documents(std::uint64_t lobby, const Json& host, const Json& backend, std::uint64_t update_id);
    bool sync_document(const std::vector<Param>& params, std::uint64_t account, const std::string& selector);
    void cancel_pending(std::uint64_t account, const std::string& selector);
    const std::string* player_info(std::uint64_t account) const;
    const LobbyDocuments* documents(std::uint64_t lobby) const;

private:
    struct MatchPlayer { Bytes address; std::uint32_t a, b; Bytes custom; };
    struct MatchSession {
        std::string selector, context;
        std::uint64_t owner;
        MatchPlayer self;
        std::vector<MatchPlayer> players;
    };
    struct Search {
        std::uint64_t id, account;
        std::string selector, compatibility, document, status;
        std::optional<std::uint64_t> lobby;
    };
    struct Lobby { std::string selector; std::uint64_t host; std::vector<std::uint64_t> members; };
    std::map<std::uint64_t, std::string> player_info_, player_rooms_;
    std::map<std::uint64_t, std::uint64_t> player_tokens_;
    std::set<std::uint64_t> token_values_;
    std::map<Bytes, MatchSession> sessions_;
    std::vector<Search> searches_; // insertion order (the reference's rowid)
    std::map<std::uint64_t, Lobby> lobbies_;
    std::map<std::uint64_t, LobbyDocuments> documents_;
};

// Lobby documents for a new lobby from the host's own advertisement.
std::pair<Json, Json> lobby_documents(const LobbyPlan& plan, const Json& advertisement, std::uint64_t now);
constexpr int ROOM_CREATED_EVENT = 2005, ROOM_JOIN_EVENT = 2001, ROOM_UPDATED_EVENT = 2006, PUSH_RECORD = 2;
Bytes room_create_event(std::uint64_t search_id, std::uint64_t lobby, const Json& host, const Json& backend);
Bytes room_join_event(std::uint64_t search_id, std::uint64_t lobby, const Json& host, const Json& backend);
Bytes room_update_event(std::uint64_t lobby, const Json& host, const Json& backend);
} // namespace ctr_online
