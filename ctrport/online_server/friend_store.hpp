// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "friend_identity.hpp"
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace ctr_online {

struct FriendEntry {
    std::string nickname;
    std::string friend_code;
    Bytes public_key;
    std::string last_endpoint;
    std::uint64_t last_seen = 0;
    std::string last_status;
};

class FriendStore {
public:
    FriendStore();
    explicit FriendStore(const std::filesystem::path& path);

    bool load(const std::filesystem::path& path);
    bool save(const std::filesystem::path& path) const;

    std::vector<FriendEntry> entries() const;

    bool add_or_update(const std::string& code_str, const std::string& nickname, std::string* error_msg = nullptr);
    bool remove(const std::string& code_str);
    bool update_endpoint(const std::string& code_str, const std::string& endpoint, const std::string& status);
    // A failed lookup changes diagnostics, not the last known address or successful contact time.
    bool update_status(const std::string& code_str, const std::string& status);

    const std::filesystem::path& path() const { return path_; }

private:
    mutable std::mutex mutex_;
    std::filesystem::path path_;
    std::vector<FriendEntry> entries_;
};

} // namespace ctr_online
