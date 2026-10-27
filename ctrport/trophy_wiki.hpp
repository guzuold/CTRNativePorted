// SPDX-License-Identifier: GPL-2.0-only
// Trophy texts from the Crash Bandicoot Wiki (crashbandicoot.fandom.com, CC BY-SA), used by the
// launcher's TROPHIES tab when the user has no PS4 trophy key: the "Trophies/Achievements" table
// of the "Crash Team Racing Nitro-Fueled" page gives each trophy's name, description, how to
// unlock it ("Criteria") and grade, in English. The table is read through the MediaWiki API
// (api.php), which answers programs; the wiki's image server does not, and the icons come from
// the dump anyway. Row N is trophy N: checked on 2026-10-27 by comparing the 48 wiki icons with
// TROP000-TROP047.PNG of the EU dump (all 48 nearest, difference < 0.7 against >= 3.3). The
// table is used only when it has as many rows as the dump has trophies. A copy is kept in
// runtime/user/trophies/wiki.json, so the texts stay available offline.
#pragma once
#include "gamebanana.hpp"
#include "trophy_data.hpp"
#include <ctime>

namespace ctr_trophy::wiki {
namespace fs = std::filesystem;
using ctr_online::Json;

constexpr const char* kApi = "https://crashbandicoot.fandom.com/api.php";
constexpr const char* kPage = "Crash_Team_Racing_Nitro-Fueled";
constexpr const wchar_t* kCredit = L"Crash Bandicoot Wiki (crashbandicoot.fandom.com), CC BY-SA";

struct Row {
    std::string name, detail, tip;
    char grade = 0; // 'P', 'G', 'S', 'B' or 0
};

// Wiki markup to plain text: links, bold/italic quotes, templates, tags and entities.
inline std::string plain(std::string s) {
    std::string out;
    for (std::size_t i = 0; i < s.size();) {
        if (s.compare(i, 2, "[[") == 0) {
            const auto end = s.find("]]", i);
            if (end == std::string::npos) break;
            std::string link = s.substr(i + 2, end - i - 2);
            if (const auto bar = link.rfind('|'); bar != std::string::npos) link = link.substr(bar + 1);
            if (link.rfind("File:", 0) != 0) out += link;
            i = end + 2;
        } else if (s.compare(i, 2, "{{") == 0) {
            const auto end = s.find("}}", i);
            i = end == std::string::npos ? s.size() : end + 2;
        } else if (s[i] == '<') {
            const auto end = s.find('>', i);
            if (end == std::string::npos) break;
            if (s.compare(i, 3, "<br") == 0) out += ' ';
            i = end + 1;
        } else if (s.compare(i, 2, "''") == 0) {
            while (i < s.size() && s[i] == '\'') ++i;
        } else {
            out += s[i++];
        }
    }
    const std::pair<const char*, const char*> entities[] = {{"&amp;", "&"}, {"&quot;", "\""}, {"&#39;", "'"},
                                                            {"&lt;", "<"},  {"&gt;", ">"},    {"&nbsp;", " "}};
    for (const auto& [from, to] : entities)
        for (std::size_t at; (at = out.find(from)) != std::string::npos;) out.replace(at, std::strlen(from), to);
    const auto first = out.find_first_not_of(" \t\r\n"), last = out.find_last_not_of(" \t\r\n");
    return first == std::string::npos ? std::string() : out.substr(first, last - first + 1);
}
inline std::string file_of(const std::string& cell) {
    const auto at = cell.find("[[File:");
    if (at == std::string::npos) return {};
    const auto end = cell.find_first_of("|]", at + 7);
    return end == std::string::npos ? std::string() : cell.substr(at + 7, end - at - 7);
}

// The rows of the table: Trophy Name | Image | Trophy Value | Description | Criteria.
// `grade_file` names the image file of each grade (Trophy Value column) -> 'P'/'G'/'S'/'B'.
inline std::vector<Row> parse_table(const std::string& wikitext, const std::map<std::string, char>& grade_file,
                                    std::vector<std::string>* value_files = nullptr) {
    std::vector<Row> rows;
    const auto start = wikitext.find("{|"), end = wikitext.find("|}", start == std::string::npos ? 0 : start);
    if (start == std::string::npos || end == std::string::npos) return rows;
    const std::string table = wikitext.substr(start, end - start);
    std::size_t at = table.find("\n|-");
    while (at != std::string::npos) {
        const auto next = table.find("\n|-", at + 3);
        std::vector<std::string> cells;
        std::istringstream lines(table.substr(at + 3, next == std::string::npos ? std::string::npos : next - at - 3));
        std::string line;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] != '|' || line.rfind("|+", 0) == 0 || line.rfind("|}", 0) == 0) continue;
            // "| a || b" puts several cells on one line
            std::string rest = line.substr(1);
            for (std::size_t sep; (sep = rest.find("||")) != std::string::npos; rest.erase(0, sep + 2))
                cells.push_back(rest.substr(0, sep));
            cells.push_back(rest);
        }
        if (cells.size() >= 5) {
            Row row{plain(cells[0]), plain(cells[3]), plain(cells[4]), 0};
            const std::string value = file_of(cells[2]);
            if (value_files) value_files->push_back(value);
            if (const auto g = grade_file.find(value); g != grade_file.end()) row.grade = g->second;
            if (!row.name.empty()) rows.push_back(std::move(row));
        }
        at = next;
    }
    return rows;
}
// The grade an image file stands for, from the file it redirects to ("Platinum Trophy.PNG").
inline char grade_of(const std::string& target) {
    std::string t = target;
    for (auto& c : t) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (t.find("platinum") != std::string::npos) return 'P';
    if (t.find("gold") != std::string::npos) return 'G';
    if (t.find("silver") != std::string::npos) return 'S';
    if (t.find("bronze") != std::string::npos) return 'B';
    return 0;
}

// Grade images whose file name says nothing (the wiki's bronze and silver ones are
// "IMG 4263.PNG" and "IMG 4264.PNG", not redirects): with exactly two left, the more frequent
// is taken as bronze and the other as silver, and kept only if the set's PSN points
// (platinum 180, gold 90, silver 30, bronze 15) fall in the 1140-1230 of a game with a
// platinum. CTR Nitro-Fueled: 180 + 2*90 + 10*30 + 35*15 = 1185; the swap would give 1560.
inline void complete_grades(const std::vector<std::string>& values, std::map<std::string, char>& grades) {
    std::map<std::string, int> count;
    for (const auto& v : values)
        if (!v.empty()) ++count[v];
    std::vector<std::string> unknown;
    for (const auto& [file, n] : count)
        if (!grades.count(file) || !grades[file]) unknown.push_back(file);
    if (unknown.size() != 2 || count[unknown[0]] == count[unknown[1]]) return;
    if (count[unknown[0]] < count[unknown[1]]) std::swap(unknown[0], unknown[1]);
    auto guess = grades;
    guess[unknown[0]] = 'B';
    guess[unknown[1]] = 'S';
    int points = 0;
    for (const auto& v : values) {
        const auto g = guess.find(v);
        const char grade = g == guess.end() ? 0 : g->second;
        points += grade == 'P' ? 180 : grade == 'G' ? 90 : grade == 'S' ? 30 : grade == 'B' ? 15 : 0;
    }
    if (points >= 1140 && points <= 1230) grades = guess;
}

inline std::string api_get(const std::string& query) {
    return ctrport::gamebanana::http_get(ctrport::gamebanana::wide(std::string(kApi) + "?" + query + "&format=json"));
}
inline std::string escape(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.')
            out += char(c);
        else {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

// Downloads the table: finds the "Trophies/Achievements" section, reads its wikitext, then
// resolves the grade images through the API's redirects.
inline std::vector<Row> fetch() {
    const Json sections = ctr_online::parse_json(api_get(std::string("action=parse&prop=sections&page=") + kPage));
    std::string index;
    if (const Json* parse = sections.find("parse"))
        if (const Json* list = parse->find("sections"))
            for (const Json& s : list->items)
                if (const Json* line = s.find("line"); line && line->text.find("Trophies") != std::string::npos)
                    if (const Json* i = s.find("index")) {
                        index = i->text;
                        break;
                    }
    if (index.empty()) throw std::runtime_error("the wiki page has no Trophies section");
    const Json page =
        ctr_online::parse_json(api_get(std::string("action=parse&prop=wikitext&page=") + kPage + "&section=" + index));
    const Json* parse = page.find("parse");
    const Json* wikitext = parse ? parse->find("wikitext") : nullptr;
    const Json* text = wikitext ? wikitext->find("*") : nullptr;
    if (!text || !text->is_string()) throw std::runtime_error("the wiki answered without the trophy table");
    std::vector<std::string> values;
    parse_table(text->text, {}, &values);
    std::set<std::string> distinct(values.begin(), values.end());
    distinct.erase("");
    std::map<std::string, char> grades;
    if (!distinct.empty()) {
        std::string titles;
        for (const auto& file : distinct) titles += (titles.empty() ? "" : "|") + ("File:" + file);
        const Json files = ctr_online::parse_json(api_get("action=query&redirects=1&titles=" + escape(titles)));
        std::map<std::string, std::string> target; // normalized title -> redirect target
        if (const Json* query = files.find("query")) {
            std::map<std::string, std::string> normalized;
            if (const Json* n = query->find("normalized"))
                for (const Json& e : n->items) normalized[e.at("to").text] = e.at("from").text;
            if (const Json* r = query->find("redirects"))
                for (const Json& e : r->items) target[e.at("from").text] = e.at("to").text;
            for (const auto& file : distinct) {
                std::string title = "File:" + file;
                for (const auto& [to, from] : normalized)
                    if (from == title) title = to;
                const auto t = target.find(title);
                grades[file] = grade_of(t != target.end() ? t->second : title);
            }
        }
    }
    complete_grades(values, grades);
    return parse_table(text->text, grades);
}

// --- Copy in runtime/user/trophies/wiki.json ---------------------------------------------------
inline fs::path cache_file(const fs::path& user_root) {
    return user_root / "trophies" / "wiki.json";
}
inline void write_cache(const fs::path& file, const std::vector<Row>& rows) {
    Json root = Json::object(), list = Json::array();
    root.set("source", Json::of(std::string("https://crashbandicoot.fandom.com/wiki/") + kPage));
    root.set("license", Json::of("CC BY-SA"));
    root.set("fetched", Json::of(static_cast<std::int64_t>(std::time(nullptr))));
    for (const auto& row : rows) {
        Json item = Json::object();
        item.set("name", Json::of(row.name));
        item.set("detail", Json::of(row.detail));
        item.set("tip", Json::of(row.tip));
        item.set("grade", Json::of(row.grade ? std::string(1, row.grade) : std::string()));
        list.items.push_back(std::move(item));
    }
    root.set("trophies", std::move(list));
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary | std::ios::trunc) << ctr_online::dump_json_spaced(root) << "\n";
}
inline std::vector<Row> read_cache(const fs::path& file, std::int64_t* fetched = nullptr) {
    std::vector<Row> rows;
    std::error_code ec;
    if (!fs::exists(file, ec)) return rows;
    const Json root = ctr_online::parse_json(ctrport::mods::read_text(file));
    if (fetched)
        if (const Json* f = root.find("fetched"); f && f->is_integer()) *fetched = f->as_i64();
    if (const Json* list = root.find("trophies"))
        for (const Json& item : list->items) {
            Row row{ctrport::gamebanana::text_of(item, "name"), ctrport::gamebanana::text_of(item, "detail"),
                    ctrport::gamebanana::text_of(item, "tip"), 0};
            const auto grade = ctrport::gamebanana::text_of(item, "grade");
            if (grade.size() == 1) row.grade = grade[0];
            rows.push_back(std::move(row));
        }
    return rows;
}

// Fills the texts of a set read without the key. True when the wiki table fits the dump.
inline bool apply(Set& set, const std::vector<Row>& rows) {
    if (set.texts || rows.empty() || rows.size() != set.trophies.size()) return false;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (set.trophies[i].id != static_cast<int>(i)) return false;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto& t = set.trophies[i];
        t.name = rows[i].name;
        t.detail = rows[i].detail;
        t.tip = rows[i].tip;
        if (rows[i].grade) t.grade = rows[i].grade;
    }
    return true;
}
} // namespace ctr_trophy::wiki
