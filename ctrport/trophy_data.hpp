// SPDX-License-Identifier: GPL-2.0-only
// Trophies of the user's own dump: sce_sys/trophy/trophy00.trp and the NP communication ID in
// sce_sys/npbind.dat. Icons (TROP000.PNG...) are plain PNG entries; the texts (TROP*.ESFM)
// are encrypted and are read only with the user's own PS4 trophy key, as in shadPS4 v0.18.0
// src/core/file_format/trp.cpp (GPL-2.0-or-later): the NP communication ID, padded to 16
// bytes, is AES-128-CBC encrypted with the trophy key and a zero IV; that block is the key
// that decrypts the entry after its first 16 bytes (the IV). Without the key the trophies
// are listed by number, and trophy 0 is taken as the platinum, as in this game's set.
// Unlocks are kept in runtime/user/trophies/<NP communication ID>.txt, one "id unix_time"
// line per trophy. Shared by ctr_hle.dll and the launcher.
#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include "guest_patch_xml.hpp"
#pragma comment(lib, "bcrypt.lib")
namespace ctr_trophy {
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;

struct Trophy {
    int id = 0;
    std::string name, detail;  // empty without the trophy key
    char grade = 0;            // 'P', 'G', 'S', 'B'; 0 when unknown
    bool hidden = false;
    int group = -1;            // -1: base game
};
struct Entry { std::string name; std::uint64_t pos = 0, len = 0; std::uint32_t flag = 0; };
struct Set {
    std::string np_comm_id;
    fs::path trp;
    std::vector<Entry> entries;
    std::vector<Trophy> trophies;
    bool texts = false;        // names and grades decrypted
    std::string error;         // why the texts are missing, if the key was given
    int platinum() const {
        for (const auto& t : trophies) if (t.grade == 'P') return t.id;
        return texts || trophies.empty() ? -1 : 0;
    }
};

inline std::uint64_t be(const unsigned char* p, int n) {
    std::uint64_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 8) | p[i];
    return v;
}
inline Bytes read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + ctrport::mods::utf8(path.wstring()));
    return Bytes((std::istreambuf_iterator<char>(in)), {});
}
inline std::string np_comm_id(const fs::path& npbind) {
    const Bytes data = read_file(npbind);
    const std::string text(data.begin(), data.end());
    std::smatch match;
    if (!std::regex_search(text, match, std::regex("NPWR[0-9]{5}_[0-9]{2}"))) throw std::runtime_error("npbind.dat has no NP communication ID");
    return match.str();
}
inline std::vector<Entry> trp_entries(const fs::path& trp) {
    std::ifstream in(trp, std::ios::binary);
    unsigned char header[0x60]{};
    if (!in.read(reinterpret_cast<char*>(header), sizeof(header)) || be(header, 4) != 0xDCA24D00u)
        throw std::runtime_error("trophy00.trp: not a trophy file");
    const auto count = be(header + 16, 4), size = be(header + 20, 4);
    if (count > 1024 || size < 64) throw std::runtime_error("trophy00.trp: invalid entry table");
    std::vector<Entry> entries;
    for (std::uint64_t i = 0; i < count; ++i) {
        unsigned char raw[64]{};
        in.seekg(static_cast<std::streamoff>(0x60 + i * size));
        if (!in.read(reinterpret_cast<char*>(raw), sizeof(raw))) throw std::runtime_error("trophy00.trp: truncated entry table");
        Entry e;
        e.name.assign(reinterpret_cast<const char*>(raw), strnlen(reinterpret_cast<const char*>(raw), 32));
        e.pos = be(raw + 32, 8); e.len = be(raw + 40, 8); e.flag = static_cast<std::uint32_t>(be(raw + 48, 4));
        entries.push_back(e);
    }
    return entries;
}
inline Bytes read_entry(const fs::path& trp, const Entry& e) {
    if (e.len > 16u * 1024 * 1024) throw std::runtime_error("trophy entry too large");
    std::ifstream in(trp, std::ios::binary);
    in.seekg(static_cast<std::streamoff>(e.pos));
    Bytes data(static_cast<std::size_t>(e.len));
    if (!in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) throw std::runtime_error("trophy entry truncated");
    return data;
}
inline bool parse_key(const std::string& hex, unsigned char out[16]) {
    std::string digits;
    for (const char c : hex) if (!std::isspace(static_cast<unsigned char>(c))) digits += c;
    if (digits.size() != 32) return false;
    for (int i = 0; i < 16; ++i) {
        const auto pair = digits.substr(static_cast<std::size_t>(i) * 2, 2);
        if (!std::isxdigit(static_cast<unsigned char>(pair[0])) || !std::isxdigit(static_cast<unsigned char>(pair[1]))) return false;
        out[i] = static_cast<unsigned char>(std::stoul(pair, nullptr, 16));
    }
    return true;
}
// AES-128-CBC without padding, through Windows CNG.
inline Bytes aes_cbc(const unsigned char key[16], const unsigned char iv[16], const Bytes& data, bool encrypt) {
    if (data.empty() || data.size() % 16) throw std::runtime_error("AES input is not a whole number of blocks");
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_KEY_HANDLE handle = nullptr;
    Bytes out(data.size());
    unsigned char chain[16]; std::memcpy(chain, iv, 16);
    ULONG written = 0;
    bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
                          sizeof(BCRYPT_CHAIN_MODE_CBC), 0) >= 0 &&
        BCryptGenerateSymmetricKey(alg, &handle, nullptr, 0, const_cast<PUCHAR>(key), 16, 0) >= 0;
    if (ok) ok = (encrypt ? BCryptEncrypt(handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, chain, 16,
                                          out.data(), static_cast<ULONG>(out.size()), &written, 0)
                          : BCryptDecrypt(handle, const_cast<PUCHAR>(data.data()), static_cast<ULONG>(data.size()), nullptr, chain, 16,
                                          out.data(), static_cast<ULONG>(out.size()), &written, 0)) >= 0;
    if (handle) BCryptDestroyKey(handle);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    if (!ok || written != out.size()) throw std::runtime_error("AES failed");
    return out;
}
inline std::string decrypt_esfm(const Bytes& entry, const unsigned char key[16], const std::string& np_comm_id) {
    if (entry.size() <= 16) throw std::runtime_error("encrypted trophy entry too small");
    Bytes id(16, 0);
    std::memcpy(id.data(), np_comm_id.data(), std::min<std::size_t>(np_comm_id.size(), 16));
    const unsigned char zero[16]{};
    const Bytes entry_key = aes_cbc(key, zero, id, true);
    const Bytes cipher(entry.begin() + 16, entry.end());
    const Bytes plain = aes_cbc(entry_key.data(), entry.data(), Bytes(cipher.begin(), cipher.begin() + static_cast<long>(cipher.size() / 16 * 16)), false);
    std::string text(plain.begin(), plain.end());
    const auto end = text.rfind('>');
    if (end == std::string::npos || text.find("<trophyconf") == std::string::npos) throw std::runtime_error("wrong trophy key");
    text.resize(end + 1);
    return text;
}
// TROP_xx.ESFM per PS4 system language (1 English uses TROP.ESFM).
inline std::string language_entry(int system_language) {
    char name[32];
    std::snprintf(name, sizeof(name), "TROP_%02d.ESFM", system_language);
    return system_language == 1 ? std::string("TROP.ESFM") : std::string(name);
}
inline const Entry* find_entry(const std::vector<Entry>& entries, const std::string& name) {
    for (const auto& e : entries) if (e.name == name) return &e;
    return nullptr;
}
// `game_dirs`: the update folder first, then the base game.
inline Set load(const std::vector<fs::path>& game_dirs, const std::string& key_hex, int system_language) {
    Set set;
    for (const auto& dir : game_dirs) {
        std::error_code ec;
        if (dir.empty() || !fs::exists(dir / "sce_sys/trophy/trophy00.trp", ec)) continue;
        set.trp = dir / "sce_sys/trophy/trophy00.trp";
        set.np_comm_id = np_comm_id(dir / "sce_sys/npbind.dat");
        break;
    }
    if (set.trp.empty()) throw std::runtime_error("trophy00.trp not found");
    set.entries = trp_entries(set.trp);
    std::map<int, Trophy> trophies;
    for (const auto& e : set.entries) {
        int id = 0;
        if (e.flag == 0 && sscanf_s(e.name.c_str(), "TROP%3d.PNG", &id) == 1 && id >= 0 && id < 128) trophies[id].id = id;
    }
    unsigned char key[16];
    if (!key_hex.empty()) {
        if (!parse_key(key_hex, key)) set.error = "the trophy key must be 32 hexadecimal digits";
        else try {
            const auto read = [&](const std::string& name) -> std::string {
                const Entry* e = find_entry(set.entries, name);
                return e ? decrypt_esfm(read_entry(set.trp, *e), key, set.np_comm_id) : std::string();
            };
            std::string texts = read(language_entry(system_language));
            if (texts.empty()) texts = read("TROP.ESFM");
            for (const std::string& xml : {read("TROPCONF.ESFM"), texts}) {
                if (xml.empty()) continue;
                const auto root = ctrport::guest_patches::parse_xml(xml);
                for (const auto& node : root.children) {
                    if (node.name != "trophy") continue;
                    const int id = std::atoi(node.attribute("id").c_str());
                    if (id < 0 || id >= 128) continue;
                    Trophy& t = trophies[id];
                    t.id = id;
                    if (const auto type = node.attribute("ttype"); !type.empty()) t.grade = type[0];
                    if (node.has("hidden")) t.hidden = node.attribute("hidden") == "yes";
                    if (node.has("gid")) t.group = std::atoi(node.attribute("gid").c_str());
                    if (const auto* name = node.child("name")) t.name = ctrport::guest_patches::trim(name->text);
                    if (const auto* detail = node.child("detail")) t.detail = ctrport::guest_patches::trim(detail->text);
                }
            }
            set.texts = true;
        } catch (const std::exception& e) { set.error = e.what(); }
    }
    for (auto& [id, t] : trophies) set.trophies.push_back(t);
    return set;
}
inline Bytes icon(const Set& set, int id) {
    char name[32];
    std::snprintf(name, sizeof(name), "TROP%03d.PNG", id);
    const Entry* e = find_entry(set.entries, name);
    return e ? read_entry(set.trp, *e) : Bytes();
}

// --- unlocks --------------------------------------------------------------------------------
inline fs::path unlock_file(const fs::path& user_root, const std::string& np_comm_id) {
    return user_root / "trophies" / (np_comm_id + ".txt");
}
inline std::map<int, std::int64_t> read_unlocks(const fs::path& file) {
    std::map<int, std::int64_t> unlocked;
    std::ifstream in(file);
    int id = 0; long long when = 0;
    while (in >> id >> when) if (id >= 0 && id < 128 && !unlocked.count(id)) unlocked[id] = when;
    return unlocked;
}
inline void write_unlocks(const fs::path& file, const std::map<int, std::int64_t>& unlocked) {
    fs::create_directories(file.parent_path());
    const fs::path temporary = file.wstring() + L".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        for (const auto& [id, when] : unlocked) out << id << ' ' << when << '\n';
        if (!out) throw std::runtime_error("cannot write trophy unlocks");
    }
    if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("cannot replace trophy unlocks");
}
// Earlier runtimes recorded every unlock call in unlocks.jsonl; the first of each counts.
inline void import_jsonl(const fs::path& user_root, const fs::path& file) {
    std::error_code ec;
    if (fs::exists(file, ec)) return;
    std::ifstream in(user_root / "trophies/unlocks.jsonl");
    if (!in) return;
    std::map<int, std::int64_t> unlocked;
    for (std::string line; std::getline(in, line);) {
        int id = -1; long long when = 0;
        if (sscanf_s(line.c_str(), "{\"trophy\":%d,\"time\":%lld}", &id, &when) == 2 && id >= 0 && id < 128 && !unlocked.count(id))
            unlocked[id] = when;
    }
    if (!unlocked.empty()) write_unlocks(file, unlocked);
}
}
