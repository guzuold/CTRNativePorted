// SPDX-License-Identifier: GPL-2.0-or-later
#include "stores.hpp"
#include "local_backup.hpp"
#include <algorithm>
#include <ctime>
#include <fstream>
#include <initializer_list>
#include <cstdlib>
#include <windows.h>
#include <sstream>

namespace ctr_online {
namespace {
struct Rejected : std::invalid_argument {
    using std::invalid_argument::invalid_argument;
};

std::vector<int> kinds(const std::vector<Param>& params, std::size_t from = 0, std::size_t count = std::string::npos) {
    std::vector<int> out;
    for (std::size_t i = from; i < params.size() && i - from < count; ++i) out.push_back(params[i].kind);
    return out;
}
bool keys_are(const Json& value, std::initializer_list<const char*> keys) {
    if (!value.is_object() || value.members.size() != keys.size()) return false;
    for (const char* key : keys)
        if (!value.find(key)) return false;
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
std::size_t utf8_size(const std::string& text) {
    return text.size();
}
const std::string& text_of(const Json& value) {
    if (!value.is_string()) throw Rejected("expected a string");
    return value.text;
}
Json single(const char* key, Json value) {
    Json o = Json::object();
    o.set(key, std::move(value));
    return o;
}
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
        if (kinds(params, i, 4) != std::vector<int>{typed::UINT32, typed::UINT64, typed::UINT8, typed::INT64})
            throw Rejected("unsupported statistics field types");
        const Json &board = params[i].value, &entity = params[i + 1].value, &mode = params[i + 2].value,
                   &rating = params[i + 3].value;
        if (entity.as_u64() != account || mode.as_u64() != 0 || board.as_u64() > 0xffffffffull)
            throw Rejected("statistics account or write mode rejected");
        records.push_back(
            {{static_cast<std::uint32_t>(board.as_u64()), std::to_string(entity.as_u64())}, rating.as_i64()});
    }
    for (const auto& [key, value] : records) ratings_[key] = value;
    save();
}

namespace {
const std::string MARKET_CONTEXT = "octane_psn";
std::vector<PbField> struct_request(const std::vector<Param>& params) {
    if (params.size() != 1 || params[0].kind != typed::STRUCT || !params[0].value.is_string())
        throw Rejected("expected one struct buffer");
    return pb_fields(from_hex(params[0].value.text));
}
// Python `{tag: value ...}` of a message: the last field with the tag.
const PbField* last_field(const std::vector<PbField>& values, std::uint64_t tag) {
    const PbField* found = nullptr;
    for (const PbField& f : values)
        if (f.tag == tag) found = &f;
    return found;
}
bool is_context(const PbField& f) {
    return f.wire != 0 && std::string(f.data.begin(), f.data.end()) == MARKET_CONTEXT;
}
// [v for t, w, v in values if t == 1] == [b'octane_psn']
void require_context(const std::vector<PbField>& values) {
    std::size_t count = 0;
    bool matches = false;
    for (const PbField& f : values)
        if (f.tag == 1) {
            ++count;
            matches = is_context(f);
        }
    if (count != 1 || !matches) throw Rejected("unsupported achievement context");
}
void append_bytes(Bytes& out, std::initializer_list<Bytes> parts) {
    for (const Bytes& part : parts) out.insert(out.end(), part.begin(), part.end());
}
} // namespace

MarketplaceStore::MarketplaceStore(std::filesystem::path path) : path_(std::move(path)) {
    load();
}
void MarketplaceStore::load() {
    std::error_code error;
    const auto written = std::filesystem::last_write_time(path_, error);
    if (error || written == loaded_) return;
    std::ifstream in(path_);
    std::string table, account;
    std::string raw_key;
    std::int64_t value;
    if (!in) throw Rejected("marketplace state cannot be read");
    decltype(balances_) balances, inventory;
    decltype(rewarded_) rewarded;
    while (in >> table) {
        if (!(in >> account >> raw_key >> value)) throw Rejected("truncated marketplace state");
        if (table == "reward") {
            if (raw_key.size() != 64 || raw_key.find_first_not_of("0123456789abcdef") != std::string::npos)
                throw Rejected("invalid reward receipt");
            rewarded.insert({account, raw_key});
        } else {
            std::size_t consumed = 0;
            const auto key = std::stoull(raw_key, &consumed);
            if (consumed != raw_key.size()) throw Rejected("invalid marketplace key");
            if (table == "balance" && key <= 255)
                balances[{account, key}] = value;
            else if (table == "inventory")
                inventory[{account, key}] = value;
            else
                throw Rejected("unknown marketplace row");
        }
    }
    if (!in.eof()) throw Rejected("marketplace state read failed");
    balances_.swap(balances);
    inventory_.swap(inventory);
    rewarded_.swap(rewarded);
    loaded_ = written;
}
void MarketplaceStore::save() {
    std::filesystem::create_directories(path_.parent_path());
    if (std::filesystem::exists(path_)) {
        std::ifstream previous(path_, std::ios::binary);
        std::ostringstream contents;
        contents << previous.rdbuf();
        if (!previous || previous.bad()) throw Rejected("Cannot back up previous marketplace state");
        Json archive = Json::object();
        archive.set("format", Json::of("ctr-marketplace-file-v1"));
        archive.set("state", Json::of(contents.str()));
        backup_blob(path_.parent_path() / "backups/local", dump_json(archive));
    }
    const std::filesystem::path temporary = path_.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        for (const auto& [key, amount] : balances_)
            out << "balance " << key.first << ' ' << key.second << ' ' << amount << '\n';
        for (const auto& [key, quantity] : inventory_)
            out << "inventory " << key.first << ' ' << key.second << ' ' << quantity << '\n';
        for (const auto& key : rewarded_) out << "reward " << key.first << ' ' << key.second << " 0\n";
        out.close();
        if (!out) throw Rejected("marketplace state write failed");
    }
    if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw Rejected("marketplace state replacement failed");
    std::error_code error;
    loaded_ = std::filesystem::last_write_time(path_, error);
}
Json MarketplaceStore::snapshot(std::uint64_t account) {
    load();
    const auto owner = std::to_string(account);
    std::ostringstream rows;
    for (const auto& [key, amount] : balances_)
        if (key.first == owner) rows << "balance " << owner << ' ' << key.second << ' ' << amount << '\n';
    for (const auto& [key, quantity] : inventory_)
        if (key.first == owner) rows << "inventory " << owner << ' ' << key.second << ' ' << quantity << '\n';
    for (const auto& key : rewarded_)
        if (key.first == owner) rows << "reward " << owner << ' ' << key.second << " 0\n";
    Json result = Json::object();
    result.set("format", Json::of("ctr-marketplace-account-v1"));
    result.set("account", Json::of(account));
    result.set("state", Json::of(rows.str()));
    return result;
}
void MarketplaceStore::set_balance(std::uint64_t account, std::uint64_t currency, std::int64_t amount) {
    if (currency > 255) throw Rejected("currency ID above 255");
    load();
    const auto before = balances_;
    // A zero balance reads like no balance; dropping the row keeps the client's few slots
    // (it asks for as many balances as it has currencies) for the nonzero ones.
    if (amount)
        balances_[{std::to_string(account), currency}] = amount;
    else
        balances_.erase({std::to_string(account), currency});
    try {
        save();
    } catch (...) {
        balances_ = before;
        throw;
    }
}
bool MarketplaceStore::adopt(std::uint64_t from, std::uint64_t to) {
    if (from == to) return false;
    load();
    const auto old_owner = std::to_string(from), owner = std::to_string(to);
    bool found = false;
    for (const auto& [key, amount] : balances_) {
        if (key.first == owner) return false;
        found |= key.first == old_owner;
    }
    for (const auto& [key, quantity] : inventory_) {
        if (key.first == owner) return false;
        found |= key.first == old_owner;
    }
    for (const auto& key : rewarded_) {
        if (key.first == owner) return false;
        found |= key.first == old_owner;
    }
    if (!found) return false;
    const auto balances_before = balances_, inventory_before = inventory_;
    const auto rewarded_before = rewarded_;
    const auto move = [&](auto& table) {
        for (auto it = table.begin(); it != table.end();) {
            if (it->first.first != old_owner) {
                ++it;
                continue;
            }
            table[{owner, it->first.second}] = it->second;
            it = table.erase(it);
        }
    };
    move(balances_);
    move(inventory_);
    for (auto it = rewarded_.begin(); it != rewarded_.end();) {
        if (it->first != old_owner) {
            ++it;
            continue;
        }
        rewarded_.insert({owner, it->second});
        it = rewarded_.erase(it);
    }
    try {
        save();
    } catch (...) {
        balances_ = balances_before;
        inventory_ = inventory_before;
        rewarded_ = rewarded_before;
        throw;
    }
    return true;
}
void MarketplaceStore::set_inventory(std::uint64_t account, std::uint64_t item, std::int64_t quantity) {
    load();
    const auto before = inventory_;
    inventory_[{std::to_string(account), item}] = quantity;
    try {
        save();
    } catch (...) {
        inventory_ = before;
        throw;
    }
}
void MarketplaceStore::award_races(const std::vector<Param>& params, std::uint64_t account, unsigned multiplier) {
    // Project economy chosen by the user (2026-10-16): all end_race modes,
    // base payout x1 or x100 selected by the player. These are our rules, not a reconstruction of retail
    // weekend/daily/golden bonuses. Unknown tracks use a common 32-coin base.
    constexpr std::int64_t fallback_base = 32;
    if (multiplier != 1 && multiplier != 100) throw Rejected("unsupported Wumpa multiplier");
    const auto catalogue_path = path_.parent_path() / "retail_reward_catalog.json";
    const auto fields = struct_request(params);
    require_context(fields);
    std::vector<std::pair<std::string, std::int64_t>> awards;
    Json tracks = Json::array();
    if (std::filesystem::exists(catalogue_path)) {
        std::ifstream catalogue_file(catalogue_path);
        const Json catalogue = parse_json(std::string(std::istreambuf_iterator<char>(catalogue_file), {}));
        if (catalogue.at("schema").as_u64() != 1 ||
            catalogue.at("eboot_sha256").text != "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5")
            throw Rejected("unsupported reward catalogue");
        tracks = catalogue.at("tracks");
        if (!tracks.is_array()) throw Rejected("invalid reward tracks");
    }
    for (const auto& field : fields) {
        if (field.tag != 2 || field.wire != 2) continue;
        const auto event = pb_fields(field.data);
        const auto* name = last_field(event, 1);
        if (!name || std::string(name->data.begin(), name->data.end()) != "end_race") continue;
        const auto* time = last_field(event, 2);
        if (!time || time->wire != 0 || !time->number) throw Rejected("race timestamp missing");
        std::map<std::string, std::uint64_t> attributes;
        for (const auto& part : event) {
            if (part.tag != 3 || part.wire != 2) continue;
            const auto pair = pb_fields(part.data);
            const auto* key = last_field(pair, 1);
            const auto* value = last_field(pair, 2);
            if (!key || key->wire != 2 || !value || value->wire != 0 ||
                !attributes.emplace(std::string(key->data.begin(), key->data.end()), value->number).second)
                throw Rejected("invalid race attribute");
        }
        if (!attributes.at("Time") || attributes.at("Online") > 1 || attributes.at("Track") > UINT32_MAX ||
            attributes.at("RaceMode") > UINT32_MAX)
            throw Rejected("invalid activity completion");
        // Unranked activities use the track's first-place base. No lap-count or
        // mode filter: battles and timed activities need not have three laps.
        const auto rank = attributes.find("Rank");
        const auto place = rank != attributes.end() && rank->second >= 1 && rank->second <= 8
                               ? std::min<std::uint64_t>(rank->second, 4)
                               : 1;
        std::int64_t amount = -1;
        bool found = false;
        for (const auto& track : tracks.items) {
            if (track.at("id").as_u64() != attributes.at("Track")) continue;
            if (found || !track.at("ranks").is_array() || track.at("ranks").items.size() != 4)
                throw Rejected("ambiguous reward track");
            found = true;
            amount = track.at("ranks").items[static_cast<std::size_t>(place - 1)].as_i64();
        }
        if (!found) amount = fallback_base;
        if (amount <= 0 || amount > 1000000) throw Rejected("invalid base reward");
        amount *= multiplier;
        std::ostringstream canonical;
        canonical << "end_race:" << time->number;
        for (const auto& [key, value] : attributes) canonical << ':' << key << '=' << value;
        awards.emplace_back(hex(sha256(bytes_of(canonical.str()))), amount);
    }
    if (awards.empty()) return;
    load();
    const auto balances_before = balances_;
    const auto receipts_before = rewarded_;
    bool changed = false;
    try {
        for (const auto& [receipt, amount] : awards) {
            if (!rewarded_.insert({std::to_string(account), receipt}).second) continue;
            auto& balance = balances_[{std::to_string(account), 10}];
            if (balance < 0 || balance > INT64_MAX - amount) throw Rejected("reward balance overflow");
            balance += amount;
            changed = true;
        }
        if (changed) save();
    } catch (...) {
        balances_ = balances_before;
        rewarded_ = receipts_before;
        throw;
    }
}
void MarketplaceStore::purchase(const std::vector<Param>& params, std::uint64_t account) {
    // Observed single/bundle request: parallel SKU, quantity and recipient arrays.
    if (params.size() < 13 || params[0].kind != typed::STRING || params[0].value.text != MARKET_CONTEXT ||
        params[1].kind != typed::UINT32)
        throw Rejected("unsupported purchase request");
    const auto count = params[1].value.as_u64();
    if (!count || count > 32 || params.size() != 9 + 4 * count) throw Rejected("unsupported purchase size");
    std::size_t pos = 2;
    auto number = [&](int kind, std::uint64_t expected) {
        const auto& p = params.at(pos++);
        if (p.kind != kind || !equals_int(p.value, expected)) throw Rejected("unsupported purchase field");
    };
    std::vector<std::uint64_t> requested;
    std::set<std::uint64_t> unique;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& p = params.at(pos++);
        if (p.kind != typed::UINT32 || !unique.insert(p.value.as_u64()).second)
            throw Rejected("duplicate or invalid SKU");
        requested.push_back(p.value.as_u64());
    }
    number(typed::UINT32, count);
    for (std::size_t i = 0; i < count; ++i) number(typed::UINT32, 1);
    number(1, 0);
    number(typed::UINT32, count);
    for (std::size_t i = 0; i < count; ++i) {
        number(typed::UINT64, 0);
        const auto& p = params.at(pos++);
        if (p.kind != typed::STRING || !p.value.text.empty()) throw Rejected("purchase recipient unsupported");
    }
    number(typed::UINT32, 0);
    number(typed::UINT32, 0);
    number(typed::UINT8, 0);
    number(typed::UINT32, 0);
    std::ifstream in(path_.parent_path() / "retail_shop_catalog.json");
    const Json catalogue = parse_json(std::string(std::istreambuf_iterator<char>(in), {}));
    if (catalogue.at("schema").as_u64() != 1 ||
        catalogue.at("eboot_sha256").text != "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5" ||
        !catalogue.at("skus").is_array())
        throw Rejected("unsupported shop catalogue");
    std::map<std::uint64_t, const Json*> skus;
    for (const auto& candidate : catalogue.at("skus").items)
        if (!skus.emplace(candidate.at("id").as_u64(), &candidate).second) throw Rejected("ambiguous shop SKU");
    auto lookup = [&](std::uint64_t id) -> const Json& {
        auto it = skus.find(id);
        if (it == skus.end()) throw Rejected("unknown shop SKU");
        return *it->second;
    };
    load();
    const auto owner = std::to_string(account);
    auto owned = [&](std::uint64_t item) {
        auto it = inventory_.find({owner, item});
        return it != inventory_.end() && it->second > 0;
    };
    const auto& first = lookup(requested.front());
    std::set<std::uint64_t> expected;
    if (const auto* components = first.find("components")) {
        if (!components->is_array() || components->items.empty() || components->items.size() > 31)
            throw Rejected("invalid bundle");
        for (const auto& id : components->items) {
            const auto& component = lookup(id.as_u64());
            if (component.find("components")) throw Rejected("nested bundle unsupported");
            if (!owned(component.at("item").as_u64())) expected.insert(id.as_u64());
        }
        // A retry may still include components already granted. Every unowned component
        // must be present and no unrelated component may be submitted.
        std::set<std::uint64_t> allowed;
        for (const auto& id : components->items) allowed.insert(id.as_u64());
        for (std::size_t i = 1; i < requested.size(); ++i)
            if (!allowed.count(requested[i])) throw Rejected("SKU outside bundle");
        for (auto id : expected)
            if (!unique.count(id)) throw Rejected("incomplete bundle");
    } else {
        if (count != 1) throw Rejected("multiple purchases require bundle");
        expected.insert(requested.front());
    }
    std::int64_t price = 0;
    std::set<std::uint64_t> grants;
    for (auto id : expected) {
        const auto& sku = lookup(id);
        const auto item = sku.at("item").as_u64();
        const auto cost = sku.at("price").as_i64();
        if (sku.at("currency").as_u64() != 10 || sku.at("quantity").as_u64() != 1 || !item || item > UINT32_MAX ||
            cost <= 0 || cost > INT32_MAX)
            throw Rejected("invalid shop item/price");
        if (!owned(item)) {
            if (!grants.insert(item).second) throw Rejected("duplicate bundle item");
            price += cost;
        }
    }
    if (grants.empty()) return;
    const auto money = balances_.find({owner, 10});
    if (money == balances_.end() || money->second < price) throw Rejected("insufficient Wumpa balance");
    const auto balances_before = balances_, inventory_before = inventory_;
    try {
        balances_[{owner, 10}] -= price;
        for (auto item : grants) inventory_[{owner, item}] = 1;
        save();
    } catch (...) {
        balances_ = balances_before;
        inventory_ = inventory_before;
        throw;
    }
}
Bytes MarketplaceStore::inventory_page(const std::vector<Param>& params, std::uint64_t account) {
    load();
    if (kinds(params) != std::vector<int>{typed::STRING, typed::UINT32, typed::UINT32} ||
        params[0].value.text != MARKET_CONTEXT)
        throw Rejected("unsupported inventory request");
    if (params[1].value.as_u64() != 1 || params[2].value.as_u64() == 0 || params[2].value.as_u64() > 4096)
        throw Rejected("unsupported inventory page");
    const std::string owner = std::to_string(account);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> entries;
    for (const auto& entry : inventory_) {
        if (entry.first.first != owner || entry.second <= 0) continue;
        if (entry.first.second > UINT32_MAX || entry.second > UINT32_MAX)
            throw Rejected("inventory value out of range");
        entries.emplace_back(static_cast<std::uint32_t>(entry.first.second), static_cast<std::uint32_t>(entry.second));
    }
    if (entries.empty()) return {};
    // Enable nonempty replies only with the local catalogue used by the purchase path.
    if (!std::filesystem::exists(path_.parent_path() / "retail_shop_catalog.json"))
        throw Rejected("nonempty inventory requires local shop catalogue");
    if (entries.size() > params[2].value.as_u64()) throw Rejected("inventory pagination not supported");
    Writer out;
    out.u32(static_cast<std::uint32_t>(entries.size())).u32(static_cast<std::uint32_t>(entries.size()));
    for (const auto& [item, quantity] : entries) {
        out.u64(account).string("psn").u32(item).u32(quantity).u32(0).blob({}).u32(0);
        out.raw({typed::INT64, 255, 255, 255, 255, 255, 255, 255, 255});
        out.raw({typed::UINT16, 0, 0}).u32(0);
    }
    return out.data();
}
Bytes MarketplaceStore::balances_message(const std::vector<Param>& params, std::uint64_t account) {
    load();
    const std::vector<PbField> request = struct_request(params);
    const PbField* context = last_field(request, 1);
    const PbField* limit = last_field(request, 2);
    if (!context || !is_context(*context) || !limit || limit->wire != 0 || limit->number == 0 || limit->number > 16)
        throw Rejected("unsupported balances request fields");
    const std::string owner = std::to_string(account);
    Bytes message;
    std::uint64_t rows = 0;
    for (const auto& [key, amount] : balances_) { // ordered by currency within an account
        if (key.first != owner) continue;
        if (rows++ == limit->number) break;
        const std::uint64_t zigzag =
            (static_cast<std::uint64_t>(amount) << 1) ^ static_cast<std::uint64_t>(amount >> 63);
        Bytes entry;
        append_bytes(entry, {pb_varint(1 << 3), pb_varint(key.second), pb_varint(2 << 3), pb_varint(zigzag)});
        append_bytes(message, {pb_varint(1 << 3 | 2), pb_varint(entry.size()), entry});
    }
    return message;
}

AchievementStore::AchievementStore(std::filesystem::path path) : path_(std::move(path)) {}
Json AchievementStore::report_events(const std::vector<Param>& params, std::uint64_t account) {
    const std::vector<PbField> values = struct_request(params);
    require_context(values);
    Json events = Json::array();
    for (const PbField& f : values) {
        if (f.tag != 2 || f.wire != 2) continue;
        // {t: v for t, w, v in fields(value) if w == 0 or t == 1}: last name, last numeric time.
        const PbField* name = nullptr;
        const PbField* time = nullptr;
        const std::vector<PbField> event_fields = pb_fields(f.data);
        for (const PbField& e : event_fields) {
            if (e.tag == 1)
                name = &e;
            else if (e.tag == 2 && e.wire == 0)
                time = &e;
        }
        if (!name || name->wire == 0) throw Rejected("achievement event without name");
        const std::string text(name->data.begin(), name->data.end());
        if (utf8_replace(text) != text) throw Rejected("achievement event name is not UTF-8");
        Json event = Json::object();
        event.set("name", Json::of(text));
        event.set("time", time ? Json::of(time->number) : Json::null());
        events.items.push_back(std::move(event));
    }
    if (events.items.empty()) throw Rejected("achievement report without events");
    std::filesystem::create_directories(path_.parent_path());
    std::ofstream out(path_, std::ios::app | std::ios::binary);
    const std::uint64_t received = static_cast<std::uint64_t>(std::time(nullptr));
    for (const Json& event : events.items) {
        Json row = Json::object();
        row.set("account", Json::of(std::to_string(account)));
        row.set("received", Json::of(received));
        row.set("event", Json::of(dump_json_spaced(event)));
        // Keep the complete retail struct request, including event attributes and
        // unknown fields, so reward/challenge evidence survives summary parsing.
        row.set("request_hex", params[0].value);
        out << dump_json(row) << '\n';
    }
    out.flush();
    if (!out) throw Rejected("achievement event persistence failed");
    return events;
}
Bytes AchievementStore::states_message(const std::vector<Param>& params, std::uint64_t) const {
    const std::vector<PbField> values = struct_request(params);
    require_context(values);
    std::vector<std::uint64_t> kind, limit;
    std::vector<const PbField*> token;
    for (const PbField& f : values) {
        if (f.tag == 2 && f.wire == 0) kind.push_back(f.number);
        if (f.tag == 3 && f.wire == 0) limit.push_back(f.number);
        if (f.tag == 4) token.push_back(&f);
    }
    if (kind.size() != 1 || kind[0] >= 8 || limit != std::vector<std::uint64_t>{100} ||
        !(token.empty() || (token.size() == 1 && token[0]->wire != 0 && token[0]->data.empty())))
        throw Rejected("unsupported achievement states request");
    for (const PbField& f : values)
        if (f.tag >= 5 && f.tag <= 7) throw Rejected("achievement state filters not supported");
    return {}; // no states, no next page token
}

std::optional<Bytes> pitstop_task(const TaskRequest& request, std::uint64_t account, MarketplaceStore& marketplace,
                                  AchievementStore& achievements, unsigned multiplier) {
    const unsigned service = request.service;
    const std::uint64_t task = request.task;
    if (!((service == 80 && (task == 154 || task == 165 || task == 245)) ||
          (service == 125 && (task == 1 || task == 3))))
        return std::nullopt;
    try {
        if (service == 80 && task == 154) {
            marketplace.purchase(request.params, account);
            return task_reply(task);
        }
        if (service == 80 && task == 165) {
            const auto records = marketplace.inventory_page(request.params, account);
            if (records.empty()) return task_reply(task);
            Writer out;
            out.raw(records);
            return task_reply(task, &out);
        }
        if (service == 80) return struct_reply(task, marketplace.balances_message(request.params, account));
        if (task == 1) {
            achievements.report_events(request.params, account);
            marketplace.award_races(request.params, account, multiplier);
            return task_reply(task);
        }
        return struct_reply(task, achievements.states_message(request.params, account));
    } catch (const std::exception&) {
        return task_reply(task, nullptr, UNSERVED_ERROR);
    }
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
    static const std::set<std::string> allowed = {"session-request", "session-image", "session-data",
                                                  "changeable-session-data"};
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
    if (!metadata.is_object() || !privacy || !privacy->is_string() ||
        (privacy->text != "private" && privacy->text != "public") || !max_user || !max_user->is_integer() ||
        signed_of(*max_user) < 1 || signed_of(*max_user) > 8 || !name || !name->is_string() || !platforms ||
        *platforms != ps4)
        throw Rejected("unsupported session metadata");
    const std::string id = uuid4_hex();
    parts.erase(parts.begin());
    sessions_[id] = Session{account, metadata, std::move(parts), {{account, ""}}, {account}, {}};
    sessions_[id].metadata.set("sessionCreateTimestamp",
                               Json::of(static_cast<std::uint64_t>(std::time(nullptr)) * 1000));
    if (!metadata.find("sessionLockFlag")) sessions_[id].metadata.set("sessionLockFlag", Json::of(false));
    return single("sessionId", Json::of(id));
}

void SessionStore::update(std::uint64_t account, const std::string& id, const Bytes& body) {
    const auto found = sessions_.find(id);
    if (found == sessions_.end() || found->second.owner != account) throw Rejected("unknown session or not its owner");
    const Json change = strip_bom_parse(body);
    if (!change.is_object() || change.members.empty()) throw Rejected("unsupported session update");
    for (const auto& [name, value] : change.members) {
        const bool ok =
            (name == "sessionPrivacy" && value.is_string() && (value.text == "public" || value.text == "private")) ||
            (name == "sessionMaxUser" && value.is_integer() && signed_of(value) >= 1 && signed_of(value) <= 8) ||
            (name == "sessionLockFlag" && value.is_bool()) || (name == "sessionStatus" && value.is_string());
        if (!ok) throw Rejected("unsupported session update field " + name);
    }
    for (const auto& [name, value] : change.members) found->second.metadata.set(name, value);
}

void SessionStore::expire_invitations(std::uint64_t now) {
    for (auto it = invitations_.begin(); it != invitations_.end();)
        it = it->second.expires <= now ? invitations_.erase(it) : std::next(it);
}
Json SessionStore::invite(std::uint64_t account, const std::string& sender, const std::string& session,
                          const std::vector<std::uint64_t>& recipients, std::uint64_t now) {
    expire_invitations(now);
    const auto owner = sessions_.find(session);
    if (owner == sessions_.end() || owner->second.owner != account || !owner->second.members.count(account) ||
        owner->second.members.at(account) != sender)
        throw Rejected("invitation sender must own session");
    if (recipients.empty() || recipients.size() > 16) throw Rejected("invalid invitation recipient count");
    const std::set<std::uint64_t> unique(recipients.begin(), recipients.end());
    if (unique.size() != recipients.size() || unique.count(0) || unique.count(account))
        throw Rejected("invalid invitation recipients");
    // Validate the whole batch before insertion; retries reuse an outstanding invitation.
    for (auto recipient : recipients) {
        std::size_t count = 0;
        bool retry = false;
        for (const auto& [id, invitation] : invitations_) {
            if (invitation.recipient == recipient) {
                ++count;
                retry |= invitation.session == session && invitation.account == account && !invitation.accepted;
            }
        }
        if (!retry && count >= 64) throw Rejected("recipient invitation queue full");
    }
    Json sent = Json::array();
    for (auto recipient : recipients) {
        auto existing = std::find_if(invitations_.begin(), invitations_.end(), [&](const auto& entry) {
            return entry.second.recipient == recipient && entry.second.session == session &&
                   entry.second.account == account && !entry.second.accepted;
        });
        std::string id;
        if (existing != invitations_.end())
            id = existing->first;
        else {
            do {
                id = hex(random_bytes(16));
            } while (invitations_.count(id));
            invitations_.emplace(id, Invitation{session, sender, account, recipient, now + 300});
        }
        Json result = Json::object();
        result.set("invitationId", Json::of(id));
        result.set("accountId", Json::of(std::to_string(recipient)));
        sent.items.push_back(result);
    }
    return sent;
}
Json SessionStore::invitations(std::uint64_t recipient, std::uint64_t now) {
    expire_invitations(now);
    Json result = Json::array();
    for (const auto& [id, invite] : invitations_) {
        if (invite.recipient != recipient || invite.accepted) continue;
        Json row = Json::object();
        row.set("invitationId", Json::of(id));
        row.set("sessionId", Json::of(invite.session));
        row.set("onlineId", Json::of(invite.sender));
        row.set("accountId", Json::of(std::to_string(invite.account)));
        result.items.push_back(row);
    }
    return result;
}
void SessionStore::dismiss(std::uint64_t recipient, const std::string& invitation, std::uint64_t now) {
    expire_invitations(now);
    const auto it = invitations_.find(invitation);
    if (it == invitations_.end() || it->second.recipient != recipient)
        throw Rejected("unknown invitation or not its recipient");
    invitations_.erase(it);
}
Json SessionStore::accept(std::uint64_t recipient, const std::string& id, std::uint64_t now) {
    expire_invitations(now);
    const auto it = invitations_.find(id);
    if (it == invitations_.end() || it->second.recipient != recipient) throw Rejected("unknown invitation");
    const auto session = sessions_.find(it->second.session);
    if (session == sessions_.end()) throw Rejected("session closed");
    if (const auto* locked = session->second.metadata.find("sessionLockFlag"); locked && locked->boolean)
        throw Rejected("session locked");
    begin_connection(recipient, it->second.session, now);
    session->second.admitted.insert(recipient);
    it->second.accepted = true;
    Json result = Json::object();
    result.set("sessionId", Json::of(it->second.session));
    result.set("invitationId", Json::of(id));
    return result;
}
const SessionStore::Session& SessionStore::readable(std::uint64_t account, const std::string& id) const {
    const auto it = sessions_.find(id);
    if (it == sessions_.end()) throw Rejected("unknown session");
    const auto& session = it->second;
    const bool invited = std::any_of(invitations_.begin(), invitations_.end(), [&](const auto& item) {
        return item.second.session == id && item.second.recipient == account &&
               item.second.expires > static_cast<std::uint64_t>(std::time(nullptr));
    });
    if (!session.admitted.count(account) && !session.members.count(account) && !invited &&
        session.metadata.at("sessionPrivacy").text != "public")
        throw Rejected("private session");
    return session;
}
Json SessionStore::describe(std::uint64_t account, const std::string& id) const {
    const auto& session = readable(account, id);
    Json result = session.metadata;
    result.set("sessionId", Json::of(id));
    Json members = Json::array();
    for (const auto& [member, name] : session.members) {
        Json row = Json::object();
        row.set("accountId", Json::of(std::to_string(member)));
        row.set("onlineId", Json::of(name));
        row.set("platform", Json::of("PS4"));
        members.items.push_back(row);
        if (member == session.owner) result.set("sessionCreator", row);
    }
    result.set("members", members);
    result.set("memberCount", Json::of(static_cast<unsigned>(members.items.size())));
    return result;
}
Part SessionStore::attachment(std::uint64_t account, const std::string& id, const std::string& name) const {
    const auto& session = readable(account, id);
    for (const auto& part : session.parts)
        if (part.name == name) return part;
    throw Rejected("session attachment absent");
}
void SessionStore::join(std::uint64_t account, const std::string& name, const std::string& id) {
    readable(account, id);
    auto& session = sessions_.at(id);
    if (!session.members.count(account)) {
        if (const auto* locked = session.metadata.find("sessionLockFlag"); locked && locked->boolean)
            throw Rejected("session locked");
        if (session.members.size() >= session.metadata.at("sessionMaxUser").as_u64()) throw Rejected("session full");
    }
    session.members[account] = name;
}
void SessionStore::leave(std::uint64_t account, const std::string& id) {
    const auto it = sessions_.find(id);
    if (it == sessions_.end()) throw Rejected("unknown session");
    if (it->second.owner == account) {
        erase(account, id);
        return;
    }
    const bool connecting = connecting_.erase({id, account}) != 0;
    if (!it->second.members.erase(account) && !connecting) throw Rejected("not a session member");
    it->second.admitted.erase(account);
    it->second.priorities.erase(account);
}
void SessionStore::erase(std::uint64_t account, const std::string& id) {
    const auto it = sessions_.find(id);
    if (it == sessions_.end() || it->second.owner != account) throw Rejected("not session owner");
    sessions_.erase(it);
    for (auto pending = connecting_.begin(); pending != connecting_.end();)
        pending = pending->first.first == id ? connecting_.erase(pending) : std::next(pending);
    for (auto invite = invitations_.begin(); invite != invitations_.end();)
        invite = invite->second.session == id ? invitations_.erase(invite) : std::next(invite);
}
void SessionStore::begin_connection(std::uint64_t account, const std::string& id, std::uint64_t now) {
    const auto& session = readable(account, id);
    if (session.members.count(account)) return;
    if (const auto* locked = session.metadata.find("sessionLockFlag"); locked && locked->boolean)
        throw Rejected("session locked");
    if (session.members.size() >= session.metadata.at("sessionMaxUser").as_u64()) throw Rejected("session full");
    for (auto it = connecting_.begin(); it != connecting_.end();)
        it = it->second <= now ? connecting_.erase(it) : std::next(it);
    connecting_[{id, account}] = now + 30;
}
std::set<std::uint64_t> SessionStore::connection_peers(std::uint64_t account, std::uint64_t now) const {
    std::set<std::uint64_t> result;
    for (const auto& [id, session] : sessions_) {
        const auto pending = connecting_.find({id, account});
        if (!session.members.count(account) && (pending == connecting_.end() || pending->second <= now)) continue;
        for (const auto& [member, name] : session.members) result.insert(member);
        for (const auto& [key, until] : connecting_)
            if (key.first == id && until > now) result.insert(key.second);
    }
    result.erase(account);
    return result;
}
std::set<std::uint64_t> MatchmakingStore::connection_peers(std::uint64_t account, const std::string& selector) const {
    std::set<std::uint64_t> result;
    for (const auto& [id, lobby] : lobbies_) {
        if (lobby.selector != selector || lobby.departed.count(account) ||
            std::find(lobby.members.begin(), lobby.members.end(), account) == lobby.members.end())
            continue;
        for (const auto member : lobby.members)
            if (member != account && !lobby.departed.count(member)) result.insert(member);
    }
    return result;
}
void SessionStore::replace_attachment(std::uint64_t account, const std::string& id, Part part) {
    const auto it = sessions_.find(id);
    if (it == sessions_.end() || it->second.owner != account) throw Rejected("not session owner");
    if (part.data.size() > 2 * 1024 * 1024 || (part.name != "session-image" && part.name != "changeable-session-data"))
        throw Rejected("invalid mutable session attachment");
    for (auto& old : it->second.parts)
        if (old.name == part.name) {
            old = std::move(part);
            return;
        }
    it->second.parts.push_back(std::move(part));
}
Json SessionStore::user_sessions(std::uint64_t requester, std::uint64_t account) const {
    Json rows = Json::array();
    for (const auto& [id, session] : sessions_) {
        if (!session.members.count(account)) continue;
        try {
            auto row = describe(requester, id);
            row.set("index", Json::of(static_cast<unsigned>(rows.items.size())));
            const auto priority = session.priorities.find(account);
            row.set("priority", Json::of(priority == session.priorities.end() ? 0 : priority->second));
            rows.items.push_back(row);
        } catch (const Rejected&) {} // Private sessions are omitted for non-invited callers.
    }
    return rows;
}
void SessionStore::member_priority(std::uint64_t account, const std::string& id, std::uint64_t priority) {
    const auto it = sessions_.find(id);
    if (it == sessions_.end() || !it->second.members.count(account) || priority > UINT32_MAX)
        throw Rejected("invalid member priority");
    it->second.priorities[account] = priority;
}
Json SessionStore::invitation(std::uint64_t recipient, const std::string& id, std::uint64_t now) {
    expire_invitations(now);
    const auto it = invitations_.find(id);
    if (it == invitations_.end() || it->second.recipient != recipient) throw Rejected("unknown invitation");
    // Invitation grants access even before acceptance, while it remains valid.
    Json row = Json::object(), from = Json::object();
    from.set("onlineId", Json::of(it->second.sender));
    from.set("accountId", Json::of(std::to_string(it->second.account)));
    row.set("fromUser", from);
    row.set("invitationId", Json::of(id));
    row.set("sessionId", Json::of(it->second.session));
    row.set("usedFlag", Json::of(it->second.accepted));
    row.set("expired", Json::of(false));
    row.set("session", describe(recipient, it->second.session));
    return row;
}

void MatchmakingStore::advertise(const std::vector<Param>& params, std::uint64_t account, const std::string& selector) {
    if (!valid_room_token(selector)) throw Rejected("valid room selector required");
    if (params.size() != 1 || params[0].kind != typed::STRING || !params[0].value.is_string())
        throw Rejected("expected one player-info string");
    const std::string& text = params[0].value.text;
    if (utf8_size(text) > 65535) throw Rejected("oversize player-info document");
    const Json document = parse_json(text);
    if (!keys_are(document, {"listen_server", "dedicated_server"})) throw Rejected("unsupported player-info schema");
    const Json& listen = document.at("listen_server");
    if (!keys_are(listen, {"local_address", "security_id", "security_key", "nat"}))
        throw Rejected("unsupported listen-server schema");
    for (const auto& [key, size] :
         {std::pair<const char*, std::size_t>{"local_address", 67}, {"security_id", 8}, {"security_key", 16}})
        if (base64_decode(text_of(listen.at(key))).size() != size) throw Rejected("invalid player-info encoding");
    const Json& nat = listen.at("nat");
    if (!nat.is_integer() || !(equals_int(nat, 1) || equals_int(nat, 2) || equals_int(nat, 3)))
        throw Rejected("unsupported NAT value");
    const Json& dedicated = document.at("dedicated_server");
    if (!keys_are(dedicated, {"build_name"}) || !dedicated.at("build_name").is_string())
        throw Rejected("unsupported dedicated-server schema");
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
Bytes hex_field(const Json& value) {
    return from_hex(text_of(value));
}
std::uint32_t u32_field(const Json& value) {
    if (!value.is_unsigned() || value.as_u64() > 0xffffffffull) throw Rejected("invalid 32-bit field");
    return static_cast<std::uint32_t>(value.as_u64());
}
} // namespace
Bytes MatchmakingStore::register_session(const std::vector<Param>& params, std::uint64_t account,
                                         const std::string& selector) {
    if (!valid_room_token(selector) || params.size() != 5 ||
        kinds(params) != std::vector<int>{typed::STRING, typed::BLOB, typed::UINT32, typed::UINT32, typed::BLOB})
        throw Rejected("unsupported session registration schema");
    const std::string& context = text_of(params[0].value);
    if (context.empty() || context.size() > 16) throw Rejected("invalid session context");
    const Bytes address = hex_field(params[1].value), custom = hex_field(params[4].value);
    if (address.size() != 67 || custom.empty() || custom.size() > 65535) throw Rejected("invalid session fields");
    const std::uint32_t a = u32_field(params[2].value), b = u32_field(params[3].value);
    // A host registering again in the same context replaces its previous session.
    for (auto it = sessions_.begin(); it != sessions_.end();)
        it = it->second.owner == account && it->second.context == context ? sessions_.erase(it) : std::next(it);
    while (true) {
        const Bytes session = random_bytes(8);
        if (std::any_of(session.begin(), session.end(), [](unsigned char c) { return c != 0; }) &&
            !sessions_.count(session)) {
            sessions_[session] = MatchSession{selector, context, account, MatchPlayer{address, a, b, custom}, 1, false};
            return session;
        }
    }
}
void MatchmakingStore::update_session_players(const std::vector<Param>& params, std::uint64_t account,
                                              const std::string& selector) {
    if (!valid_room_token(selector) || params.size() < 3 ||
        kinds(params, 0, 3) != std::vector<int>{typed::STRING, typed::BLOB, typed::UINT32})
        throw Rejected("invalid session player header");
    const std::string& context = text_of(params[0].value);
    const Bytes session = hex_field(params[1].value);
    // Layout: context, session ID, u32 number of players, then the session's full information
    // (host address, game type, maximum players, attributes) once, whatever the player count.
    const std::uint64_t count = params[2].value.as_u64();
    if (count < 1 || count > 8 || params.size() != 7 || session.size() != 8)
        throw Rejected("invalid session player count");
    const auto it = sessions_.find(session);
    if (it == sessions_.end() || it->second.owner != account || it->second.selector != selector ||
        it->second.context != context)
        throw Rejected("session owner or room mismatch");
    if (kinds(params, 3, 4) != std::vector<int>{typed::BLOB, typed::UINT32, typed::UINT32, typed::BLOB})
        throw Rejected("invalid session player fields");
    const Bytes address = hex_field(params[3].value), custom = hex_field(params[6].value);
    if (address.size() != 67 || custom.empty() || custom.size() > 65535)
        throw Rejected("invalid session player values");
    it->second.self = MatchPlayer{address, u32_field(params[4].value), u32_field(params[5].value), custom};
    it->second.players = static_cast<std::uint32_t>(count);
}
Bytes MatchmakingStore::reserve_session(const std::vector<Param>& params, std::uint64_t account,
                                        const std::string& selector) {
    if (!valid_room_token(selector) || params.size() != 1 || params[0].kind != typed::STRING)
        throw Rejected("unsupported session reservation");
    const std::string& context = text_of(params[0].value);
    if (context.empty() || context.size() > 16) throw Rejected("invalid session context");
    for (auto it = sessions_.begin(); it != sessions_.end();)
        it = it->second.owner == account && it->second.context == context && it->second.reserved ? sessions_.erase(it)
                                                                                                 : std::next(it);
    while (true) {
        const Bytes session = random_bytes(8);
        if (std::any_of(session.begin(), session.end(), [](unsigned char c) { return c != 0; }) &&
            !sessions_.count(session)) {
            sessions_[session] = MatchSession{selector, context, account, MatchPlayer{}, 0, true};
            return session;
        }
    }
}
void MatchmakingStore::update_session(const std::vector<Param>& params, std::uint64_t account, bool initialize) {
    if (params.size() != 6 || kinds(params) != std::vector<int>{typed::STRING, typed::BLOB, typed::BLOB, typed::UINT32,
                                                                typed::UINT32, typed::BLOB})
        throw Rejected("unsupported session update schema");
    const auto it = sessions_.find(hex_field(params[1].value));
    if (it == sessions_.end() || it->second.owner != account || it->second.context != text_of(params[0].value))
        throw Rejected("unknown session or not its owner");
    const Bytes address = hex_field(params[2].value), custom = hex_field(params[5].value);
    if (address.size() != 67 || custom.empty() || custom.size() > 65535) throw Rejected("invalid session fields");
    it->second.self = MatchPlayer{address, u32_field(params[3].value), u32_field(params[4].value), custom};
    if (initialize) {
        it->second.reserved = false;
        it->second.players = 1;
    }
}
void MatchmakingStore::delete_session(const std::vector<Param>& params, std::uint64_t account) {
    if (params.size() != 2 || kinds(params) != std::vector<int>{typed::STRING, typed::BLOB})
        throw Rejected("unsupported session deletion");
    const auto it = sessions_.find(hex_field(params[1].value));
    if (it == sessions_.end() || it->second.owner != account || it->second.context != text_of(params[0].value))
        throw Rejected("unknown session or not its owner");
    sessions_.erase(it);
}
std::vector<MatchmakingStore::FriendSession>
MatchmakingStore::friend_sessions(const std::string& context, const std::vector<std::uint64_t>& accounts,
                                  std::uint64_t requester) const {
    std::vector<FriendSession> found;
    for (const std::uint64_t account : accounts) {
        if (account == requester) continue;
        for (const auto& [id, session] : sessions_) {
            if (session.owner != account || session.context != context || session.reserved) continue;
            found.push_back(
                {session.self.address, id, session.self.a, session.self.b, session.players, session.self.custom});
        }
    }
    return found;
}
std::vector<std::pair<std::string, MatchmakingStore::FriendSession>>
MatchmakingStore::owned_sessions(std::uint64_t account) const {
    std::vector<std::pair<std::string, FriendSession>> found;
    for (const auto& [id, session] : sessions_)
        if (session.owner == account && !session.reserved)
            found.push_back(
                {session.context,
                 {session.self.address, id, session.self.a, session.self.b, session.players, session.self.custom}});
    return found;
}
std::uint64_t MatchmakingStore::start_search(const std::vector<Param>& params, std::uint64_t account,
                                             const std::string& selector) {
    if (params.size() != 1 || params[0].kind != typed::STRING) throw Rejected("expected one search document");
    Json document = parse_json(text_of(params[0].value));
    if (!keys_are(document, {"members", "tokens", "lobby_slots", "dedicated_server", "team_balance", "ruleset_payload",
                             "player_id_to_slots", "performance"}))
        throw Rejected("unsupported search schema");
    // First verified scope: one local player per instance, no guests or teams.
    const Json& members = document.at("members");
    if (!members.is_array() || members.items.size() != 1 || !equals_u64(members.items[0], account))
        throw Rejected("search member or token mismatch");
    const Json& tokens = document.at("tokens");
    const std::uint64_t token = player_token(account);
    if (!tokens.is_array() || tokens.items.size() != 1 || !equals_u64(tokens.items[0], token))
        throw Rejected("search member or token mismatch");
    const auto room = player_rooms_.find(account);
    if (!valid_room_token(selector) || room == player_rooms_.end() || room->second != selector)
        throw Rejected("search room mismatch");
    const Json& slots = document.at("lobby_slots");
    if (!keys_are(slots, {"min", "max"}) || !slots.at("min").is_integer() || !slots.at("max").is_integer() ||
        !(2 <= signed_of(slots.at("min")) && signed_of(slots.at("min")) <= signed_of(slots.at("max")) &&
          signed_of(slots.at("max")) <= 8))
        throw Rejected("unsupported lobby slots");
    Json slot_map = Json::object();
    slot_map.set(std::to_string(account), Json::of(0));
    if (document.at("dedicated_server") != single("request_level", Json::of("not_needed")) ||
        document.at("team_balance") != single("num_teams", Json::of(0)) ||
        document.at("player_id_to_slots") != slot_map)
        throw Rejected("unsupported dedicated server, team or guest schema");
    const Json& filters = document.at("ruleset_payload");
    const Json& performance = document.at("performance");
    bool filters_ok =
        keys_are(filters, {"filter"}) && keys_are(filters.at("filter"), {"matchmaking_version", "playlist_id"});
    if (filters_ok)
        for (const auto& [name, value] : filters.at("filter").members)
            if (!value.is_integer() || value.kind == Json::Kind::Int) filters_ok = false;
    if (!filters_ok || !keys_are(performance, {"game_type"}) || !performance.at("game_type").is_unsigned() ||
        performance.at("game_type").as_u64() > UINT32_MAX)
        throw Rejected("unsupported search filters");
    Json compatibility_list = Json::array();
    compatibility_list.items = {slots, filters, performance};
    const std::string compatibility = dump_json_sorted(compatibility_list);
    const std::string text = dump_json(document);
    for (const Search& s : searches_)
        if (s.account == account && s.selector == selector && s.document == text && s.status == "pending") return s.id;
    for (auto& [id, lobby] : lobbies_) lobby.departed.insert(account);
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
std::vector<LobbyPlan> MatchmakingStore::match_ready(const std::map<std::uint64_t, std::string>& available,
                                                     const std::map<std::string, std::size_t>& expected,
                                                     const std::set<std::uint64_t>& reachable) {
    std::vector<std::pair<std::pair<std::string, std::string>, std::vector<Search*>>> groups;
    for (Search& s : searches_) {
        if (s.status != "pending") continue;
        const auto it = available.find(s.account);
        if (it == available.end() || it->second != s.selector) continue;
        const auto key = std::make_pair(s.selector, s.compatibility);
        auto group = std::find_if(groups.begin(), groups.end(), [&](const auto& g) { return g.first == key; });
        if (group == groups.end()) {
            groups.push_back({key, {}});
            group = groups.end() - 1;
        }
        group->second.push_back(&s);
    }
    std::vector<LobbyPlan> plans;
    for (auto& [key, players] : groups) {
        const Json slots = parse_json(players[0]->document).at("lobby_slots");
        std::int64_t min_players = signed_of(slots.at("min"));
#pragma warning(suppress : 4996)
        if (const char* env = std::getenv("CTR_ONLINE_MIN_PLAYERS")) {
            try {
                min_players = std::max<std::int64_t>(min_players, std::stoll(env));
            } catch (...) {}
        }
        if (const auto party = expected.find(key.first); party != expected.end())
            min_players = std::max(min_players, static_cast<std::int64_t>(party->second));
        if (static_cast<std::int64_t>(players.size()) < min_players) continue;
        if (min_players > signed_of(slots.at("max"))) continue;
        if (static_cast<std::int64_t>(players.size()) > signed_of(slots.at("max")))
            players.resize(static_cast<std::size_t>(signed_of(slots.at("max"))));
        std::uint64_t lobby;
        do lobby = random_bits(52);
        while (!lobby || lobbies_.count(lobby));
        LobbyPlan plan;
        plan.lobby_id = lobby;
        plan.selector = key.first;
        // Guests connect to the host; a host behind a NAT that refuses incoming connections
        // (operator NAT, phone hotspot) can be reached by nobody.
        const auto host = std::find_if(players.begin(), players.end(),
                                       [&](const Search* s) { return reachable.count(s->account) != 0; });
        Lobby record{key.first, (host != players.end() ? *host : players[0])->account, {}};
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
std::string MatchmakingStore::migration_routes(std::uint64_t lobby) const {
    const auto& room = lobbies_.at(lobby);
    std::string result = "v1|";
    for (const auto account : room.members) {
        if (room.departed.count(account) || !player_info_.count(account) || player_rooms_.at(account) != room.selector)
            continue;
        const auto info = parse_json(player_info_.at(account));
        const auto address = base64_decode(info.at("listen_server").at("local_address").text);
        result += std::to_string(account) + "=" + hex(address) + ";";
    }
    return result;
}
void MatchmakingStore::save_documents(std::uint64_t lobby, const Json& host, const Json& backend,
                                      std::uint64_t update_id) {
    documents_[lobby] = LobbyDocuments{host, backend, update_id};
    documents_.at(lobby).host.set("ctr_migration", Json::of(migration_routes(lobby)));
}
bool MatchmakingStore::sync_document(const std::vector<Param>& params, std::uint64_t account,
                                     const std::string& selector) {
    if (params.size() != 3 || kinds(params) != std::vector<int>{typed::UINT64, typed::UINT64, typed::STRING})
        throw Rejected("invalid lobby synchronization fields");
    const std::uint64_t lobby = params[0].value.as_u64(), expected = params[1].value.as_u64();
    const auto row = lobbies_.find(lobby);
    const auto current = documents_.find(lobby);
    if (row == lobbies_.end() || row->second.host != account || row->second.selector != selector ||
        current == documents_.end())
        throw Rejected("lobby host or room mismatch");
    if (current->second.update_id != expected) return false;
    Json document = parse_json(text_of(params[2].value));
    static const std::set<std::string> keys = {
        "update_time",  "update_id",       "lobby_open", "lobby_open_for_pres_join", "player_state", "listen_server",
        "team_balance", "ruleset_payload", "attachment"};
    if (!document.is_object()) throw Rejected("unsupported host document schema");
    for (const std::string& key : keys)
        if (!document.find(key)) throw Rejected("unsupported host document schema");
    // ctr_migration is the server's own field (save_documents): a host that echoes it back is
    // accepted, and the value is regenerated from the server's state rather than trusted.
    for (const auto& [name, value] : document.members)
        if (!keys.count(name) && name != "game_id" && name != "ctr_migration")
            throw Rejected("unsupported host document schema");
    const Json& update_time = document.at("update_time");
    if (!equals_u64(document.at("update_id"), expected) || !update_time.is_integer() ||
        update_time.kind == Json::Kind::Int || !document.at("lobby_open").is_bool() ||
        !document.at("lobby_open_for_pres_join").is_bool())
        throw Rejected("invalid host document version or flags");
    const Json& state = document.at("player_state");
    std::set<std::string> members;
    for (const std::uint64_t m : row->second.members) members.insert(std::to_string(m));
    std::set<std::string> state_keys;
    if (state.is_object())
        for (const auto& [name, value] : state.members) state_keys.insert(name);
    std::set<std::string> required_members;
    for (const auto member : row->second.members)
        if (!row->second.departed.count(member)) required_members.insert(std::to_string(member));
    bool state_ok =
        state.is_object() && std::includes(members.begin(), members.end(), state_keys.begin(), state_keys.end()) &&
        std::includes(state_keys.begin(), state_keys.end(), required_members.begin(), required_members.end());
    if (state_ok)
        for (const auto& [name, value] : state.members)
            if (!value.is_integer() || value.kind == Json::Kind::Int || value.as_u64() >= 4) state_ok = false;
    if (!state_ok) throw Rejected("invalid lobby player states");
    const Json& listen = document.at("listen_server");
    if (!keys_are(listen, {"host_address", "host_player_id"}) || !equals_u64(listen.at("host_player_id"), account) ||
        base64_decode(text_of(listen.at("host_address"))).size() != 67)
        throw Rejected("invalid lobby host advertisement");
    const Json& attachment = document.at("attachment");
    if (!attachment.is_null() && (!attachment.is_string() || attachment.text.size() >= 1024))
        throw Rejected("invalid lobby attachment");
    const Json& team = document.at("team_balance");
    if (!keys_are(team, {"can_change_teams"}) || !team.at("can_change_teams").is_bool())
        throw Rejected("invalid lobby team balance");
    const Json& rules = document.at("ruleset_payload");
    bool rules_ok = rules.is_object();
    if (rules_ok)
        for (const auto& [name, value] : rules.members)
            if ((name != "is_merge_src" && name != "is_merge_dst") || !value.is_bool()) rules_ok = false;
    if (!rules_ok) throw Rejected("unsupported lobby merge rules");
    Json backend = current->second.backend;
    auto remaining_members = row->second.members;
    remaining_members.erase(std::remove_if(remaining_members.begin(), remaining_members.end(),
                                           [&](auto member) { return !state_keys.count(std::to_string(member)); }),
                            remaining_members.end());
    auto& backend_players = backend.find("players")->members;
    backend_players.erase(std::remove_if(backend_players.begin(), backend_players.end(),
                                         [&](const auto& entry) { return !state_keys.count(entry.first); }),
                          backend_players.end());
    const std::uint64_t version = expected + 1;
    const auto now = static_cast<std::uint64_t>(std::time(nullptr));
    document.set("update_id", Json::of(version));
    document.set("update_time", Json::of(now));
    backend.set("update_id", Json::of(version));
    backend.set("update_time", Json::of(now));
    save_documents(lobby, document, backend, version);
    row->second.members = std::move(remaining_members);
    return true;
}
void MatchmakingStore::cancel_pending(std::uint64_t account, const std::string& selector) {
    for (auto& [id, lobby] : lobbies_)
        if (lobby.selector == selector) lobby.departed.insert(account);
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
std::optional<std::uint64_t> MatchmakingStore::promote(std::uint64_t id, Lobby& lobby, std::uint64_t lost,
                                                       const std::map<std::uint64_t, std::string>& connected) {
    const auto candidate = std::find_if(lobby.members.begin(), lobby.members.end(), [&](auto a) {
        return a != lost && !lobby.departed.count(a) && connected.count(a) && connected.at(a) == lobby.selector &&
               player_rooms_.count(a) && player_rooms_.at(a) == lobby.selector && player_info(a);
    });
    if (candidate == lobby.members.end() || !documents_.count(id)) return std::nullopt;
    const auto account = *candidate;
    auto& docs = documents_.at(id);
    const auto advertisement = parse_json(*player_info(account));
    const auto& listen = advertisement.at("listen_server");
    docs.host.find("listen_server")->set("host_player_id", Json::of(account));
    docs.host.find("listen_server")->set("host_address", listen.at("local_address"));
    ++docs.update_id;
    docs.host.set("update_id", Json::of(docs.update_id));
    docs.backend.set("update_id", Json::of(docs.update_id));
    lobby.host = account;
    lobby.departed.insert(lost);
    docs.host.set("ctr_migration", Json::of(migration_routes(id)));
    return account;
}
std::vector<LobbyPlan> MatchmakingStore::promote_disconnected(std::uint64_t lost,
                                                              const std::map<std::uint64_t, std::string>& connected) {
    std::vector<LobbyPlan> changed;
    for (auto& [id, lobby] : lobbies_) {
        if (lobby.host != lost) continue;
        if (const auto account = promote(id, lobby, lost, connected)) {
            LobbyPlan plan;
            plan.lobby_id = id;
            plan.host = *account;
            changed.push_back(plan);
        }
    }
    return changed;
}
std::optional<std::uint64_t> MatchmakingStore::leave_lobby(std::uint64_t account, std::uint64_t lobby,
                                                           const std::map<std::uint64_t, std::string>& connected) {
    const auto row = lobbies_.find(lobby);
    if (row == lobbies_.end() ||
        std::find(row->second.members.begin(), row->second.members.end(), account) == row->second.members.end())
        throw Rejected("not a member of this lobby");
    if (row->second.departed.count(account)) return std::nullopt;
    const auto promoted = row->second.host == account ? promote(lobby, row->second, account, connected) : std::nullopt;
    row->second.departed.insert(account);
    return promoted;
}
void MatchmakingStore::cancel_search(std::uint64_t account, std::uint64_t search_id) {
    for (Search& s : searches_)
        if (s.id == search_id && s.account == account && s.status == "pending") s.status = "cancelled";
}
bool MatchmakingStore::departed(std::uint64_t lobby, std::uint64_t account) const {
    const auto row = lobbies_.find(lobby);
    return row != lobbies_.end() && row->second.departed.count(account) != 0;
}
} // namespace ctr_online
