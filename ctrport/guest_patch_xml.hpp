// SPDX-License-Identifier: GPL-2.0-only
// Patch XML files in the shadPS4 / GoldHEN format, as SacredFire and GetLostIn
// CTR-NitroFueled-PC read them from patches/ with the choices in patches.json
// ({"enabled": ["file.xml/Name"], "disabled": ["file.xml/Name"]}; otherwise the Metadata
// isEnabled attribute decides). Value encodings and address convention follow the public
// shadPS4 v0.18.0 src/common/memory_patcher.cpp (GPL-2.0-or-later): numeric types are
// written little-endian, utf8/utf16 get a terminator, an eboot Address includes the 0x400000
// base, and "mask" lines search a byte pattern (?? = any byte) and write at match + Offset.
// Additional checks of this runtime: a patch must name the running title in TitleID and target
// eboot.bin, and every write must stay inside one mapped segment of the eboot image.
// mask_jump32 (code caves) is not supported and is reported. Shared by the launcher (C++20)
// and ctrport.exe (C++17).
#pragma once
#include "mod_overlay.hpp"
#include <cstdint>
#include <cstring>
#include <functional>
namespace ctrport::guest_patches {
namespace fs = std::filesystem;
using mods::utf8;
using mods::wide;

// --- Minimal XML reader: elements, attributes, text, comments, declarations ---------------
struct Node {
    std::string name, text;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<Node> children;
    std::string attribute(const char* key) const {
        for (const auto& a : attributes) if (a.first == key) return a.second;
        return {};
    }
    bool has(const char* key) const {
        for (const auto& a : attributes) if (a.first == key) return true;
        return false;
    }
    const Node* child(const char* key) const {
        for (const auto& c : children) if (c.name == key) return &c;
        return nullptr;
    }
};
inline std::string decode_entities(const std::string& raw) {
    std::string out;
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] != '&') { out += raw[i]; continue; }
        const auto end = raw.find(';', i);
        if (end == std::string::npos) throw std::runtime_error("XML: unterminated entity");
        const auto name = raw.substr(i + 1, end - i - 1);
        unsigned long code = 0;
        if (name == "amp") code = '&'; else if (name == "lt") code = '<'; else if (name == "gt") code = '>';
        else if (name == "quot") code = '"'; else if (name == "apos") code = '\'';
        else if (name.size() > 2 && name[0] == '#' && (name[1] == 'x' || name[1] == 'X')) code = std::stoul(name.substr(2), nullptr, 16);
        else if (name.size() > 1 && name[0] == '#') code = std::stoul(name.substr(1), nullptr, 10);
        else throw std::runtime_error("XML: unknown entity &" + name + ";");
        if (code < 0x80) out += static_cast<char>(code);
        else out += utf8(std::wstring(1, static_cast<wchar_t>(code)));
        i = end;
    }
    return out;
}
inline Node parse_xml(const std::string& text) {
    std::size_t at = 0;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xef) at = 3;
    const auto fail = [&](const char* why) { throw std::runtime_error(std::string("XML: ") + why + " at byte " + std::to_string(at)); };
    const auto skip_space = [&] { while (at < text.size() && std::isspace(static_cast<unsigned char>(text[at]))) ++at; };
    const auto skip_misc = [&] {
        while (true) {
            skip_space();
            if (text.compare(at, 4, "<!--") == 0) {
                const auto end = text.find("-->", at + 4);
                if (end == std::string::npos) fail("unterminated comment");
                at = end + 3;
            } else if (text.compare(at, 2, "<?") == 0) {
                const auto end = text.find("?>", at + 2);
                if (end == std::string::npos) fail("unterminated declaration");
                at = end + 2;
            } else if (text.compare(at, 9, "<!DOCTYPE") == 0) {
                const auto end = text.find('>', at);
                if (end == std::string::npos) fail("unterminated DOCTYPE");
                at = end + 1;
            } else return;
        }
    };
    const auto name = [&]() -> std::string {
        const auto start = at;
        while (at < text.size() && (std::isalnum(static_cast<unsigned char>(text[at])) || text[at] == '_' || text[at] == '-' || text[at] == ':' || text[at] == '.')) ++at;
        if (start == at) fail("name expected");
        return text.substr(start, at - start);
    };
    std::function<Node(int)> element = [&](int depth) -> Node {
        if (depth > 64) fail("nesting too deep");
        if (at >= text.size() || text[at] != '<') fail("element expected");
        ++at;
        Node node;
        node.name = name();
        while (true) {
            skip_space();
            if (at >= text.size()) fail("unterminated tag");
            if (text[at] == '/') {
                if (text.compare(at, 2, "/>") != 0) fail("invalid tag end");
                at += 2;
                return node;
            }
            if (text[at] == '>') { ++at; break; }
            auto key = name();
            skip_space();
            if (at >= text.size() || text[at] != '=') fail("attribute value expected");
            ++at; skip_space();
            if (at >= text.size() || (text[at] != '"' && text[at] != '\'')) fail("quoted attribute value expected");
            const char quote = text[at++];
            const auto end = text.find(quote, at);
            if (end == std::string::npos) fail("unterminated attribute value");
            node.attributes.push_back({key, decode_entities(text.substr(at, end - at))});
            at = end + 1;
        }
        while (true) {
            const auto next = text.find('<', at);
            if (next == std::string::npos) fail("unterminated element");
            node.text += decode_entities(text.substr(at, next - at));
            at = next;
            if (text.compare(at, 4, "<!--") == 0) { skip_misc(); continue; }
            if (text.compare(at, 9, "<![CDATA[") == 0) {
                const auto end = text.find("]]>", at + 9);
                if (end == std::string::npos) fail("unterminated CDATA");
                node.text += text.substr(at + 9, end - at - 9);
                at = end + 3;
                continue;
            }
            if (text.compare(at, 2, "</") == 0) {
                at += 2;
                if (name() != node.name) fail("mismatched closing tag");
                skip_space();
                if (at >= text.size() || text[at] != '>') fail("invalid closing tag");
                ++at;
                return node;
            }
            node.children.push_back(element(depth + 1));
        }
    };
    skip_misc();
    Node root = element(0);
    skip_misc();
    if (at != text.size()) fail("data after the root element");
    return root;
}

// --- Patch files ------------------------------------------------------------------------------
struct Line { std::string type, address, value, offset; };
struct Patch {
    std::wstring file;  // file name inside the patches folder
    std::string name, author, note, app_ver, app_elf, patch_ver;
    std::vector<std::string> title_ids;
    bool default_enabled = false;
    std::vector<Line> lines;
    std::wstring key() const { return file + L"/" + wide(name); }
};
inline std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}
inline std::vector<Patch> parse_patches(const std::string& text, const std::wstring& file) {
    const Node root = parse_xml(text);
    if (root.name != "Patch") throw std::runtime_error("XML: root element is not <Patch>");
    std::vector<std::string> titles;
    if (const auto* ids = root.child("TitleID"))
        for (const auto& id : ids->children) if (id.name == "ID") titles.push_back(trim(id.text));
    std::vector<Patch> patches;
    for (const auto& meta : root.children) {
        if (meta.name != "Metadata") continue;
        Patch patch;
        patch.file = file;
        patch.name = meta.attribute("Name");
        if (patch.name.empty()) throw std::runtime_error("XML: <Metadata> without Name");
        patch.author = meta.attribute("Author");
        patch.note = meta.attribute("Note");
        patch.app_ver = meta.attribute("AppVer");
        patch.app_elf = meta.attribute("AppElf");
        patch.patch_ver = meta.attribute("PatchVer");
        patch.title_ids = titles;
        patch.default_enabled = meta.attribute("isEnabled") == "true";
        for (const auto& list : meta.children) {
            if (list.name != "PatchList") continue;
            for (const auto& line : list.children) {
                if (line.name != "Line") continue;
                patch.lines.push_back({line.attribute("Type"), trim(line.attribute("Address")), line.attribute("Value"), trim(line.attribute("Offset"))});
            }
        }
        patches.push_back(std::move(patch));
    }
    return patches;
}
struct PatchFile { std::wstring file; std::vector<Patch> patches; std::string error; };
inline std::vector<PatchFile> load_folder(const fs::path& dir) {
    std::vector<PatchFile> files;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file() || mods::fold(entry.path().extension().wstring()) != L".xml") continue;
        PatchFile file{entry.path().filename().wstring(), {}, {}};
        try { file.patches = parse_patches(mods::read_text(entry.path()), file.file); }
        catch (const std::exception& e) { file.error = e.what(); }
        files.push_back(std::move(file));
    }
    std::sort(files.begin(), files.end(), [](const PatchFile& a, const PatchFile& b) { return mods::fold(a.file) < mods::fold(b.file); });
    return files;
}

struct Selection { std::vector<std::wstring> enabled, disabled; };
inline Selection read_selection(const fs::path& path) {
    Selection selection;
    std::error_code ec;
    if (!fs::exists(path, ec)) return selection;
    const auto lists = mods::parse_string_lists(mods::read_text(path), "patches.json");
    if (const auto it = lists.find("enabled"); it != lists.end()) selection.enabled = it->second;
    if (const auto it = lists.find("disabled"); it != lists.end()) selection.disabled = it->second;
    return selection;
}
inline void write_selection(const fs::path& path, const Selection& selection) {
    mods::write_text(path, mods::format_string_lists({{"enabled", selection.enabled}, {"disabled", selection.disabled}}));
}
inline bool selected(const Patch& patch, const Selection& selection) {
    const auto key = patch.key();
    if (std::find(selection.enabled.begin(), selection.enabled.end(), key) != selection.enabled.end()) return true;
    if (std::find(selection.disabled.begin(), selection.disabled.end(), key) != selection.disabled.end()) return false;
    return patch.default_enabled;
}
inline void choose(Selection& selection, const Patch& patch, bool on) {
    const auto key = patch.key();
    for (auto* list : {&selection.enabled, &selection.disabled})
        list->erase(std::remove(list->begin(), list->end(), key), list->end());
    (on ? selection.enabled : selection.disabled).push_back(key);
}

// Why a patch does not apply to this title and version; empty when it does.
inline std::string mismatch(const Patch& patch, const std::string& title_id, const std::string& app_ver) {
    if (std::find(patch.title_ids.begin(), patch.title_ids.end(), title_id) == patch.title_ids.end())
        return "TitleID does not list " + title_id;
    if (!patch.app_elf.empty() && mods::fold(wide(patch.app_elf)) != L"eboot.bin") return "AppElf " + patch.app_elf + " is not eboot.bin";
    if (patch.app_ver != app_ver) {
        const bool masks_only = !patch.lines.empty() && std::all_of(patch.lines.begin(), patch.lines.end(), [](const Line& l) { return l.type == "mask"; });
        if (!masks_only) return "AppVer " + patch.app_ver + " differs from " + app_ver;
    }
    return {};
}

inline unsigned long long parse_number(const std::string& raw, bool force_hex = false) {
    const auto value = trim(raw);
    if (value.empty()) throw std::runtime_error("empty number");
    std::size_t start = 0;
    int base = force_hex ? 16 : 10;
    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) { start = 2; base = 16; }
    else if (value[0] == '$' || value[0] == '#') { start = 1; base = 16; }
    std::size_t used = 0;
    const auto digits = value.substr(start);
    if (digits.empty() || digits[0] == '-' || digits[0] == '+') throw std::runtime_error("invalid number " + value);
    const auto number = std::stoull(digits, &used, base);
    if (used != digits.size()) throw std::runtime_error("invalid number " + value);
    return number;
}
inline std::vector<unsigned char> little(unsigned long long value, std::size_t size, const std::string& raw) {
    if (size < 8 && value >> (size * 8)) throw std::runtime_error("value " + raw + " does not fit in " + std::to_string(size) + " bytes");
    std::vector<unsigned char> out(size);
    for (std::size_t i = 0; i < size; ++i) out[i] = static_cast<unsigned char>(value >> (i * 8));
    return out;
}
inline std::vector<unsigned char> hex_bytes(const std::string& raw) {
    std::string digits;
    for (const char c : raw) if (!std::isspace(static_cast<unsigned char>(c))) digits += c;
    if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits.erase(0, 2);
    if (digits.empty() || digits.size() % 2) throw std::runtime_error("bytes value needs an even number of hex digits");
    std::vector<unsigned char> out;
    for (std::size_t i = 0; i < digits.size(); i += 2) {
        if (!std::isxdigit(static_cast<unsigned char>(digits[i])) || !std::isxdigit(static_cast<unsigned char>(digits[i + 1])))
            throw std::runtime_error("bytes value is not hexadecimal");
        out.push_back(static_cast<unsigned char>(std::stoul(digits.substr(i, 2), nullptr, 16)));
    }
    return out;
}
inline std::vector<unsigned char> encode(const std::string& type, const std::string& value) {
    if (type == "bytes" || type == "mask") return hex_bytes(value);
    if (type == "byte") return little(parse_number(value), 1, value);
    if (type == "bytes16") return little(parse_number(value), 2, value);
    if (type == "bytes32") return little(parse_number(value), 4, value);
    if (type == "bytes64") return little(parse_number(value), 8, value);
    if (type == "float32" || type == "float64") {
        std::size_t used = 0;
        const auto text = trim(value);
        const double number = std::stod(text, &used);
        if (used != text.size()) throw std::runtime_error("invalid float " + value);
        std::vector<unsigned char> out(type == "float32" ? 4 : 8);
        if (type == "float32") { const float f = static_cast<float>(number); std::memcpy(out.data(), &f, 4); }
        else std::memcpy(out.data(), &number, 8);
        return out;
    }
    if (type == "utf8") {
        std::vector<unsigned char> out(value.begin(), value.end());
        out.push_back(0);
        return out;
    }
    if (type == "utf16") {
        std::vector<unsigned char> out;
        for (const wchar_t c : wide(value)) { out.push_back(static_cast<unsigned char>(c & 0xff)); out.push_back(static_cast<unsigned char>(c >> 8)); }
        out.push_back(0); out.push_back(0);
        return out;
    }
    throw std::runtime_error("unsupported line type \"" + type + "\"");
}
// "48 8B ?? 05" -> bytes, -1 for wildcards (shadPS4 PatternToByte).
inline std::vector<int> pattern(const std::string& raw) {
    std::vector<int> out;
    std::istringstream in(raw);
    for (std::string token; in >> token;) {
        if (token == "?" || token == "??") { out.push_back(-1); continue; }
        if (token.size() != 2 || !std::isxdigit(static_cast<unsigned char>(token[0])) || !std::isxdigit(static_cast<unsigned char>(token[1])))
            throw std::runtime_error("invalid mask pattern token " + token);
        out.push_back(static_cast<int>(std::stoul(token, nullptr, 16)));
    }
    if (out.empty()) throw std::runtime_error("empty mask pattern");
    return out;
}

using Spans = std::vector<std::pair<std::uint64_t, std::uint64_t>>;
struct Write { std::uint64_t offset; std::vector<unsigned char> bytes; };
// Image offsets and bytes of every line of a patch, validated against the mapped segments.
inline std::vector<Write> plan(const Patch& patch, const unsigned char* image, const Spans& spans, bool version_matches) {
    std::vector<Write> writes;
    const auto inside = [&](std::uint64_t offset, std::size_t size) {
        for (const auto& span : spans) if (offset >= span.first && size <= span.second - span.first && offset - span.first <= span.second - span.first - size) return true;
        return false;
    };
    for (const auto& line : patch.lines) {
        if (line.type == "mask_jump32") throw std::runtime_error("mask_jump32 lines are not supported");
        if (line.type != "mask" && !version_matches) continue;
        auto bytes = encode(line.type, line.value);
        std::uint64_t offset = 0;
        if (line.type == "mask") {
            const auto wanted = pattern(line.address);
            bool found = false;
            for (const auto& span : spans) {
                if (span.second - span.first < wanted.size()) continue;
                for (std::uint64_t at = span.first; at + wanted.size() <= span.second && !found; ++at) {
                    std::size_t i = 0;
                    while (i < wanted.size() && (wanted[i] < 0 || image[at + i] == wanted[i])) ++i;
                    if (i == wanted.size()) { offset = at; found = true; }
                }
                if (found) break;
            }
            if (!found) throw std::runtime_error("mask pattern not found: " + line.address);
            const long long shift = line.offset.empty() ? 0 : std::stoll(line.offset, nullptr, 10);
            offset = static_cast<std::uint64_t>(static_cast<long long>(offset) + shift);
        } else {
            const auto address = parse_number(line.address, true);
            if (address < 0x400000) throw std::runtime_error("address " + line.address + " is below the 0x400000 eboot base");
            offset = address - 0x400000;
        }
        if (!inside(offset, bytes.size())) {
            char where[32]; std::snprintf(where, sizeof(where), "0x%llx", static_cast<unsigned long long>(offset));
            throw std::runtime_error(std::string("write at image offset ") + where + " is outside the eboot's mapped segments");
        }
        writes.push_back({offset, std::move(bytes)});
    }
    return writes;
}

// --- param.sfo (TITLE_ID, APP_VER) --------------------------------------------------------
inline std::map<std::string, std::string> read_sfo(const fs::path& path) {
    const auto data = mods::read_text(path);
    const auto u32 = [&](std::size_t at) -> std::uint32_t {
        if (at + 4 > data.size()) throw std::runtime_error("param.sfo truncated");
        return static_cast<std::uint32_t>(static_cast<unsigned char>(data[at])) | static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + 1])) << 8 |
               static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + 2])) << 16 | static_cast<std::uint32_t>(static_cast<unsigned char>(data[at + 3])) << 24;
    };
    if (data.size() < 20 || data.compare(0, 4, std::string("\0PSF", 4)) != 0) throw std::runtime_error("param.sfo: not a PSF file");
    const auto keys = u32(8), values = u32(12), count = u32(16);
    std::map<std::string, std::string> out;
    for (std::uint32_t i = 0; i < count && i < 4096; ++i) {
        const std::size_t entry = 20 + std::size_t(i) * 16;
        const auto key_offset = u32(entry) & 0xffff, format = (u32(entry) >> 16) & 0xffff, length = u32(entry + 4), value_offset = u32(entry + 12);
        const std::size_t key_at = std::size_t(keys) + key_offset, value_at = std::size_t(values) + value_offset;
        if (key_at >= data.size() || value_at > data.size() || length > data.size() - value_at) throw std::runtime_error("param.sfo entry out of range");
        const auto key_end = data.find('\0', key_at);
        if (key_end == std::string::npos) throw std::runtime_error("param.sfo key unterminated");
        if (format != 0x0204) continue;  // UTF-8 strings only
        std::string value = data.substr(value_at, length);
        if (const auto nul = value.find('\0'); nul != std::string::npos) value.resize(nul);
        out[data.substr(key_at, key_end - key_at)] = value;
    }
    return out;
}

// --- Runtime: apply the selected patches of CTR_PATCHES_DIR to the mapped eboot -----------
// Returns the number of patches applied. Problems in a selected patch stop the launch.
inline int apply_selected(unsigned char* image, const Spans& spans, const std::string& title_id, const std::string& app_ver,
                          const fs::path& dir, const fs::path& selection_path) {
    const auto selection = read_selection(selection_path);
    // Every selected patch is validated before the first byte is written.
    std::vector<std::pair<std::wstring, std::vector<Write>>> chosen_writes;
    for (const auto& file : load_folder(dir)) {
        if (!file.error.empty()) {
            const auto prefix = file.file + L"/";
            const bool chosen = std::any_of(selection.enabled.begin(), selection.enabled.end(),
                                            [&](const std::wstring& key) { return key.rfind(prefix, 0) == 0; });
            std::fprintf(stderr, "Patches: %s: %s%s\n", utf8(file.file).c_str(), file.error.c_str(), chosen ? "" : " (skipped)");
            if (chosen) throw std::runtime_error("selected patch file " + utf8(file.file) + " cannot be read: " + file.error);
            continue;
        }
        for (const auto& patch : file.patches) {
            if (!selected(patch, selection)) continue;
            const auto why = mismatch(patch, title_id, app_ver);
            if (!why.empty()) {
                std::fprintf(stderr, "Patches: %s: skipped, %s\n", utf8(patch.key()).c_str(), why.c_str());
                continue;
            }
            std::vector<Write> writes;
            try { writes = plan(patch, image, spans, patch.app_ver == app_ver); }
            catch (const std::exception& e) { throw std::runtime_error("patch " + utf8(patch.key()) + ": " + e.what()); }
            chosen_writes.push_back({patch.key(), std::move(writes)});
        }
    }
    int applied = 0;
    for (const auto& [key, writes] : chosen_writes) {
        for (const auto& w : writes) std::memcpy(image + w.offset, w.bytes.data(), w.bytes.size());
        std::fprintf(stderr, "Patches: %s: applied, %zu lines\n", utf8(key).c_str(), writes.size());
        for (const auto& w : writes)
            std::printf("{\"scope\":\"guest_xml_patch_applied\",\"patch\":%s,\"target\":%llu,\"bytes\":%zu}\n",
                        mods::json_quote(key).c_str(), static_cast<unsigned long long>(w.offset), w.bytes.size());
        ++applied;
    }
    std::fflush(stdout);
    std::fprintf(stderr, "Patches: %d applied\n", applied);
    std::fflush(stderr);
    return applied;
}
inline int apply_from_environment(unsigned char* image, const Spans& spans, const std::string& title_id, unsigned version) {
    wchar_t dir[32768]{}, config[32768]{};
    const DWORD dir_len = GetEnvironmentVariableW(L"CTR_PATCHES_DIR", dir, 32768);
    if (!dir_len || dir_len >= 32768) return 0;
    const DWORD config_len = GetEnvironmentVariableW(L"CTR_PATCHES_CONFIG", config, 32768);
    const fs::path selection = config_len && config_len < 32768 ? fs::path(config) : fs::path(dir) / L"patches.json";
    char app_ver[8];
    std::snprintf(app_ver, sizeof(app_ver), "%02u.%02u", version / 100, version % 100);
    return apply_selected(image, spans, title_id, app_ver, dir, selection);
}
}
