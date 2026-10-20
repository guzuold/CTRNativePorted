// SPDX-License-Identifier: GPL-2.0-or-later
#include "friend_store.hpp"
#include "crypto.hpp"
#include "json.hpp"
#include <fstream>
#include <algorithm>

namespace ctr_online {

FriendStore::FriendStore() = default;

FriendStore::FriendStore(const std::filesystem::path& path) : path_(path) {
    load(path);
}

bool FriendStore::load(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    path_ = path;
    entries_.clear();
    if (!std::filesystem::exists(path)) return false;

    try {
        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Json root = parse_json(content);
        if (!root.is_array()) return false;
        for (const Json& item : root.items) {
            FriendEntry entry;
            entry.nickname = item.at("nickname").text;
            entry.friend_code = item.at("friend_code").text;
            entry.public_key = from_hex(item.at("public_key_hex").text);
            entry.last_endpoint = item.at("last_endpoint").text;
            entry.last_seen = item.at("last_seen").as_u64();
            entry.last_status = item.at("last_status").text;
            if (entry.public_key.size() == 64) {
                entries_.push_back(std::move(entry));
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool FriendStore::save(const std::filesystem::path& path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    Json root = Json::array();
    for (const auto& entry : entries_) {
        Json item = Json::object();
        item.set("nickname", Json::of(entry.nickname));
        item.set("friend_code", Json::of(entry.friend_code));
        item.set("public_key_hex", Json::of(hex(entry.public_key)));
        item.set("last_endpoint", Json::of(entry.last_endpoint));
        item.set("last_seen", Json::of(entry.last_seen));
        item.set("last_status", Json::of(entry.last_status));
        root.items.push_back(std::move(item));
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) return false;
    file << dump_json_spaced(root) << '\n';
    return static_cast<bool>(file);
}

std::vector<FriendEntry> FriendStore::entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_;
}

bool FriendStore::add_or_update(const std::string& code_str, const std::string& nickname, std::string* error_msg) {
    FriendCodeInfo info;
    const CodeParseError err = parse_friend_code(code_str, info);
    if (err != CodeParseError::Ok) {
        if (error_msg) *error_msg = code_parse_error_string(err);
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        for (auto& entry : entries_) {
            if (entry.friend_code == info.code) {
                entry.nickname = nickname.empty() ? entry.nickname : nickname;
                entry.public_key = info.public_key;
                lock.unlock();
                const bool saved = path_.empty() || save(path_);
                if (!saved && error_msg) *error_msg = "Unable to save friend list";
                return saved;
            }
        }

        FriendEntry entry;
        entry.nickname = nickname.empty() ? "Amico" : nickname;
        entry.friend_code = info.code;
        entry.public_key = info.public_key;
        entry.last_status = "Mai contattato";
        entries_.push_back(std::move(entry));
    }

    const bool saved = path_.empty() || save(path_);
    if (!saved && error_msg) *error_msg = "Unable to save friend list";
    return saved;
}

bool FriendStore::remove(const std::string& code_str) {
    FriendCodeInfo info;
    const CodeParseError err = parse_friend_code(code_str, info);
    const std::string target = (err == CodeParseError::Ok) ? info.code : code_str;

    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::remove_if(entries_.begin(), entries_.end(),
                                 [&](const FriendEntry& e) { return e.friend_code == target; });
        if (it != entries_.end()) {
            entries_.erase(it, entries_.end());
            removed = true;
        }
    }

    if (removed && !path_.empty()) save(path_);
    return removed;
}

bool FriendStore::update_status(const std::string& code_str, const std::string& status) {
    FriendCodeInfo info;
    const auto err = parse_friend_code(code_str, info);
    const auto target = err == CodeParseError::Ok ? info.code : code_str;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : entries_) {
            if (entry.friend_code == target) {
                entry.last_status = status;
                found = true;
                break;
            }
        }
    }
    if (found && !path_.empty()) save(path_);
    return found;
}

bool FriendStore::update_endpoint(const std::string& code_str, const std::string& endpoint, const std::string& status) {
    FriendCodeInfo info;
    const CodeParseError err = parse_friend_code(code_str, info);
    const std::string target = (err == CodeParseError::Ok) ? info.code : code_str;

    bool found = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& entry : entries_) {
            if (entry.friend_code == target) {
                entry.last_endpoint = endpoint;
                entry.last_status = status;
                entry.last_seen = static_cast<std::uint64_t>(std::time(nullptr));
                found = true;
                break;
            }
        }
    }

    if (found && !path_.empty()) save(path_);
    return found;
}

} // namespace ctr_online
