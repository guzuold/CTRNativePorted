// SPDX-License-Identifier: GPL-2.0-or-later
#include "stores.hpp"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <initializer_list>
#include <cstdlib>

namespace ctr_online {
namespace {
struct Rejected : std::invalid_argument { using std::invalid_argument::invalid_argument; };

std::vector<int> kinds(const std::vector<Param>& params, std::size_t from = 0, std::size_t count = std::string::npos) {
    std::vector<int> out;
    for (std::size_t i = from; i < params.size() && i - from < count; ++i) out.push_back(params[i].kind);
    return out;
}
bool keys_are(const Json& value, std::initializer_list<const char*> keys) {
    if (!value.is_object() || value.members.size() != keys.size()) return false;
    for (const char* key : keys) if (!value.find(key)) return false;
    return true;
}
// Python `a == n` for a JSON value against an integer (ints, integral floats and bools compare).
bool equals_int(const Json& value, std::int64_t n) {
    if (value.kind == Json::Kind::Uint) return n >= 0 && value.unsigned_value == static_cast<std::uint64_t>(n);
    if (value.kind == Json::Kind::Int) return value.integer == n;
    if (value.kind == Json::Kind::Double) return value.number == static_cast<double>(n);
    if (value.kind == Json::Kind::Bool) return (value.boolean ? 1 : 0) == n;
    return false;
}
bool equals_u64(const Json& value, std::uint64_t n) {
    if (value.kind == Json::Kind::Uint) return value.unsigned_value == n;
    if (n <= 0x7fffffffffffffffull) return equals_int(value, static_cast<std::int64_t>(n));
    return value.kind == Json::Kind::Double && value.number == static_cast<double>(n);
}
std::int64_t signed_of(const Json& value) {
    if (value.kind == Json::Kind::Int) return value.integer;
    if (value.unsigned_value > 0x7fffffffffffffffull) return INT64_MAX;
    return static_cast<std::int64_t>(value.unsigned_value);
}
std::size_t utf8_size(const std::string& text) { return text.size(); }
const std::string& text_of(const Json& value) {
    if (!value.is_string()) throw Rejected("expected a string");
    return value.text;
}
Json single(const char* key, Json value) { Json o = Json::object(); o.set(key, std::move(value)); return o; }
Json strip_bom_parse(const Bytes& data) {
    std::string text(data.begin(), data.end());
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
    return parse_json(text);
}
} // namespace

StatsStore::StatsStore(std::filesystem::path path) : path_(std::move(path)) {
    std::ifstream in(path_);
    std::uint32_t board;
    std::string account;
    std::int64_t rating;
    while (in >> board >> account >> rating) ratings_[{board, account}] = rating;
}
void StatsStore::save() const {
    std::filesystem::create_directories(path_.parent_path());
    const std::filesystem::path temporary = path_.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        for (const auto& [key, rating] : ratings_) out << key.first << ' ' << key.second << ' ' << rating << '\n';
    }
    std::filesystem::rename(temporary, path_);
}
void StatsStore::write(const std::vector<Param>& params, std::uint64_t account) {
    if (params.empty() || params.size() % 4) throw Rejected("unsupported statistics record length");
    std::vector<std::pair<std::pair<std::uint32_t, std::string>, std::int64_t>> records;
    for (std::size_t i = 0; i < params.size(); i += 4) {
        if (kinds(params, i, 4) != std::vector<int>{typed::UINT32, typed::UINT64, typed::UINT8, typed::INT64}) throw Rejected("unsupported statistics field types");
        const Json &board = params[i].value, &entity = params[i + 1].value, &mode = params[i + 2].value, &rating = params[i + 3].value;
        if (entity.as_u64() != account || mode.as_u64() != 0 || board.as_u64() > 0xffffffffull)
            throw Rejected("statistics account or write mode rejected");
        records.push_back({{static_cast<std::uint32_t>(board.as_u64()), std::to_string(entity.as_u64())}, rating.as_i64()});
    }
    for (const auto& [key, value] : records) ratings_[key] = value;
    save();
}

Json friend_page(std::int64_t offset, std::int64_t limit) {
    if (offset < 0 || limit < 1 || limit > 500) throw Rejected("invalid friend pagination");
    Json page = Json::object();
    page.set("totalResults", Json::of(0));
    page.set("size", Json::of(0));
    page.set("friendList", Json::array());
    return page;
}

Json SessionStore::create(std::uint64_t account, const std::string& content_type, const Bytes& body) {
    std::vector<Part> parts = parse_session_parts(content_type, body);
    static const std::set<std::string> allowed = {"session-request", "session-image", "session-data", "changeable-session-data"};
    std::set<std::string> names;
    for (const Part& p : parts) names.insert(p.name);
    if (parts.empty() || parts[0].name != "session-request" || names.size() != parts.size() ||
        !std::all_of(names.begin(), names.end(), [](const std::string& n) { return allowed.count(n) > 0; }))
        throw Rejected("unsupported session parts");
    const Json metadata = strip_bom_parse(parts[0].data);
    const Json* privacy = metadata.find("sessionPrivacy");
    const Json* max_user = metadata.find("sessionMaxUser");
    const Json* name = metadata.find("sessionName");
    const Json* platforms = metadata.find("availablePlatforms");
    Json ps4 = Json::array();
    ps4.items.push_back(Json::of("PS4"));
    if (!metadata.is_object() || !privacy || !privacy->is_string() || (privacy->text != "private" && privacy->text != "public") ||
        !max_user || !max_user->is_integer() || signed_of(*max_user) < 1 || signed_of(*max_user) > 8 ||
        !name || !name->is_string() || !platforms || *platforms != ps4)
        throw Rejected("unsupported session metadata");
    const std::string id = uuid4_hex();
    parts.erase(parts.begin());
    sessions_[id] = Session{account, metadata, std::move(parts)};
    return single("sessionId", Json::of(id));
}

void MatchmakingStore::advertise(const std::vector<Param>& params, std::uint64_t account, const std::string& selector) {
    if (!valid_room_token(selector)) throw Rejected("valid room selector required");
    if (params.size() != 1 || params[0].kind != typed::STRING || !params[0].value.is_string()) throw Rejected("expected one player-info string");
    const std::string& text = params[0].value.text;
    if (utf8_size(text) > 65535) throw Rejected("oversize player-info document");
    const Json document = parse_json(text);
    if (!keys_are(document, {"listen_server", "dedicated_server"})) throw Rejected("unsupported player-info schema");
    const Json& listen = document.at("listen_server");
    if (!keys_are(listen, {"local_address", "security_id", "security_key", "nat"})) throw Rejected("unsupported listen-server schema");
    for (const auto& [key, size] : {std::pair<const char*, std::size_t>{"local_address", 67}, {"security_id", 8}, {"security_key", 16}})
        if (base64_decode(text_of(listen.at(key))).size() != size) throw Rejected("invalid player-info encoding");
    const Json& nat = listen.at("nat");
    if (!nat.is_integer() || !(equals_int(nat, 1) || equals_int(nat, 2) || equals_int(nat, 3))) throw Rejected("unsupported NAT value");
    const Json& dedicated = document.at("dedicated_server");
    if (!keys_are(dedicated, {"build_name"}) || !dedicated.at("build_name").is_string()) throw Rejected("unsupported dedicated-server schema");
    player_info_[account] = text;
    player_rooms_[account] = selector;
}
std::uint64_t MatchmakingStore::player_token(std::uint64_t account) {
    if (!player_info_.count(account)) throw Rejected("player advertisement required");
    if (const auto it = player_tokens_.find(account); it != player_tokens_.end()) return it->second;
    while (true) {
        const std::uint64_t token = random_u64();
        if (token && !token_values_.count(token)) {
            player_tokens_[account] = token;
            token_values_.insert(token);
            return token;
        }
    }
}
namespace {
Bytes hex_field(const Json& value) { return from_hex(text_of(value)); }
std::uint32_t u32_field(const Json& value) {
    if (!value.is_unsigned() || value.as_u64() > 0xffffffffull) throw Rejected("invalid 32-bit field");
    return static_cast<std::uint32_t>(value.as_u64());
}
} // namespace
Bytes MatchmakingStore::register_session(const std::vector<Param>& params, std::uint64_t account, const std::string& selector) {
    if (!valid_room_token(selector) || params.size() != 5 || kinds(params) != std::vector<int>{typed::STRING, typed::BLOB, typed::UINT32, typed::UINT32, typed::BLOB})
        throw Rejected("unsupported session registration schema");
    const std::string& context = text_of(params[0].value);
    if (context.empty() || context.size() > 16) throw Rejected("invalid session context");
    const Bytes address = hex_field(params[1].value), custom = hex_field(params[4].value);
    if (address.size() != 67 || custom.empty() || custom.size() > 65535) throw Rejected("invalid session fields");
    const std::uint32_t a = u32_field(params[2].value), b = u32_field(params[3].value);
    while (true) {
        const Bytes session = random_bytes(8);
        if (std::any_of(session.begin(), session.end(), [](unsigned char c) { return c != 0; }) && !sessions_.count(session)) {
            sessions_[session] = MatchSession{selector, context, account, MatchPlayer{address, a, b, custom}, {}};
            return session;
        }
    }
}
void MatchmakingStore::update_session_players(const std::vector<Param>& params, std::uint64_t account, const std::string& selector) {
    if (!valid_room_token(selector) || params.size() < 3 || kinds(params, 0, 3) != std::vector<int>{typed::STRING, typed::BLOB, typed::UINT32})
        throw Rejected("invalid session player header");
    const std::string& context = text_of(params[0].value);
    const Bytes session = hex_field(params[1].value);
    const Json& count_value = params[2].value;
    const std::uint64_t count = count_value.as_u64();
    if (count < 1 || count > 8 || params.size() != 3 + count * 4 || session.size() != 8) throw Rejected("invalid session player count");
    const auto it = sessions_.find(session);
    if (it == sessions_.end() || it->second.owner != account || it->second.selector != selector || it->second.context != context)
        throw Rejected("session owner or room mismatch");
    std::vector<MatchPlayer> rows;
    for (std::size_t position = 0; position < count; ++position) {
        const std::size_t base = 3 + position * 4;
        if (kinds(params, base, 4) != std::vector<int>{typed::BLOB, typed::UINT32, typed::UINT32, typed::BLOB}) throw Rejected("invalid session player fields");
        const Bytes address = hex_field(params[base].value), custom = hex_field(params[base + 3].value);
        if (address.size() != 67 || custom.empty() || custom.size() > 65535) throw Rejected("invalid session player values");
        rows.push_back(MatchPlayer{address, u32_field(params[base + 1].value), u32_field(params[base + 2].value), custom});
    }
    it->second.players = std::move(rows);
}
std::uint64_t MatchmakingStore::start_search(const std::vector<Param>& params, std::uint64_t account, const std::string& selector) {
    if (params.size() != 1 || params[0].kind != typed::STRING) throw Rejected("expected one search document");
    Json document = parse_json(text_of(params[0].value));
    if (!keys_are(document, {"members", "tokens", "lobby_slots", "dedicated_server", "team_balance", "ruleset_payload",
                             "player_id_to_slots", "performance"}))
        throw Rejected("unsupported search schema");
    // First verified scope: one local player per instance, no guests or teams.
    const Json& members = document.at("members");
    if (!members.is_array() || members.items.size() != 1 || !equals_u64(members.items[0], account)) throw Rejected("search member or token mismatch");
    const Json& tokens = document.at("tokens");
    const std::uint64_t token = player_token(account);
    if (!tokens.is_array() || tokens.items.size() != 1 || !equals_u64(tokens.items[0], token)) throw Rejected("search member or token mismatch");
    const auto room = player_rooms_.find(account);
    if (!valid_room_token(selector) || room == player_rooms_.end() || room->second != selector) throw Rejected("search room mismatch");
    const Json& slots = document.at("lobby_slots");
    if (!keys_are(slots, {"min", "max"}) || !slots.at("min").is_integer() || !slots.at("max").is_integer() ||
        !(2 <= signed_of(slots.at("min")) && signed_of(slots.at("min")) <= signed_of(slots.at("max")) && signed_of(slots.at("max")) <= 8))
        throw Rejected("unsupported lobby slots");
    Json slot_map = Json::object();
    slot_map.set(std::to_string(account), Json::of(0));
    if (document.at("dedicated_server") != single("request_level", Json::of("not_needed")) ||
        document.at("team_balance") != single("num_teams", Json::of(0)) || document.at("player_id_to_slots") != slot_map)
        throw Rejected("unsupported dedicated server, team or guest schema");
    const Json& filters = document.at("ruleset_payload");
    const Json& performance = document.at("performance");
    bool filters_ok = keys_are(filters, {"filter"}) && keys_are(filters.at("filter"), {"matchmaking_version", "playlist_id"});
    if (filters_ok)
        for (const auto& [name, value] : filters.at("filter").members)
            if (!value.is_integer() || value.kind == Json::Kind::Int) filters_ok = false;
    if (!filters_ok || performance != single("game_type", Json::of(1))) throw Rejected("unsupported search filters");
    Json compatibility_list = Json::array();
    compatibility_list.items = {slots, filters, performance};
    const std::string compatibility = dump_json_sorted(compatibility_list);
    const std::string text = dump_json(document);
    for (const Search& s : searches_)
        if (s.account == account && s.selector == selector && s.document == text && s.status == "pending") return s.id;
    for (Search& s : searches_)
        if (s.account == account && s.status == "pending") s.status = "cancelled";
    while (true) {
        const std::uint64_t id = random_bits(52);
        if (id && std::none_of(searches_.begin(), searches_.end(), [&](const Search& s) { return s.id == id; })) {
            searches_.push_back(Search{id, account, selector, compatibility, text, "pending", std::nullopt});
            return id;
        }
    }
}
std::vector<LobbyPlan> MatchmakingStore::match_ready(const std::map<std::uint64_t, std::string>& available) {
    std::vector<std::pair<std::pair<std::string, std::string>, std::vector<Search*>>> groups;
    for (Search& s : searches_) {
        if (s.status != "pending") continue;
        const auto it = available.find(s.account);
        if (it == available.end() || it->second != s.selector) continue;
        const auto key = std::make_pair(s.selector, s.compatibility);
        auto group = std::find_if(groups.begin(), groups.end(), [&](const auto& g) { return g.first == key; });
        if (group == groups.end()) { groups.push_back({key, {}}); group = groups.end() - 1; }
        group->second.push_back(&s);
    }
    std::vector<LobbyPlan> plans;
    for (auto& [key, players] : groups) {
        const Json slots = parse_json(players[0]->document).at("lobby_slots");
        std::int64_t min_players = signed_of(slots.at("min"));
#pragma warning(suppress: 4996)
        if (const char* env = std::getenv("CTR_ONLINE_MIN_PLAYERS")) {
            try { min_players = std::max<std::int64_t>(min_players, std::stoll(env)); } catch (...) {}
        }
        if (static_cast<std::int64_t>(players.size()) < min_players) continue;
        if (static_cast<std::int64_t>(players.size()) > signed_of(slots.at("max"))) players.resize(static_cast<std::size_t>(signed_of(slots.at("max"))));
        std::uint64_t lobby;
        do lobby = random_bits(52); while (!lobby || lobbies_.count(lobby));
        LobbyPlan plan;
        plan.lobby_id = lobby;
        plan.selector = key.first;
        Lobby record{key.first, players[0]->account, {}};
        for (Search* player : players) {
            record.members.push_back(player->account);
            player->status = "paired";
            player->lobby = lobby;
            plan.players.push_back(PlanPlayer{player->account, player->id, parse_json(player->document)});
        }
        plan.host = record.host;
        lobbies_[lobby] = std::move(record);
        plans.push_back(std::move(plan));
    }
    return plans;
}
void MatchmakingStore::save_documents(std::uint64_t lobby, const Json& host, const Json& backend, std::uint64_t update_id) {
    documents_[lobby] = LobbyDocuments{host, backend, update_id};
}
bool MatchmakingStore::sync_document(const std::vector<Param>& params, std::uint64_t account, const std::string& selector) {
    if (params.size() != 3 || kinds(params) != std::vector<int>{typed::UINT64, typed::UINT64, typed::STRING}) throw Rejected("invalid lobby synchronization fields");
    const std::uint64_t lobby = params[0].value.as_u64(), expected = params[1].value.as_u64();
    const auto row = lobbies_.find(lobby);
    const auto current = documents_.find(lobby);
    if (row == lobbies_.end() || row->second.host != account || row->second.selector != selector || current == documents_.end())
        throw Rejected("lobby host or room mismatch");
    if (current->second.update_id != expected) return false;
    Json document = parse_json(text_of(params[2].value));
    static const std::set<std::string> keys = {"update_time", "update_id", "lobby_open", "lobby_open_for_pres_join", "player_state",
                                               "listen_server", "team_balance", "ruleset_payload", "attachment"};
    if (!document.is_object()) throw Rejected("unsupported host document schema");
    for (const std::string& key : keys) if (!document.find(key)) throw Rejected("unsupported host document schema");
    for (const auto& [name, value] : document.members)
        if (!keys.count(name) && name != "game_id") throw Rejected("unsupported host document schema");
    const Json& update_time = document.at("update_time");
    if (!equals_u64(document.at("update_id"), expected) || !update_time.is_integer() || update_time.kind == Json::Kind::Int ||
        !document.at("lobby_open").is_bool() || !document.at("lobby_open_for_pres_join").is_bool())
        throw Rejected("invalid host document version or flags");
    const Json& state = document.at("player_state");
    std::set<std::string> members;
    for (const std::uint64_t m : row->second.members) members.insert(std::to_string(m));
    std::set<std::string> state_keys;
    if (state.is_object()) for (const auto& [name, value] : state.members) state_keys.insert(name);
    bool state_ok = state.is_object() && state_keys == members;
    if (state_ok)
        for (const auto& [name, value] : state.members)
            if (!value.is_integer() || value.kind == Json::Kind::Int || value.as_u64() >= 4) state_ok = false;
    if (!state_ok) throw Rejected("invalid lobby player states");
    const Json& listen = document.at("listen_server");
    if (!keys_are(listen, {"host_address", "host_player_id"}) || !equals_u64(listen.at("host_player_id"), account) ||
        base64_decode(text_of(listen.at("host_address"))).size() != 67)
        throw Rejected("invalid lobby host advertisement");
    const Json& attachment = document.at("attachment");
    if (!attachment.is_null() && (!attachment.is_string() || attachment.text.size() >= 1024)) throw Rejected("invalid lobby attachment");
    const Json& team = document.at("team_balance");
    if (!keys_are(team, {"can_change_teams"}) || !team.at("can_change_teams").is_bool()) throw Rejected("invalid lobby team balance");
    const Json& rules = document.at("ruleset_payload");
    bool rules_ok = rules.is_object();
    if (rules_ok)
        for (const auto& [name, value] : rules.members)
            if ((name != "is_merge_src" && name != "is_merge_dst") || !value.is_bool()) rules_ok = false;
    if (!rules_ok) throw Rejected("unsupported lobby merge rules");
    Json backend = current->second.backend;
    const std::uint64_t version = expected + 1;
    const auto now = static_cast<std::uint64_t>(std::time(nullptr));
    document.set("update_id", Json::of(version));
    document.set("update_time", Json::of(now));
    backend.set("update_id", Json::of(version));
    backend.set("update_time", Json::of(now));
    save_documents(lobby, document, backend, version);
    return true;
}
void MatchmakingStore::cancel_pending(std::uint64_t account, const std::string& selector) {
    for (Search& s : searches_)
        if (s.account == account && s.selector == selector && s.status == "pending") s.status = "cancelled";
}
const std::string* MatchmakingStore::player_info(std::uint64_t account) const {
    const auto it = player_info_.find(account);
    return it == player_info_.end() ? nullptr : &it->second;
}
const LobbyDocuments* MatchmakingStore::documents(std::uint64_t lobby) const {
    const auto it = documents_.find(lobby);
    return it == documents_.end() ? nullptr : &it->second;
}

std::pair<Json, Json> lobby_documents(const LobbyPlan& plan, const Json& advertisement, std::uint64_t now) {
    const Json& listen = advertisement.at("listen_server");
    const Json& search = plan.players.at(0).search;
    Json state = Json::object(), players = Json::object();
    for (const PlanPlayer& p : plan.players) {
        state.set(std::to_string(p.account), Json::of(0));
        players.set(std::to_string(p.account), Json::of("expected"));
    }
    Json host_listen = Json::object();
    host_listen.set("host_address", listen.at("local_address"));
    host_listen.set("host_player_id", Json::of(plan.host));
    Json host = Json::object();
    host.set("update_time", Json::of(now));
    host.set("update_id", Json::of(1));
    host.set("lobby_open", Json::of(true));
    host.set("lobby_open_for_pres_join", Json::of(false));
    host.set("player_state", std::move(state));
    host.set("listen_server", std::move(host_listen));
    host.set("team_balance", single("can_change_teams", Json::of(false)));
    host.set("ruleset_payload", Json::object());
    host.set("attachment", Json::null());
    Json dtls = Json::object();
    dtls.set("security_id", listen.at("security_id"));
    dtls.set("security_key", listen.at("security_key"));
    Json backend = Json::object();
    backend.set("update_id", Json::of(1));
    backend.set("update_time", Json::of(now));
    backend.set("lobby_id", Json::of(plan.lobby_id));
    backend.set("create_time", Json::of(now));
    backend.set("players", std::move(players));
    backend.set("dtls", std::move(dtls));
    backend.set("ruleset_payload", search.at("ruleset_payload"));
    return {host, backend};
}
Bytes room_create_event(std::uint64_t search_id, std::uint64_t lobby, const Json& host, const Json& backend) {
    Writer w;
    w.u32(ROOM_CREATED_EVENT).u64(search_id).u64(lobby).u64(host.at("update_id").as_u64());
    w.string(dump_json(host)).string(dump_json(backend));
    return w.data();
}
Bytes room_join_event(std::uint64_t search_id, std::uint64_t lobby, const Json& host, const Json& backend) {
    Writer w;
    w.u32(ROOM_JOIN_EVENT).u64(search_id).u64(lobby);
    w.string(dump_json(host)).string(dump_json(backend));
    return w.data();
}
Bytes room_update_event(std::uint64_t lobby, const Json& host, const Json& backend) {
    Writer w;
    w.u32(ROOM_UPDATED_EVENT).u64(lobby).u64(host.at("update_id").as_u64());
    w.string(dump_json(host)).string(dump_json(backend));
    return w.data();
}
} // namespace ctr_online
