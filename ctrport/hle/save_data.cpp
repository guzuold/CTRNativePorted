// SPDX-License-Identifier: GPL-2.0-or-later
// libSceSaveData: the functions CTR imports besides Initialize3 (ctrport's own).
// Behaviour and structure layouts follow pinned shadPS4 v0.18.0
// src/core/libraries/save_data/savedata.cpp: per-user, per-title directories, mount points
// "/savedata0".."/savedata15" resolved by ctrport's file system, wildcard search ('%', '_'),
// sort keys and orders, firmware >= 2.50 result fields. Storage layout is ctrport's own:
// runtime/user/savedata/<user>/<title>/<dir>/ with sce_sys/param.bin (the guest
// OrbisSaveDataParam plus block count) and sce_sys/icon0.png.
#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "ctr_hle.h"

namespace fs = std::filesystem;

namespace {
constexpr int OK = 0;
constexpr int PARAMETER = static_cast<int>(0x809F0000);
constexpr int BUSY = static_cast<int>(0x809F0003);
constexpr int NOT_MOUNTED = static_cast<int>(0x809F0004);
constexpr int BAD_MOUNTED = static_cast<int>(0x809F000D);
constexpr int EXISTS = static_cast<int>(0x809F0007);
constexpr int NOT_FOUND = static_cast<int>(0x809F0008);
constexpr int INTERNAL = static_cast<int>(0x809F000B);
constexpr int MOUNT_FULL = static_cast<int>(0x809F000C);
constexpr int INVALID_LOGIN_USER = static_cast<int>(0x809F0011);
constexpr std::uint64_t BLOCK_SIZE = 32768, BLOCKS_MIN2 = 96;
constexpr const char* LIB = "libSceSaveData";

enum MountMode : std::uint32_t { RDONLY = 1, RDWR = 2, CREATE = 4, DESTRUCT_OFF = 8, COPY_ICON = 16, CREATE2 = 32 };

struct DirName {
    char data[32];
};
struct TitleId {
    char data[10];
    char pad[6];
};
struct Param {
    char title[128];
    char subTitle[128];
    char detail[1024];
    std::uint32_t userParam;
    std::int32_t pad;
    std::int64_t mtime;
    std::uint8_t reserved[32];
};
static_assert(sizeof(Param) == 1328);
struct SearchCond {
    std::int32_t userId;
    std::int32_t pad;
    const TitleId* titleId;
    const DirName* dirName;
    std::uint32_t key;
    std::uint32_t order;
    std::uint8_t reserved[32];
};
struct SearchInfo {
    std::uint64_t blocks;
    std::uint64_t freeBlocks;
    std::uint8_t reserved[32];
};
struct SearchResult {
    std::uint32_t hitNum;
    std::int32_t pad;
    DirName* dirNames;
    std::uint32_t dirNamesNum;
    std::uint32_t setNum;
    Param* params;
    SearchInfo* infos;
    std::uint8_t reserved[12];
    std::int32_t pad2;
};
static_assert(sizeof(SearchCond) == 64 && sizeof(SearchResult) == 56);
struct Mount2 {
    std::int32_t userId;
    std::int32_t pad;
    const DirName* dirName;
    std::uint64_t blocks;
    std::uint32_t mountMode;
    std::uint8_t reserved[32];
    std::int32_t pad2;
};
struct MountPoint {
    char data[16];
};
struct MountResult {
    MountPoint mountPoint;
    std::uint64_t requiredBlocks;
    std::uint32_t unused;
    std::uint32_t mountStatus;
    std::uint8_t reserved[28];
    std::int32_t pad;
};
static_assert(sizeof(Mount2) == 64 && sizeof(MountResult) == 64);
struct Icon {
    void* buf;
    std::size_t bufSize;
    std::size_t dataSize;
    std::uint8_t reserved[32];
};
struct Delete {
    std::int32_t userId;
    std::int32_t pad;
    const TitleId* titleId;
    const DirName* dirName;
    std::uint32_t unused;
    std::uint8_t reserved[32];
    std::int32_t pad2;
};
struct Stored {
    Param param;
    std::uint64_t blocks;
};

struct Slot {
    std::string dir, point;
    std::int32_t user = 0;
    fs::path path;
    bool read_only = false;
    Stored stored{};
};
std::mutex mutex;
std::array<std::optional<Slot>, 16> slots;

std::string bounded(const char* data, std::size_t size) {
    return std::string(data, strnlen(data, size));
}
const char* title_id() {
    const auto* h = ctr_hle::host();
    return h ? h->title_id : "CUSA14876";
}
fs::path title_path(std::int32_t user, const std::string& title) {
    return fs::path("runtime/user/savedata") / std::to_string(user) / title;
}
bool valid_dir_name(const std::string& name) {
    if (name.empty() || name.size() >= 32 || name.starts_with("sce_")) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
}
bool read_stored(const fs::path& dir, Stored& stored) {
    std::ifstream in(dir / "sce_sys" / "param.bin", std::ios::binary);
    return in.read(reinterpret_cast<char*>(&stored), sizeof(stored)) && in.gcount() == sizeof(stored);
}
bool write_stored(const fs::path& dir, const Stored& stored) {
    std::error_code ec;
    fs::create_directories(dir / "sce_sys", ec);
    std::ofstream out(dir / "sce_sys" / "param.bin", std::ios::binary | std::ios::trunc);
    return static_cast<bool>(out.write(reinterpret_cast<const char*>(&stored), sizeof(stored)));
}
std::uint64_t directory_bytes(const fs::path& dir) {
    std::uint64_t total = 0;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec)) {
            total += entry.file_size(ec);
        }
    }
    return total;
}
bool match(std::string_view str, std::string_view pattern) {
    std::size_t s = 0, p = 0;
    while (s < str.size() && p < pattern.size()) {
        if (pattern[p] == '%') {
            for (std::size_t w = s; w <= str.size(); ++w) {
                if (match(str.substr(w), pattern.substr(p + 1))) {
                    return true;
                }
            }
            return false;
        }
        if (pattern[p] != '_' && pattern[p] != str[s]) {
            return false;
        }
        ++s;
        ++p;
    }
    while (p < pattern.size() && pattern[p] == '%') {
        ++p;
    }
    return s == str.size() && p == pattern.size();
}
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
Slot* find_slot(const MountPoint* point) {
    if (!point) {
        return nullptr;
    }
    const auto name = bounded(point->data, sizeof(point->data));
    for (auto& slot : slots) {
        if (slot && slot->point == name) {
            return &*slot;
        }
    }
    return nullptr;
}
int log(const char* function, int result, const std::string& extra = {}) {
    ctr_hle::log_call(LIB, function, result, extra.c_str());
    return result;
}
std::string quoted(const char* key, const std::string& value) {
    std::string out = ",\"";
    out += key;
    out += "\":\"";
    for (char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out + "\"";
}
} // namespace

CTR_SYSV int sceSaveDataDirNameSearch(const SearchCond* cond, SearchResult* result) {
    std::scoped_lock lock{mutex};
    if (!cond || !result || cond->key > 5 || cond->order > 1) {
        return log("sceSaveDataDirNameSearch", PARAMETER);
    }
    const std::string title = cond->titleId ? bounded(cond->titleId->data, 10) : title_id();
    const auto root = title_path(cond->userId, title);
    std::vector<std::string> names;
    std::error_code ec;
    if (fs::exists(root, ec)) {
        for (const auto& entry : fs::directory_iterator(root, ec)) {
            const auto name = entry.path().filename().string();
            if (entry.is_directory(ec) && !name.starts_with("sce_") &&
                fs::exists(entry.path() / "sce_sys" / "param.bin", ec)) {
                names.push_back(name);
            }
        }
    }
    const std::string pattern = cond->dirName ? lower(bounded(cond->dirName->data, 32)) : std::string{};
    if (!pattern.empty()) {
        std::erase_if(names, [&](const std::string& n) { return !match(lower(n), pattern); });
    }
    struct Entry {
        std::string name;
        Stored stored;
        std::uint64_t used_blocks;
    };
    std::vector<Entry> entries;
    for (const auto& name : names) {
        Entry e{name, {}, 0};
        read_stored(root / name, e.stored);
        e.used_blocks = directory_bytes(root / name) / BLOCK_SIZE;
        entries.push_back(std::move(e));
    }
    auto key = [&](const Entry& e) -> long double {
        switch (cond->key) {
        case 1: return e.stored.param.userParam;
        case 2: return static_cast<long double>(e.stored.blocks);
        case 3: return static_cast<long double>(e.stored.param.mtime);
        case 5: return static_cast<long double>(e.stored.blocks - std::min(e.stored.blocks, e.used_blocks));
        default: return 0;
        }
    };
    if (cond->key == 0) {
        std::stable_sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.name < b.name; });
    } else {
        std::stable_sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) { return key(a) < key(b); });
    }
    if (cond->order == 1) {
        std::reverse(entries.begin(), entries.end());
    }
    const std::size_t count = std::min<std::size_t>(result->dirNamesNum, entries.size());
    result->hitNum = static_cast<std::uint32_t>(entries.size());
    result->setNum = static_cast<std::uint32_t>(count);
    for (std::size_t i = 0; i < count; ++i) {
        std::memset(result->dirNames[i].data, 0, sizeof(result->dirNames[i].data));
        std::memcpy(result->dirNames[i].data, entries[i].name.data(), std::min<std::size_t>(entries[i].name.size(), 31));
        if (result->params) {
            result->params[i] = entries[i].stored.param;
        }
        if (result->infos) {
            result->infos[i] = SearchInfo{};
            result->infos[i].blocks = entries[i].stored.blocks;
            result->infos[i].freeBlocks = entries[i].stored.blocks - std::min(entries[i].stored.blocks, entries[i].used_blocks);
        }
    }
    return log("sceSaveDataDirNameSearch", OK,
               quoted("pattern", pattern) + ",\"user\":" + std::to_string(cond->userId) +
                   ",\"hits\":" + std::to_string(entries.size()) + ",\"set\":" + std::to_string(count));
}
CTR_HLE_REGISTER("dyIhnXq-0SM", "libSceSaveData", sceSaveDataDirNameSearch);

CTR_SYSV int sceSaveDataMount2(const Mount2* mount, MountResult* result) {
    std::scoped_lock lock{mutex};
    if (!mount || !mount->dirName || !result) {
        return log("sceSaveDataMount2", PARAMETER);
    }
    if (mount->userId < 0) {
        return log("sceSaveDataMount2", INVALID_LOGIN_USER);
    }
    const std::string dir = bounded(mount->dirName->data, 32);
    if (!valid_dir_name(dir)) {
        return log("sceSaveDataMount2", PARAMETER, quoted("dir", dir));
    }
    const bool read_only = mount->mountMode & RDONLY;
    const bool create = mount->mountMode & CREATE;
    const bool create2 = mount->mountMode & CREATE2;
    int free_slot = -1;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (slots[i] && slots[i]->dir == dir) {
            return log("sceSaveDataMount2", BUSY, quoted("dir", dir));
        }
        if (!slots[i] && free_slot < 0) {
            free_slot = static_cast<int>(i);
        }
    }
    if (free_slot < 0) {
        return log("sceSaveDataMount2", MOUNT_FULL);
    }
    const auto path = title_path(mount->userId, title_id()) / dir;
    std::error_code ec;
    const bool exists = fs::exists(path / "sce_sys" / "param.bin", ec);
    if (!create && !create2 && !exists) {
        return log("sceSaveDataMount2", NOT_FOUND, quoted("dir", dir));
    }
    if (create && exists) {
        return log("sceSaveDataMount2", EXISTS, quoted("dir", dir));
    }
    Slot slot;
    slot.dir = dir;
    slot.user = mount->userId;
    slot.path = path;
    slot.read_only = read_only;
    slot.point = "/savedata" + std::to_string(free_slot);
    const bool created = !exists;
    if (created) {
        slot.stored = Stored{};
        slot.stored.param.mtime = static_cast<std::int64_t>(std::time(nullptr));
        slot.stored.blocks = std::max<std::uint64_t>(mount->blocks, BLOCKS_MIN2);
        if (!write_stored(path, slot.stored)) {
            return log("sceSaveDataMount2", INTERNAL, quoted("dir", dir));
        }
    } else if (!read_stored(path, slot.stored)) {
        return log("sceSaveDataMount2", INTERNAL, quoted("dir", dir));
    }
    const auto* h = ctr_hle::host();
    if (!h || h->mount(slot.point.c_str(), path.generic_string().c_str()) != 0) {
        return log("sceSaveDataMount2", INTERNAL, quoted("dir", dir) + ",\"host_mount\":false");
    }
    std::memset(result, 0, sizeof(*result));
    std::memcpy(result->mountPoint.data, slot.point.data(), slot.point.size());
    result->mountStatus = (create2 && created) ? 1 : 0;
    const std::string extra = quoted("dir", dir) + quoted("mount_point", slot.point) +
                              ",\"created\":" + (created ? "true" : "false") +
                              ",\"read_only\":" + (read_only ? "true" : "false") +
                              ",\"mode\":" + std::to_string(mount->mountMode);
    slots[free_slot] = std::move(slot);
    return log("sceSaveDataMount2", OK, extra);
}
CTR_HLE_REGISTER("0z45PIH+SNI", "libSceSaveData", sceSaveDataMount2);

CTR_SYSV int sceSaveDataUmount(const MountPoint* point) {
    std::scoped_lock lock{mutex};
    if (!point) {
        return log("sceSaveDataUmount", PARAMETER);
    }
    for (auto& slot : slots) {
        if (slot && slot->point == bounded(point->data, 16)) {
            const auto* h = ctr_hle::host();
            if (h) {
                h->unmount(slot->point.c_str());
            }
            if (!slot->read_only) {
                slot->stored.param.mtime = static_cast<std::int64_t>(std::time(nullptr));
                write_stored(slot->path, slot->stored);
            }
            const std::string extra = quoted("mount_point", slot->point);
            slot.reset();
            return log("sceSaveDataUmount", OK, extra);
        }
    }
    return log("sceSaveDataUmount", NOT_FOUND);
}
CTR_HLE_REGISTER("BMR4F-Uek3E", "libSceSaveData", sceSaveDataUmount);

CTR_SYSV int sceSaveDataGetParam(const MountPoint* point, std::uint32_t type, void* buffer,
                                 std::size_t size, std::size_t* got) {
    std::scoped_lock lock{mutex};
    if (type > 5 || !buffer) {
        return log("sceSaveDataGetParam", PARAMETER);
    }
    Slot* slot = find_slot(point);
    if (!slot) {
        return log("sceSaveDataGetParam", NOT_MOUNTED);
    }
    const Param& p = slot->stored.param;
    std::size_t written = 0;
    switch (type) {
    case 0:
        if (size != sizeof(Param)) {
            return log("sceSaveDataGetParam", PARAMETER);
        }
        std::memcpy(buffer, &p, sizeof(Param));
        written = sizeof(Param);
        break;
    case 1:
    case 2:
    case 3: {
        const char* text = type == 1 ? p.title : type == 2 ? p.subTitle : p.detail;
        const std::size_t limit = type == 3 ? sizeof(p.detail) : sizeof(p.title);
        if (!size) {
            return log("sceSaveDataGetParam", PARAMETER);
        }
        const std::size_t n = std::min(strnlen(text, limit), size - 1);
        std::memcpy(buffer, text, n);
        static_cast<char*>(buffer)[n] = 0;
        written = n + 1;
        break;
    }
    case 4:
        std::memcpy(buffer, &p.userParam, sizeof(std::uint32_t));
        written = sizeof(std::uint32_t);
        break;
    case 5:
        std::memcpy(buffer, &p.mtime, sizeof(std::int64_t));
        written = sizeof(std::int64_t);
        break;
    }
    if (got) {
        *got = written;
    }
    return log("sceSaveDataGetParam", OK, ",\"type\":" + std::to_string(type));
}
CTR_HLE_REGISTER("XgvSuIdnMlw", "libSceSaveData", sceSaveDataGetParam);

CTR_SYSV int sceSaveDataSetParam(const MountPoint* point, std::uint32_t type, const void* buffer,
                                 std::size_t size) {
    std::scoped_lock lock{mutex};
    if (type > 4 || !point || !buffer) {
        return log("sceSaveDataSetParam", PARAMETER);
    }
    Slot* slot = find_slot(point);
    if (!slot) {
        return log("sceSaveDataSetParam", NOT_MOUNTED);
    }
    Param& p = slot->stored.param;
    switch (type) {
    case 0: {
        if (size != sizeof(Param)) {
            return log("sceSaveDataSetParam", PARAMETER);
        }
        const auto mtime = p.mtime;
        std::memcpy(&p, buffer, sizeof(Param));
        p.mtime = mtime;
        break;
    }
    case 1:
    case 2:
    case 3: {
        char* text = type == 1 ? p.title : type == 2 ? p.subTitle : p.detail;
        const std::size_t limit = type == 3 ? sizeof(p.detail) : sizeof(p.title);
        std::memset(text, 0, limit);
        const auto* source = static_cast<const char*>(buffer);
        std::memcpy(text, source, std::min(strnlen(source, limit - 1), limit - 1));
        break;
    }
    case 4:
        std::memcpy(&p.userParam, buffer, sizeof(std::uint32_t));
        break;
    }
    if (!write_stored(slot->path, slot->stored)) {
        return log("sceSaveDataSetParam", INTERNAL);
    }
    return log("sceSaveDataSetParam", OK, ",\"type\":" + std::to_string(type));
}
CTR_HLE_REGISTER("85zul--eGXs", "libSceSaveData", sceSaveDataSetParam);

CTR_SYSV int sceSaveDataSaveIcon(const MountPoint* point, const Icon* icon) {
    std::scoped_lock lock{mutex};
    if (!point || !icon || !icon->buf) {
        return log("sceSaveDataSaveIcon", PARAMETER);
    }
    Slot* slot = find_slot(point);
    if (!slot) {
        return log("sceSaveDataSaveIcon", NOT_MOUNTED);
    }
    if (slot->read_only) {
        return log("sceSaveDataSaveIcon", BAD_MOUNTED);
    }
    std::error_code ec;
    fs::create_directories(slot->path / "sce_sys", ec);
    std::ofstream out(slot->path / "sce_sys" / "icon0.png", std::ios::binary | std::ios::trunc);
    const auto bytes = std::min(icon->bufSize, icon->dataSize);
    if (!out.write(static_cast<const char*>(icon->buf), static_cast<std::streamsize>(bytes))) {
        return log("sceSaveDataSaveIcon", INTERNAL);
    }
    return log("sceSaveDataSaveIcon", OK, ",\"bytes\":" + std::to_string(bytes));
}
CTR_HLE_REGISTER("c88Yy54Mx0w", "libSceSaveData", sceSaveDataSaveIcon);

CTR_SYSV int sceSaveDataLoadIcon(const MountPoint* point, Icon* icon) {
    std::scoped_lock lock{mutex};
    if (!point || !icon || !icon->buf) {
        return log("sceSaveDataLoadIcon", PARAMETER);
    }
    Slot* slot = find_slot(point);
    if (!slot) {
        return log("sceSaveDataLoadIcon", NOT_MOUNTED);
    }
    const auto path = slot->path / "sce_sys" / "icon0.png";
    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return log("sceSaveDataLoadIcon", NOT_FOUND);
    }
    std::ifstream in(path, std::ios::binary);
    const auto size = static_cast<std::size_t>(fs::file_size(path, ec));
    icon->dataSize = size;
    in.read(static_cast<char*>(icon->buf), static_cast<std::streamsize>(std::min(icon->bufSize, size)));
    return log("sceSaveDataLoadIcon", OK, ",\"bytes\":" + std::to_string(size));
}
CTR_HLE_REGISTER("cGjO3wM3V28", "libSceSaveData", sceSaveDataLoadIcon);

CTR_SYSV int sceSaveDataDelete(const Delete* del) {
    std::scoped_lock lock{mutex};
    if (!del || !del->dirName) {
        return log("sceSaveDataDelete", PARAMETER);
    }
    const std::string dir = bounded(del->dirName->data, 32);
    if (!valid_dir_name(dir)) {
        return log("sceSaveDataDelete", PARAMETER);
    }
    for (const auto& slot : slots) {
        if (slot && slot->dir == dir) {
            return log("sceSaveDataDelete", BUSY, quoted("dir", dir));
        }
    }
    std::error_code ec;
    fs::remove_all(title_path(del->userId, title_id()) / dir, ec);
    return log("sceSaveDataDelete", ec ? INTERNAL : OK, quoted("dir", dir));
}
CTR_HLE_REGISTER("S1GkePI17zQ", "libSceSaveData", sceSaveDataDelete);

CTR_SYSV int sceSaveDataTerminate() {
    std::scoped_lock lock{mutex};
    for (const auto& slot : slots) {
        if (slot) {
            return log("sceSaveDataTerminate", BUSY); // firmware >= 4.00 behaviour
        }
    }
    return log("sceSaveDataTerminate", OK);
}
CTR_HLE_REGISTER("yKDy8S5yLA0", "libSceSaveData", sceSaveDataTerminate);
