// SPDX-License-Identifier: GPL-2.0-or-later
// Project-server state, ported from tools/online local_matchmaking.py, local_stats.py,
// local_friends.py, local_sessions.py, local_marketplace.py, local_achievements.py and
// lobby_documents.py. Only the observed retail
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

// Pit Stop state (local_marketplace.py): Wumpa currency balances and inventory per account.
// Retail v01.21 requests: service 80 task 165 typed [STRING "octane_psn", UINT32 1, UINT32 856]
// (inventory page) and task 245 struct {1: "octane_psn", 2: max results} (balances).
// Nonempty item records verified by a retail purchase/unlock. Stored as text
// lines "balance <account> <currency> <amount>" / "inventory <account> <item> <quantity>"
// (the rows of the reference's two SQLite tables). The file is read again when it changes on
// disk, so balances set with ctr_online_server.exe --set-balance reach a running server.
class MarketplaceStore {
public:
    explicit MarketplaceStore(std::filesystem::path path);
    // Account-only backup; never exposes other players' balances or receipts.
    Json snapshot(std::uint64_t account);
    // Task 165: observed inventory record layout; rejects unsupported pagination.
    Bytes inventory_page(const std::vector<Param>& params, std::uint64_t account);
    void purchase(const std::vector<Param>& params, std::uint64_t account);
    // Task 245: struct reply message, repeated field 1 {1: currency, 2: zigzag amount}.
    Bytes balances_message(const std::vector<Param>& params, std::uint64_t account);
    // The client reads a currency ID as one byte (it fails the whole reply above 255) and
    // matches it against its local currency list: in retail v01.21 10 is "Wumpa Coins",
    // 11 "Nitro Boost", 12 "Free Nitro Boost". Amount 0 removes the balance.
    void set_balance(std::uint64_t account, std::uint64_t currency, std::int64_t amount);
    void set_inventory(std::uint64_t account, std::uint64_t item, std::int64_t quantity);
    // Moves every row of `from` to `to` when `to` has none (accounts before v0.5.2 were the
    // name alone). Returns whether anything moved.
    bool adopt(std::uint64_t from, std::uint64_t to);
    // Project rule: all end_race activities, track base x1/x100, common base32 if
    // unavailable. Unranked uses first-place base. Receipts prevent replay credit.
    void award_races(const std::vector<Param>& params, std::uint64_t account, unsigned multiplier = 1);
private:
    void load();
    void save();
    std::filesystem::path path_;
    std::filesystem::file_time_type loaded_{};
    std::map<std::pair<std::string, std::uint64_t>, std::int64_t> balances_, inventory_;
    std::set<std::pair<std::string, std::string>> rewarded_;
};

// Wumpa Challenge state (local_achievements.py), achievement engine service 125, struct
// requests in the "octane_psn" context: task 1 event reports (recorded, acknowledged) and
// task 3 achievement states of one kind 0..7 (always empty: no challenge schedule yet).
// Events are appended as JSON lines {"account", "received", "event"}, the columns of the
// reference's events table; "event" holds the same json.dumps text.
class AchievementStore {
public:
    explicit AchievementStore(std::filesystem::path path);
    Json report_events(const std::vector<Param>& params, std::uint64_t account); // the stored events
    Bytes states_message(const std::vector<Param>& params, std::uint64_t account) const;
private:
    std::filesystem::path path_;
};

// Replies of the Pit Stop and Wumpa Challenge tasks (80/165, 80/245, 125/1, 125/3), as
// demonware_tasks.handle; nullopt for other tasks. Rejected requests get UNSERVED_ERROR.
std::optional<Bytes> pitstop_task(const TaskRequest& request, std::uint64_t account,
                                  MarketplaceStore& marketplace, AchievementStore& achievements, unsigned multiplier = 1);

// Project friend graph: always empty (no social service). Nonempty pages are not served.
Json friend_page(std::int64_t offset, std::int64_t limit);

class SessionStore {
public:
    // Returns {"sessionId": ...}.
    Json create(std::uint64_t account, const std::string& content_type, const Bytes& body);
    // PUT /v1/sessions/{id} by the session's owner (observed: {"sessionPrivacy":"public"}).
    void update(std::uint64_t account, const std::string& id, const Bytes& body);
    // Project-owned invitation transport; polling does not accept or consume an invite.
    Json invite(std::uint64_t account, const std::string& sender, const std::string& session,
                const std::vector<std::uint64_t>& recipients, std::uint64_t now);
    Json invitations(std::uint64_t recipient, std::uint64_t now);
    void dismiss(std::uint64_t recipient, const std::string& invitation, std::uint64_t now);
    Json accept(std::uint64_t recipient, const std::string& invitation, std::uint64_t now);
    Json invitation(std::uint64_t recipient, const std::string& id, std::uint64_t now);
    Json describe(std::uint64_t account, const std::string& id) const;
    Part attachment(std::uint64_t account, const std::string& id, const std::string& name) const;
    void join(std::uint64_t account, const std::string& name, const std::string& id);
    void leave(std::uint64_t account, const std::string& id);
    void erase(std::uint64_t account, const std::string& id);
    void replace_attachment(std::uint64_t account, const std::string& id, Part part);
    Json user_sessions(std::uint64_t requester, std::uint64_t account) const;
    void member_priority(std::uint64_t account, const std::string& id, std::uint64_t priority);
    bool contains(const std::string& id) const { return sessions_.count(id) > 0; }
    // A readable session may be contacted before retail posts /members.
    void begin_connection(std::uint64_t account, const std::string& id, std::uint64_t now);
    std::set<std::uint64_t> connection_peers(std::uint64_t account, std::uint64_t now) const;
private:
    struct Session {
        std::uint64_t owner; Json metadata; std::vector<Part> parts;
        std::map<std::uint64_t, std::string> members;
        std::set<std::uint64_t> admitted;
        std::map<std::uint64_t, std::uint64_t> priorities;
    };
    std::map<std::string, Session> sessions_;
    std::map<std::pair<std::string, std::uint64_t>, std::uint64_t> connecting_;
    struct Invitation { std::string session, sender; std::uint64_t account, recipient, expires; bool accepted = false; };
    std::map<std::string, Invitation> invitations_;
    void expire_invitations(std::uint64_t now);
    const Session& readable(std::uint64_t account, const std::string& id) const;
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
    // 138/19 reserves a session ID in a context (observed: a private lobby being created);
    // 138/20 initializes it and 138/2 updates it with host address, type, size and attributes;
    // 138/3 deletes it. Only the owner may change or delete a session.
    Bytes reserve_session(const std::vector<Param>& params, std::uint64_t account, const std::string& selector);
    void update_session(const std::vector<Param>& params, std::uint64_t account, bool initialize);
    void delete_session(const std::vector<Param>& params, std::uint64_t account);
    // 138/14: the sessions that the given friends registered in a context (e.g. "Delta"),
    // other than the requester's own. Any room: friends may play public or private games.
    struct FriendSession { Bytes address, id; std::uint32_t game_type, max_players, players; Bytes custom; };
    std::vector<FriendSession> friend_sessions(const std::string& context, const std::vector<std::uint64_t>& accounts,
                                               std::uint64_t requester) const;
    // The published (not reserved) sessions of one account, with their context, for the
    // presence a friend's service caches and answers 138/14 from.
    std::vector<std::pair<std::string, FriendSession>> owned_sessions(std::uint64_t account) const;
    std::uint64_t start_search(const std::vector<Param>& params, std::uint64_t account, const std::string& selector);
    // Selects connected pending players with equal room and retail filters. The host is the
    // first searcher among `reachable` (players whose PC accepts connections from the
    // Internet), else the first searcher.
    std::vector<LobbyPlan> match_ready(const std::map<std::uint64_t, std::string>& available,
                                       const std::map<std::string, std::size_t>& expected = {},
                                       const std::set<std::uint64_t>& reachable = {});
    void save_documents(std::uint64_t lobby, const Json& host, const Json& backend, std::uint64_t update_id);
    bool sync_document(const std::vector<Param>& params, std::uint64_t account, const std::string& selector);
    // Preserve the existing race documents/keys while electing a connected member.
    std::vector<LobbyPlan> promote_disconnected(std::uint64_t lost,
        const std::map<std::uint64_t, std::string>& connected);
    void cancel_pending(std::uint64_t account, const std::string& selector);
    // 145/10: `account` leaves `lobby`. When it was the host, the next connected member of the
    // lobby is promoted and returned (the remaining players must be told).
    std::optional<std::uint64_t> leave_lobby(std::uint64_t account, std::uint64_t lobby,
        const std::map<std::uint64_t, std::string>& connected);
    // 145/8: `account` cancels its search `search_id` (the mm_id answered by 145/18).
    void cancel_search(std::uint64_t account, std::uint64_t search_id);
    bool departed(std::uint64_t lobby, std::uint64_t account) const;
    const std::string* player_info(std::uint64_t account) const;
    const LobbyDocuments* documents(std::uint64_t lobby) const;
    std::set<std::uint64_t> connection_peers(std::uint64_t account, const std::string& selector) const;

private:
    std::string migration_routes(std::uint64_t lobby) const;
    struct Lobby;
    std::optional<std::uint64_t> promote(std::uint64_t id, Lobby& lobby, std::uint64_t lost,
        const std::map<std::uint64_t, std::string>& connected);
    struct MatchPlayer { Bytes address; std::uint32_t a, b; Bytes custom; };
    struct MatchSession {
        std::string selector, context;
        std::uint64_t owner;
        MatchPlayer self;               // host address, game type (a), maximum players (b), attributes
        std::uint32_t players = 1;
        bool reserved = false;          // ID handed out by 138/19, no game published yet
    };
    struct Search {
        std::uint64_t id, account;
        std::string selector, compatibility, document, status;
        std::optional<std::uint64_t> lobby;
    };
    struct Lobby { std::string selector; std::uint64_t host; std::vector<std::uint64_t> members; std::set<std::uint64_t> departed; };
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
