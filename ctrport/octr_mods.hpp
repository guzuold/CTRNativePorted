// SPDX-License-Identifier: GPL-2.0-only
// octr-mod mods: a folder in runtime/mods with a mod.json manifest (schemaVersion 2), the
// format of the mods for the PC ports published on GameBanana (game 26138). Shared by
// ctrport.exe (C++17) and the launcher. Supported, from the manifests themselves:
// - "archive_asset" operations: entry `index` of archives/<archive> (checked against its
//   `asset` path) is replaced by `payload` (a file of the mod) or by the entry with `runs`
//   ([offset, original hex, patched hex]) applied. The entry the game would read must hash to
//   `original_sha256` and the result to `patched_sha256`. A mod is applied whole or not at
//   all: one that does not fit the game (another region or version, a feature this runtime
//   lacks) is skipped, the game starts without it, and the reason is logged and written to
//   runtime/user/logs/mods_skipped.json for the launcher. Mods later in the load order apply
//   on top of earlier ones. The archive is served
//   as a view (settings/menu_overlay.hpp): the original file, the replaced entries appended
//   uncompressed after it, their 16-byte descriptors redirected; no game file is copied.
// - "patches": the mod's XML patch files, applied while the mod is enabled (guest_patch_xml.hpp).
// - "files": the SHA256 of each file of the mod, checked at startup.
// Any other capability or operation kind is refused by name, so a mod is never half-applied.
#pragma once
#include "settings/menu_overlay.hpp"
#include "app0.hpp"
#include "guest_patch_xml.hpp"
#include "mod_overlay.hpp"
#include <functional>
#include <set>

namespace ctrport::octr {
namespace fs = std::filesystem;
using Bytes = std::vector<std::uint8_t>;
using View = settings::menu_overlay::View;

// --- JSON reader for the manifests ----------------------------------------------------------------
struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    bool boolean = false;
    double number = 0;
    std::string text;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> members;
    const Json* find(const std::string& key) const {
        for (const auto& member : members) if (member.first == key) return &member.second;
        return nullptr;
    }
};
class Reader {
public:
    explicit Reader(const std::string& text) : s_(text) {}
    Json document() {
        Json value = parse(0);
        space();
        if (at_ != s_.size()) fail("text after the end");
        return value;
    }
private:
    [[noreturn]] void fail(const std::string& what) const { throw std::runtime_error("JSON: " + what + " at byte " + std::to_string(at_)); }
    void space() { while (at_ < s_.size() && std::strchr(" \t\r\n", s_[at_])) ++at_; }
    bool take(char c) { space(); if (at_ < s_.size() && s_[at_] == c) { ++at_; return true; } return false; }
    void expect(char c) { if (!take(c)) fail(std::string("expected '") + c + "'"); }
    static void utf8(std::string& out, unsigned code) {
        if (code < 0x80) out += char(code);
        else if (code < 0x800) { out += char(0xC0 | (code >> 6)); out += char(0x80 | (code & 0x3F)); }
        else if (code < 0x10000) { out += char(0xE0 | (code >> 12)); out += char(0x80 | ((code >> 6) & 0x3F)); out += char(0x80 | (code & 0x3F)); }
        else { out += char(0xF0 | (code >> 18)); out += char(0x80 | ((code >> 12) & 0x3F)); out += char(0x80 | ((code >> 6) & 0x3F)); out += char(0x80 | (code & 0x3F)); }
    }
    unsigned hex4() {
        if (at_ + 4 > s_.size()) fail("short \\u escape");
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = s_[at_++];
            value = value * 16 + (c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : (fail("bad \\u escape"), 0));
        }
        return value;
    }
    std::string string() {
        expect('"');
        std::string out;
        while (true) {
            if (at_ >= s_.size()) fail("unterminated string");
            const char c = s_[at_++];
            if (c == '"') return out;
            if (c != '\\') { out += c; continue; }
            if (at_ >= s_.size()) fail("unterminated escape");
            const char e = s_[at_++];
            switch (e) {
            case '"': case '\\': case '/': out += e; break;
            case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break;
            case 'r': out += '\r'; break; case 't': out += '\t'; break;
            case 'u': {
                unsigned code = hex4();
                if (code >= 0xD800 && code < 0xDC00 && at_ + 6 <= s_.size() && s_[at_] == '\\' && s_[at_ + 1] == 'u') {
                    at_ += 2;
                    const unsigned low = hex4();
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                utf8(out, code);
                break;
            }
            default: fail("bad escape");
            }
        }
    }
    Json parse(int depth) {
        if (depth > 64) fail("nesting too deep");
        space();
        if (at_ >= s_.size()) fail("unexpected end");
        Json value;
        const char c = s_[at_];
        if (c == '{') {
            ++at_;
            value.kind = Json::Object;
            if (take('}')) return value;
            do {
                space();
                std::string key = string();
                expect(':');
                value.members.emplace_back(std::move(key), parse(depth + 1));
            } while (take(','));
            expect('}');
        } else if (c == '[') {
            ++at_;
            value.kind = Json::Array;
            if (take(']')) return value;
            do value.items.push_back(parse(depth + 1)); while (take(','));
            expect(']');
        } else if (c == '"') {
            value.kind = Json::String;
            value.text = string();
        } else if (s_.compare(at_, 4, "true") == 0) { at_ += 4; value.kind = Json::Bool; value.boolean = true; }
        else if (s_.compare(at_, 5, "false") == 0) { at_ += 5; value.kind = Json::Bool; }
        else if (s_.compare(at_, 4, "null") == 0) { at_ += 4; }
        else {
            const char* start = s_.c_str() + at_;
            char* end = nullptr;
            value.number = std::strtod(start, &end);
            if (end == start) fail("unexpected character");
            at_ += size_t(end - start);
            value.kind = Json::Number;
        }
        return value;
    }
    const std::string& s_;
    size_t at_ = 0;
};

// --- Manifest ----------------------------------------------------------------------------------------
struct Run { std::uint64_t offset = 0; Bytes from, to; };
struct Operation {
    std::string archive, asset, original_sha256, patched_sha256;
    std::uint32_t index = 0;
    fs::path payload;       // empty: `runs` instead
    std::vector<Run> runs;
};
struct Manifest {
    std::string id, name;
    std::vector<std::string> unsupported;  // capabilities and operation kinds this runtime lacks
    std::vector<Operation> operations;
    std::vector<fs::path> patches;
    std::vector<std::pair<fs::path, std::string>> files;  // file, SHA256
};
inline bool supported_capability(const std::string& name) {
    return name == "archive-assets" || name == "xml-patches" || name == "code-signatures";
}
inline std::string lower(std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c = char(c + 32); return s; }
inline Bytes hex_bytes(const std::string& hex) {
    if (hex.size() % 2) throw std::runtime_error("odd hex length");
    Bytes out;
    for (size_t i = 0; i < hex.size(); i += 2) {
        const auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            throw std::runtime_error("bad hex digit");
        };
        out.push_back(std::uint8_t(nibble(hex[i]) * 16 + nibble(hex[i + 1])));
    }
    return out;
}
// An absolute path in the \\?\ form, so payloads deep inside a mod work past MAX_PATH.
inline fs::path long_path(const fs::path& path) {
    std::wstring text = fs::absolute(path).lexically_normal().wstring();
    if (text.rfind(LR"(\\?\)", 0) == 0) return text;
    for (auto& c : text) if (c == L'/') c = L'\\';
    return text.rfind(LR"(\\)", 0) == 0 ? LR"(\\?\UNC\)" + text.substr(2) : LR"(\\?\)" + text;
}
// A path of the manifest, kept inside the mod folder.
inline fs::path inside(const fs::path& folder, const std::string& relative) {
    const fs::path path = fs::u8path(relative);
    if (relative.empty() || path.has_root_path() || relative.find(':') != std::string::npos) throw std::runtime_error("path outside the mod: " + relative);
    for (const auto& part : path) if (part == "..") throw std::runtime_error("path outside the mod: " + relative);
    return long_path(folder / path);
}
inline std::string text_of(const Json& object, const char* key) {
    const Json* value = object.find(key);
    return value && value->kind == Json::String ? value->text : std::string();
}

inline Manifest read_manifest(const fs::path& folder) {
    auto text = mods::read_text(long_path(folder / L"mod.json"));
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
    const Json root = Reader(text).document();
    if (root.kind != Json::Object) throw std::runtime_error("mod.json is not an object");
    Manifest manifest;
    manifest.id = text_of(root, "id");
    manifest.name = text_of(root, "name");
    if (const Json* capabilities = root.find("requiresCapabilities"))
        for (const Json& capability : capabilities->items)
            if (capability.kind == Json::String && !supported_capability(capability.text)) manifest.unsupported.push_back(capability.text);
    if (const Json* operations = root.find("operations"))
        for (const Json& item : operations->items) {
            const std::string kind = text_of(item, "kind");
            if (kind != "archive_asset") { manifest.unsupported.push_back("operation " + (kind.empty() ? std::string("?") : kind)); continue; }
            Operation op;
            op.archive = text_of(item, "archive");
            op.asset = text_of(item, "asset");
            op.original_sha256 = lower(text_of(item, "original_sha256"));
            op.patched_sha256 = lower(text_of(item, "patched_sha256"));
            const Json* index = item.find("index");
            if (op.archive.empty() || op.original_sha256.size() != 64 || op.patched_sha256.size() != 64 || !index || index->kind != Json::Number ||
                index->number < 0 || index->number > 4294967295.0)
                throw std::runtime_error("archive_asset operation without archive, index or hashes");
            op.index = std::uint32_t(index->number);
            if (const auto payload = text_of(item, "payload"); !payload.empty()) {
                op.payload = inside(folder, payload);
                std::error_code ec;
                if (!fs::is_regular_file(op.payload, ec)) throw std::runtime_error("payload missing: " + payload);
            } else if (const Json* runs = item.find("runs"); runs && runs->kind == Json::Array && !runs->items.empty()) {
                for (const Json& r : runs->items) {
                    if (r.items.size() != 3 || r.items[0].kind != Json::Number || r.items[0].number < 0 || r.items[1].kind != Json::String ||
                        r.items[2].kind != Json::String)
                        throw std::runtime_error("bad run in " + op.asset);
                    Run run{std::uint64_t(r.items[0].number), hex_bytes(r.items[1].text), hex_bytes(r.items[2].text)};
                    if (run.from.size() != run.to.size()) throw std::runtime_error("run lengths differ in " + op.asset);
                    op.runs.push_back(std::move(run));
                }
            } else {
                throw std::runtime_error("archive_asset operation without payload or runs: " + op.asset);
            }
            manifest.operations.push_back(std::move(op));
        }
    if (const Json* patches = root.find("patches"))
        for (const Json& patch : patches->items)
            if (patch.kind == Json::String) {
                manifest.patches.push_back(inside(folder, patch.text));
                std::error_code ec;
                if (!fs::is_regular_file(manifest.patches.back(), ec)) throw std::runtime_error("patch file missing: " + patch.text);
            }
    if (const Json* files = root.find("files"))
        for (const Json& file : files->items) {
            const auto name = text_of(file, "file"), hash = lower(text_of(file, "sha256"));
            if (!name.empty() && hash.size() == 64) manifest.files.push_back({inside(folder, name), hash});
        }
    return manifest;
}

// --- Building the archive views ----------------------------------------------------------------------
inline std::string file_sha256(const fs::path& path) {
    BCRYPT_ALG_HANDLE algorithm{}; BCRYPT_HASH_HANDLE hash{};
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) throw std::runtime_error("SHA256 provider");
    BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.u8string());
    std::vector<char> buffer(1 << 20);
    while (in.read(buffer.data(), std::streamsize(buffer.size())) || in.gcount())
        BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(in.gcount()), 0);
    unsigned char digest[32];
    BCryptFinishHash(hash, digest, 32, 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    std::string out;
    for (unsigned char b : digest) { out += "0123456789abcdef"[b >> 4]; out += "0123456789abcdef"[b & 15]; }
    return out;
}
inline Bytes file_bytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    Bytes out((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (!in.eof() && in.fail()) throw std::runtime_error("cannot read " + path.u8string());
    return out;
}
inline std::string folded_asset(std::string path) {
    for (auto& c : path) if (c == '\\') c = '/';
    return lower(path);
}
inline bool names_asset(const settings::menu_assets::IgaEntry& entry, const std::string& asset) {
    if (asset.empty()) return true;
    const auto want = folded_asset(asset), path = folded_asset(entry.path), full = folded_asset(entry.full_path);
    return path == want || full == want || (full.size() > want.size() && full.compare(full.size() - want.size(), want.size(), want) == 0 &&
                                            full[full.size() - want.size() - 1] == '/');
}

// --- IGZ assets that differ between regions only in their language tables ------------------
// An IGZ10 file lists the files it loads in its TDEP fixup section as (name, path) pairs, sorted
// by name without case; the .lng language tables of a UI file are there and nowhere else. The EU
// and US games ship different sets (ar/de/es/it against mx/pt), so a US mod's UI file can be
// moved onto the EU game by giving it the EU set, but only when this is proven:
//  1. this game's entry with the mod's language set must be byte for byte the asset the mod was
//     made for (its original_sha256): the regions differ in nothing else;
//  2. the port with the mod's language set given back must be byte for byte the mod's payload
//     (its patched_sha256): the port changed nothing else.
namespace igz {
using Deps = std::vector<std::pair<std::string, std::string>>;
struct Tdep { Deps deps; std::size_t at = 0, size = 0, header = 0; };
inline std::uint32_t u32(const Bytes& d, std::size_t at) { std::uint32_t v; std::memcpy(&v, d.data() + at, 4); return v; }
inline bool tdep(const Bytes& d, Tdep& out) {
    if (d.size() < 0x224 || u32(d, 0) != 0x49475A01u || u32(d, 4) != 10) return false;
    std::size_t at = u32(d, 24);  // first chunk: the fixup sections
    const std::uint32_t sections = u32(d, 16);
    for (std::uint32_t i = 0; i < sections; ++i) {
        if (at + 16 > d.size()) return false;
        const std::uint32_t n = u32(d, at + 4), size = u32(d, at + 8), header = u32(d, at + 12);
        if (size < header || at + size > d.size()) return false;
        if (std::memcmp(d.data() + at, "TDEP", 4) == 0) {
            std::vector<std::string> strings;
            std::size_t p = at + header;
            for (std::uint32_t k = 0; k < n * 2; ++k) {
                const auto end = std::find(d.begin() + static_cast<std::ptrdiff_t>(p), d.begin() + static_cast<std::ptrdiff_t>(at + size), 0);
                if (end == d.begin() + static_cast<std::ptrdiff_t>(at + size)) return false;
                strings.emplace_back(d.begin() + static_cast<std::ptrdiff_t>(p), end);
                p = static_cast<std::size_t>(end - d.begin()) + 1;
            }
            for (std::uint32_t k = 0; k < n; ++k) out.deps.push_back({strings[k * 2], strings[k * 2 + 1]});
            out.at = at; out.size = size; out.header = header;
            return true;
        }
        at += size;
    }
    return false;
}
// The file with its TDEP section replaced; chunk sizes and the following chunks' offsets updated.
inline Bytes with_deps(const Bytes& d, const Tdep& own, const Deps& deps) {
    Bytes payload;
    for (const auto& [name, path] : deps) {
        payload.insert(payload.end(), name.begin(), name.end()); payload.push_back(0);
        payload.insert(payload.end(), path.begin(), path.end()); payload.push_back(0);
    }
    payload.insert(payload.end(), 4 - payload.size() % 4, 0);  // 1-4 zero bytes, as the game's files
    Bytes section(d.begin() + static_cast<std::ptrdiff_t>(own.at), d.begin() + static_cast<std::ptrdiff_t>(own.at + own.header));
    const std::uint32_t count = static_cast<std::uint32_t>(deps.size()), size = static_cast<std::uint32_t>(own.header + payload.size());
    std::memcpy(section.data() + 4, &count, 4);
    std::memcpy(section.data() + 8, &size, 4);
    section.insert(section.end(), payload.begin(), payload.end());
    const std::int64_t delta = static_cast<std::int64_t>(section.size()) - static_cast<std::int64_t>(own.size);
    Bytes out(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(own.at));
    out.insert(out.end(), section.begin(), section.end());
    out.insert(out.end(), d.begin() + static_cast<std::ptrdiff_t>(own.at + own.size), d.end());
    const auto add = [&](std::size_t at) { const std::uint32_t v = static_cast<std::uint32_t>(u32(out, at) + delta); std::memcpy(out.data() + at, &v, 4); };
    add(28);  // the fixup chunk's size
    for (std::size_t entry = 36; entry + 16 <= 0x224 && u32(out, entry + 4); entry += 16) add(entry + 4);  // later chunks' offsets
    return out;
}
inline bool language(const std::pair<std::string, std::string>& dep) {
    const auto& p = dep.second;
    return p.size() > 4 && lower(p.substr(p.size() - 4)) == ".lng";
}
// `d` with its language tables replaced by those of `source`, in the game's order.
inline Bytes swap_languages(const Bytes& d, const Tdep& own, const Deps& source) {
    Deps merged;
    for (const auto& dep : own.deps) if (!language(dep)) merged.push_back(dep);
    for (const auto& dep : source) if (language(dep)) merged.push_back(dep);
    std::stable_sort(merged.begin(), merged.end(), [](const auto& a, const auto& b) { return lower(a.first) < lower(b.first); });
    return with_deps(d, own, merged);
}
// The mod's payload for this game, or empty when the two proofs above do not hold.
inline Bytes language_port(const Bytes& entry, const Bytes& payload, const std::string& original_sha256, const std::string& patched_sha256) {
    Tdep here, mod;
    if (!tdep(entry, here) || !tdep(payload, mod)) return {};
    if (settings::menu_assets::sha256(swap_languages(entry, here, mod.deps)) != original_sha256) return {};
    Bytes port = swap_languages(payload, mod, here.deps);
    Tdep ported;
    if (!tdep(port, ported) || settings::menu_assets::sha256(swap_languages(port, ported, mod.deps)) != patched_sha256) return {};
    return port;
}
}  // namespace igz

// What one archive gets: its replaced entries, by index, after every mod.
struct Replacement {
    Bytes bytes;        // the entry, or empty when served from `file`
    fs::path file;
    std::uint64_t size = 0;
    std::string sha256;
    std::wstring mod;
};
using Resolve = std::function<fs::path(const std::string&)>;  // /app0 relative path -> file, empty when missing

struct Result {
    std::map<std::string, std::shared_ptr<const View>> views;  // lower-case /app0 path
    std::vector<std::pair<std::wstring, fs::path>> patches;     // mod, XML file
    std::vector<std::pair<std::wstring, std::string>> skipped;  // mod, why it was not applied
    std::vector<std::pair<std::wstring, std::string>> adapted;  // mod, what was ported from the other region
    std::vector<std::string> log;
};

// title_id and app_ver: the running game (param.sfo); when given, a mod whose patch is for
// another game and cannot be ported (guest_patch_xml.hpp, other_region) is skipped whole.
inline Result build(const std::vector<mods::Mod>& mods, const Resolve& resolve, const std::string& title_id = {}, const std::string& app_ver = {}) {
    Result result;
    struct Target {
        fs::path host;
        std::unique_ptr<settings::menu_assets::Iga> archive;
        std::map<std::uint32_t, Replacement> replaced;
    };
    std::map<std::string, Target> targets;
    for (const auto& mod : mods) {
        // A mod is applied whole or not at all: its changes are staged, and kept only when
        // every operation checks out. A mod that does not fit this game is skipped and
        // reported, and the game starts without it.
        std::map<std::pair<std::string, std::uint32_t>, Replacement> staged;
        Manifest manifest;
        int ported = 0;
        try {
            manifest = read_manifest(mod.folder);
            if (!manifest.unsupported.empty()) {
                std::string list;
                for (const auto& name : manifest.unsupported) list += (list.empty() ? "" : ", ") + name;
                throw std::runtime_error("needs features this runtime does not have yet (" + list + ")");
            }
            if (!title_id.empty())
                for (const auto& file : manifest.patches)
                    for (const auto& patch : guest_patches::parse_patches(mods::read_text(file), file.filename().wstring()))
                        if (const auto why = guest_patches::mismatch(patch, title_id, app_ver); !why.empty() && !guest_patches::other_region(patch, title_id, app_ver))
                            throw std::runtime_error("its code patch " + patch.name + " is for another game (" + why + ") and cannot be ported");
            for (const auto& [file, hash] : manifest.files) {
                std::error_code ec;
                if (!fs::is_regular_file(file, ec) || file_sha256(file) != hash)
                    throw std::runtime_error("file " + mods::utf8(file.lexically_relative(mod.folder).generic_wstring()) + " is missing or damaged");
            }
            for (const auto& op : manifest.operations) {
                const std::string key = lower(op.archive.find('/') == std::string::npos ? "archives/" + op.archive : op.archive);
                auto& target = targets[key];
                if (!target.archive) {
                    target.host = resolve(key);
                    if (target.host.empty()) throw std::runtime_error(op.archive + " is not in the game folders");
                    target.archive = std::make_unique<settings::menu_assets::Iga>(target.host, false);
                }
                auto& archive = *target.archive;
                const settings::menu_assets::IgaEntry* entry = nullptr;
                if (op.index < archive.files.size() && names_asset(archive.files[op.index], op.asset)) entry = &archive.files[op.index];
                for (size_t i = 0; !entry && i < archive.files.size(); ++i)
                    if (!op.asset.empty() && names_asset(archive.files[i], op.asset)) entry = &archive.files[i];
                if (!entry) throw std::runtime_error(op.asset + " is not in " + op.archive);
                // The entry as the game would read it: this mod's own change, an earlier mod's, or the archive's.
                const Replacement* previous = nullptr;
                if (const auto s = staged.find({key, entry->index}); s != staged.end()) previous = &s->second;
                else if (const auto r = target.replaced.find(entry->index); r != target.replaced.end()) previous = &r->second;
                Bytes current;
                std::string current_sha;
                if (!previous) {
                    current = archive.extract(*entry);
                    current_sha = settings::menu_assets::sha256(current);
                } else {
                    current_sha = previous->sha256;
                }
                Replacement next;
                next.mod = mod.name;
                if (current_sha != op.original_sha256) {
                    // The other region's asset: ported when it differs only in its language tables.
                    Bytes port;
                    if (!previous && !op.payload.empty()) port = igz::language_port(current, file_bytes(op.payload), op.original_sha256, op.patched_sha256);
                    if (port.empty())
                        throw std::runtime_error(op.asset + " in " + op.archive + " differs from the one the mod was made for (found " + current_sha.substr(0, 12) + ")" +
                                                 (!previous ? " (another game version?)" : " (changed by " + mods::utf8(previous->mod) + ")"));
                    next.sha256 = settings::menu_assets::sha256(port);
                    next.size = port.size();
                    next.bytes = std::move(port);
                    ++ported;
                } else if (!op.payload.empty()) {
                    next.file = op.payload;
                    next.size = fs::file_size(op.payload);
                    next.sha256 = file_sha256(op.payload);
                } else {
                    if (previous) current = previous->file.empty() ? previous->bytes : file_bytes(previous->file);
                    for (const auto& run : op.runs) {
                        if (run.offset + run.from.size() > current.size() || !std::equal(run.from.begin(), run.from.end(), current.begin() + std::ptrdiff_t(run.offset)))
                            throw std::runtime_error(op.asset + ": bytes at " + std::to_string(run.offset) + " differ from the mod's");
                        std::copy(run.to.begin(), run.to.end(), current.begin() + std::ptrdiff_t(run.offset));
                    }
                    next.sha256 = settings::menu_assets::sha256(current);
                    next.size = current.size();
                    next.bytes = std::move(current);
                }
                if (next.sha256 != op.patched_sha256 && current_sha == op.original_sha256)
                    throw std::runtime_error(op.asset + ": the result does not match the mod's patched_sha256");
                staged[{key, entry->index}] = std::move(next);
            }
        } catch (const std::exception& e) {
            result.skipped.push_back({mod.name, e.what()});
            result.log.push_back("Mods: " + mods::utf8(mod.name) + ": not applied: " + e.what());
            continue;
        }
        for (auto& [where, replacement] : staged) targets[where.first].replaced[where.second] = std::move(replacement);
        if (ported) {
            result.adapted.push_back({mod.name, "language tables of " + std::to_string(ported) + " assets ported"});
            result.log.push_back("Mods: " + mods::utf8(mod.name) + ": " + std::to_string(ported) + " assets ported from the other region (language tables)");
        }
        for (const auto& patch : manifest.patches) result.patches.push_back({mod.name, patch});
        result.log.push_back("Mods: " + mods::utf8(mod.name) + ": " + std::to_string(manifest.operations.size()) + " asset changes, " +
                             std::to_string(manifest.patches.size()) + " patch files");
    }
    for (auto& [key, target] : targets) {
        if (target.replaced.empty()) continue;
        auto view = std::make_shared<View>();
        view->host = target.host;
        view->source_size = target.archive->size();
        for (auto& [index, replacement] : target.replaced)
            settings::menu_overlay::append_entry(*view, *target.archive, target.archive->files[index], std::move(replacement.bytes),
                                                 replacement.file, replacement.size);
        result.log.push_back("Mods: " + key + ": " + std::to_string(target.replaced.size()) + " entries replaced");
        result.views[key] = view;
    }
    return result;
}

// Runtime startup, after mods::overlay.configure_from_environment: the views of the enabled
// octr-mod mods replace the archives they change, and their patch files are registered. What
// was skipped or adapted goes to runtime/user/logs/mods_status.json (mod_overlay.hpp).
inline void configure(const fs::path& base, const fs::path& patch) {
    mods::statuses().clear();
    const auto mods = mods::overlay.octr_mods();
    if (mods.empty()) { mods::write_statuses(); return; }
    std::string title_id, app_ver;
    try {
        const auto sfo = guest_patches::read_sfo((patch.empty() ? base : patch) / "sce_sys/param.sfo");
        if (sfo.count("TITLE_ID") && sfo.count("APP_VER")) { title_id = sfo.at("TITLE_ID"); app_ver = sfo.at("APP_VER"); }
    } catch (const std::exception&) {}
    auto result = build(mods, [&](const std::string& relative) -> fs::path {
        try { return app0_read_path(base, patch, relative); } catch (const std::exception&) { return {}; }
    }, title_id, app_ver);
    for (const auto& line : result.log) std::fprintf(stderr, "%s\n", line.c_str());
    std::fflush(stderr);
    for (const auto& [mod, why] : result.skipped) mods::statuses().push_back({mod, "skipped", why});
    for (const auto& [mod, what] : result.adapted) mods::statuses().push_back({mod, "adapted", what});
    mods::write_statuses();
    settings::menu_overlay::overlay.set_mod_views(std::move(result.views));
    guest_patches::mod_patch_files() = std::move(result.patches);
}
}  // namespace ctrport::octr
