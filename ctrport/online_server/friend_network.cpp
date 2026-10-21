// SPDX-License-Identifier: GPL-2.0-or-later
#include "friend_network.hpp"
#include "crypto.hpp"
#include "friend_store.hpp"
#include "party_lobby.hpp"
#include "protocol.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ctr_online {
int http_request(const std::string& ipv4, std::uint16_t port, const std::string& path, const std::string& payload,
                 std::string& body, int timeout_ms); // launcher_online.cpp

namespace {
const char* const kDomain = "ctr-peer-v1:";
std::string digest_text(const std::string& body) { return kDomain + hex(sha256(bytes_of(body))); }
std::string text_or_empty(const Json& object, const std::string& key) {
    const Json* value = object.find(key);
    return value && value->is_string() ? value->text : std::string();
}
} // namespace

Json sign_peer_message(const HostIdentity& identity, const Json& payload) {
    const std::string body = dump_json(payload);
    const auto timestamp = party_now();
    Json packet = Json::object();
    packet.set("code", Json::of(identity.friend_code()));
    packet.set("payload", Json::of(body));
    packet.set("time", Json::of(timestamp));
    packet.set("signature", Json::of(hex(identity.sign_challenge(digest_text(body), {}, timestamp))));
    return packet;
}

std::string peer_message_sender(const Json& packet) {
    return packet.is_object() ? text_or_empty(packet, "code") : std::string();
}

Json verify_peer_message(const Json& packet, const std::string& expected_code, std::uint64_t now) {
    FriendCodeInfo identity;
    if (!packet.is_object() || parse_friend_code(expected_code, identity) != CodeParseError::Ok ||
        text_or_empty(packet, "code") != identity.code)
        throw std::runtime_error("Invalid friend service identity");
    const auto timestamp = packet.at("time").as_u64();
    if ((timestamp > now ? timestamp - now : now - timestamp) > 60)
        throw std::runtime_error("Friend service message expired");
    const auto& body = packet.at("payload").text;
    if (!verify_friend_response(identity.public_key, digest_text(body), {}, timestamp,
                                from_hex(packet.at("signature").text)))
        throw std::runtime_error("Invalid friend service signature");
    Json payload = parse_json(body);
    if (!payload.is_object()) throw std::runtime_error("Invalid friend service message");
    return payload;
}

Json peer_request(const HostIdentity& self, const std::string& ip, std::uint16_t port,
                  const std::string& target_code, Json payload, int timeout_ms) {
    const auto nonce = hex(random_bytes(16));
    payload.set("nonce", Json::of(nonce));
    payload.set("to", Json::of(target_code));
    std::string body;
    const int status = http_request(ip, port, "/ctr/v1/peer", dump_json(sign_peer_message(self, payload)), body, timeout_ms);
    if (status != 200) {
        std::string reason = status == 0 ? "Friend service unreachable" : "Friend service: HTTP " + std::to_string(status);
        try { const auto error = parse_json(body); if (error.find("error")) reason = error.at("error").text; } catch (...) {}
        throw std::runtime_error(reason);
    }
    Json response = verify_peer_message(parse_json(body), target_code, party_now());
    if (text_or_empty(response, "nonce") != nonce || text_or_empty(response, "to") != self.friend_code())
        throw std::runtime_error("Friend service reply does not match");
    return response;
}

bool PeerNonces::fresh(const std::string& sender, const std::string& nonce, std::uint64_t now) {
    if (nonce.size() < 16 || nonce.size() > 64) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = seen_.begin(); it != seen_.end();)
        it = now > it->second && now - it->second > 120 ? seen_.erase(it) : std::next(it);
    return seen_.emplace(sender + "/" + nonce, now).second;
}

FriendWatcher::FriendWatcher(const HostIdentity& self, std::filesystem::path friends_path, Resolver resolver,
                             std::chrono::milliseconds period)
    : self_(self), friends_path_(std::move(friends_path)), resolver_(std::move(resolver)), period_(period) {
    names_path_ = friends_path_.parent_path() / "friend_names.json";
    Json names = Json::object();
    try {
        std::ifstream file(names_path_, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        if (file) names = parse_json(text.str());
    } catch (...) {}
    for (const auto& entry : FriendStore(friends_path_).entries()) {
        FriendState state;
        state.code = entry.friend_code;
        state.nickname = entry.nickname;
        if (const Json* name = names.is_object() ? names.find(entry.friend_code) : nullptr; name && name->is_string()) {
            state.name = name->text;
            state.account = state.name.empty() ? 0 : player_account(state.code, state.name);
        }
        order_.push_back(state.code);
        states_.emplace(state.code, std::move(state));
    }
}

FriendWatcher::~FriendWatcher() { stop(); }

void FriendWatcher::start() {
    if (thread_.joinable()) return;
    stopping_ = false;
    thread_ = std::thread([this] { run(); });
}

void FriendWatcher::stop() {
    {
        std::lock_guard<std::mutex> lock(wake_mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void FriendWatcher::run() {
    while (!stopping_) {
        try { poll_now(); } catch (...) {}
        std::unique_lock<std::mutex> lock(wake_mutex_);
        wake_.wait_for(lock, period_, [this] { return stopping_.load(); });
    }
}

std::optional<std::pair<std::string, std::uint16_t>> FriendWatcher::endpoint_of(const std::string& code) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = states_.find(code);
        if (it != states_.end() && it->second.online && !it->second.ip.empty())
            return std::make_pair(it->second.ip, it->second.port);
        if (const auto observed = observed_.find(code); observed != observed_.end())
            return observed->second;
    }
    return resolver_ ? resolver_(code) : std::nullopt;
}

void FriendWatcher::observe_endpoint(const std::string& code, const std::string& ip, std::uint16_t port) {
    if (!port || ip.empty() || !is_friend(code)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    observed_[code] = {ip, port};
}

void FriendWatcher::poll_now() {
    // One round at a time: a slow round must not land its older answers over a newer one.
    std::lock_guard<std::mutex> round(poll_mutex_);
    const FriendStore store(friends_path_);
    std::vector<FriendState> next;
    for (const auto& entry : store.entries()) {
        FriendState state;
        state.code = entry.friend_code;
        state.nickname = entry.nickname;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = states_.find(state.code);
            if (it != states_.end()) {
                // Offline friends keep their last known name and account for the in-game list.
                state.name = it->second.name;
                state.account = it->second.account;
                state.seen = it->second.seen;
            }
        }
        if (stopping_) return;
        try {
            if (const auto endpoint = endpoint_of(state.code)) {
                Json request = Json::object();
                request.set("action", Json::of("presence"));
                request.set("name", Json::of(local_name ? local_name() : std::string()));
                request.set("contact_port", Json::of(static_cast<unsigned>(contact_port)));
                const Json report = peer_request(self_, endpoint->first, endpoint->second, state.code, request, 2000);
                state.online = true;
                state.in_game = report.find("in_game") && report.at("in_game").boolean;
                state.name = text_or_empty(report, "name");
                state.account = state.name.empty() ? 0 : player_account(state.code, state.name);
                state.ip = endpoint->first;
                state.port = endpoint->second;
                state.seen = party_now();
                state.report = report;
                // Persist only after the expected friend signs a fresh presence response.
                const std::string address = state.ip + ":" + std::to_string(state.port);
                if (entry.last_endpoint != address)
                    FriendStore(friends_path_).update_endpoint(state.code, address, "Server verificato");
            } else if (on_failure) {
                on_failure(state.code, "Friend address not resolved yet");
            }
        } catch (const std::exception& error) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                observed_.erase(state.code);
            }
            if (on_failure) on_failure(state.code, error.what());
        }
        next.push_back(std::move(state));
    }
    bool changed = false, renamed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<std::string, FriendState> states;
        std::vector<std::string> order;
        for (auto& state : next) {
            const auto it = states_.find(state.code);
            if (it == states_.end() || it->second.online != state.online || it->second.in_game != state.in_game ||
                it->second.name != state.name || it->second.report != state.report)
                changed = true;
            if (it == states_.end() || it->second.name != state.name) renamed = true;
            order.push_back(state.code);
            states.emplace(state.code, std::move(state));
        }
        if (order != order_) changed = renamed = true;
        states_ = std::move(states);
        for (auto it = observed_.begin(); it != observed_.end();)
            it = states_.count(it->first) ? std::next(it) : observed_.erase(it);
        order_ = std::move(order);
        if (renamed) {
            Json names = Json::object();
            for (const auto& code : order_)
                if (!states_.at(code).name.empty()) names.set(code, Json::of(states_.at(code).name));
            std::ofstream(names_path_, std::ios::binary | std::ios::trunc) << dump_json(names);
        }
    }
    if (changed && on_change) on_change();
}

std::vector<FriendState> FriendWatcher::friends() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<FriendState> result;
    for (const auto& code : order_) result.push_back(states_.at(code));
    return result;
}

std::optional<FriendState> FriendWatcher::by_code(const std::string& code) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = states_.find(code);
    if (it == states_.end()) return std::nullopt;
    return it->second;
}

std::optional<FriendState> FriendWatcher::by_account(std::uint64_t account) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [code, state] : states_)
        if (account && state.account == account) return state;
    return std::nullopt;
}

bool FriendWatcher::is_friend(const std::string& code) const {
    // The rubrica on disk is the authority: removing a friend in the launcher takes effect at once.
    for (const auto& entry : FriendStore(friends_path_).entries())
        if (entry.friend_code == code) return true;
    return false;
}

Json FriendWatcher::request(const std::string& code, Json payload, int timeout_ms) {
    if (!is_friend(code)) throw std::runtime_error("Not in the friends list");
    const auto endpoint = endpoint_of(code);
    if (!endpoint) throw std::runtime_error("Friend service unreachable");
    return peer_request(self_, endpoint->first, endpoint->second, code, std::move(payload), timeout_ms);
}

FriendWatcher::Resolver rubrica_resolver(const std::filesystem::path& friends_path) {
    return [friends_path](const std::string& code) -> std::optional<std::pair<std::string, std::uint16_t>> {
        for (const auto& entry : FriendStore(friends_path).entries()) {
            if (entry.friend_code != code || entry.last_endpoint.empty()) continue;
            const auto colon = entry.last_endpoint.rfind(':');
            if (colon == std::string::npos) return std::make_pair(entry.last_endpoint, std::uint16_t(8780));
            const int port = std::atoi(entry.last_endpoint.c_str() + colon + 1);
            if (port <= 0 || port > 65535) return std::nullopt;
            return std::make_pair(entry.last_endpoint.substr(0, colon), static_cast<std::uint16_t>(port));
        }
        return std::nullopt;
    };
}

} // namespace ctr_online
