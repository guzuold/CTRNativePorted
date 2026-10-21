// SPDX-License-Identifier: GPL-2.0-or-later
#include "json.hpp"
#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ctr_online {
std::uint64_t Json::as_u64() const {
    if (kind == Kind::Uint) return unsigned_value;
    throw std::invalid_argument("JSON value is not an unsigned integer");
}
std::int64_t Json::as_i64() const {
    if (kind == Kind::Int) return integer;
    if (kind == Kind::Uint && unsigned_value <= 0x7fffffffffffffffull) return static_cast<std::int64_t>(unsigned_value);
    throw std::invalid_argument("JSON value is not a 64-bit signed integer");
}
const Json* Json::find(const std::string& key) const {
    if (kind != Kind::Object) return nullptr;
    for (const auto& [name, value] : members) if (name == key) return &value;
    return nullptr;
}
Json* Json::find(const std::string& key) {
    if (kind != Kind::Object) return nullptr;
    for (auto& [name, value] : members) if (name == key) return &value;
    return nullptr;
}
const Json& Json::at(const std::string& key) const {
    const Json* value = find(key);
    if (!value) throw std::invalid_argument("missing JSON key " + key);
    return *value;
}
Json& Json::set(const std::string& key, Json value) {
    if (kind != Kind::Object) { kind = Kind::Object; members.clear(); }
    for (auto& member : members) if (member.first == key) { member.second = std::move(value); return member.second; }
    members.emplace_back(key, std::move(value));
    return members.back().second;
}
std::vector<std::string> Json::keys() const {
    std::vector<std::string> out;
    for (const auto& member : members) out.push_back(member.first);
    return out;
}
bool Json::operator==(const Json& other) const {
    if (is_integer() && other.is_integer()) {
        if (kind == Kind::Uint && other.kind == Kind::Uint) return unsigned_value == other.unsigned_value;
        if (kind == Kind::Int && other.kind == Kind::Int) return integer == other.integer;
        return false;
    }
    if (kind != other.kind) return false;
    switch (kind) {
    case Kind::Null: return true;
    case Kind::Bool: return boolean == other.boolean;
    case Kind::Double: return number == other.number;
    case Kind::String: return text == other.text;
    case Kind::Array: return items == other.items;
    case Kind::Object: {
        // Python dict equality ignores order.
        if (members.size() != other.members.size()) return false;
        for (const auto& [name, value] : members) {
            const Json* match = other.find(name);
            if (!match || !(*match == value)) return false;
        }
        return true;
    }
    default: return false;
    }
}

namespace {
struct Parser {
    const std::string& s;
    std::size_t i = 0;
    [[noreturn]] void fail(const char* what) { throw std::invalid_argument(std::string("JSON: ") + what); }
    void space() { while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i; }
    bool literal(const char* word) {
        const std::size_t n = std::char_traits<char>::length(word);
        if (s.compare(i, n, word) == 0) { i += n; return true; }
        return false;
    }
    void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    }
    std::uint32_t hex4() {
        if (i + 4 > s.size()) fail("truncated escape");
        std::uint32_t v = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else fail("bad escape");
        }
        return v;
    }
    std::string string() {
        if (i >= s.size() || s[i] != '"') fail("expected string");
        ++i;
        std::string out;
        while (true) {
            if (i >= s.size()) fail("unterminated string");
            const char c = s[i++];
            if (c == '"') return out;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (i >= s.size()) fail("truncated escape");
            const char e = s[i++];
            switch (e) {
            case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
            case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break;
            case 'r': out += '\r'; break; case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t cp = hex4();
                if (cp >= 0xD800 && cp < 0xDC00 && s.compare(i, 2, "\\u") == 0) {
                    i += 2;
                    const std::uint32_t low = hex4();
                    if (low >= 0xDC00 && low < 0xE000) cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    else { append_utf8(out, cp); cp = low; }
                }
                append_utf8(out, cp);
                break;
            }
            default: fail("bad escape");
            }
        }
    }
    Json value(unsigned depth = 0) {
        if (depth > 64) fail("nesting limit exceeded");
        space();
        if (i >= s.size()) fail("unexpected end");
        const char c = s[i];
        if (depth >= 64 && (c == '{' || c == '[')) fail("nesting limit exceeded");
        if (c == '{') {
            ++i; Json object = Json::object(); space();
            if (i < s.size() && s[i] == '}') { ++i; return object; }
            while (true) {
                space(); std::string key = string(); space();
                if (i >= s.size() || s[i] != ':') fail("expected ':'");
                ++i;
                Json item = value(depth + 1);
                // Python json keeps the last duplicate key.
                object.set(key, std::move(item));
                space();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == '}') { ++i; return object; }
                fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            ++i; Json array = Json::array(); space();
            if (i < s.size() && s[i] == ']') { ++i; return array; }
            while (true) {
                array.items.push_back(value(depth + 1)); space();
                if (i < s.size() && s[i] == ',') { ++i; continue; }
                if (i < s.size() && s[i] == ']') { ++i; return array; }
                fail("expected ',' or ']'");
            }
        }
        if (c == '"') return Json::of(string());
        if (literal("true")) return Json::of(true);
        if (literal("false")) return Json::of(false);
        if (literal("null")) return Json::null();
        // Number.
        const std::size_t start = i;
        if (s[i] == '-') ++i;
        if (i >= s.size() || !(s[i] >= '0' && s[i] <= '9')) fail("bad number");
        if (s[i] == '0') ++i; else while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        bool real = false;
        if (i < s.size() && s[i] == '.') { real = true; ++i; if (i >= s.size() || !(s[i] >= '0' && s[i] <= '9')) fail("bad fraction"); while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i; }
        if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) { real = true; ++i; if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i; if (i >= s.size() || !(s[i] >= '0' && s[i] <= '9')) fail("bad exponent"); while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i; }
        const std::string token = s.substr(start, i - start);
        Json j;
        if (real) { j.kind = Json::Kind::Double; j.number = std::strtod(token.c_str(), nullptr); return j; }
        errno = 0;
        if (token[0] == '-') {
            const long long v = std::strtoll(token.c_str(), nullptr, 10);
            if (errno == ERANGE) fail("integer out of 64-bit range");
            return Json::of(static_cast<std::int64_t>(v));
        }
        const unsigned long long v = std::strtoull(token.c_str(), nullptr, 10);
        if (errno == ERANGE) fail("integer out of 64-bit range");
        return Json::of(static_cast<std::uint64_t>(v));
    }
};

void dump_string(std::string& out, const std::string& text) {
    out += '"';
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '"') { out += "\\\""; ++i; continue; }
        if (c == '\\') { out += "\\\\"; ++i; continue; }
        if (c == '\n') { out += "\\n"; ++i; continue; }
        if (c == '\r') { out += "\\r"; ++i; continue; }
        if (c == '\t') { out += "\\t"; ++i; continue; }
        if (c == '\b') { out += "\\b"; ++i; continue; }
        if (c == '\f') { out += "\\f"; ++i; continue; }
        if (c < 0x20) {
            char buf[8]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; ++i; continue;
        }
        if (c < 0x80) { out += static_cast<char>(c); ++i; continue; }
        // UTF-8 sequence -> \uXXXX (surrogate pairs above the BMP), as ensure_ascii does.
        std::uint32_t cp = 0; int extra = 0;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { cp = 0xFFFD; extra = 0; }
        ++i;
        for (int k = 0; k < extra && i < text.size(); ++k, ++i) cp = (cp << 6) | (static_cast<unsigned char>(text[i]) & 0x3F);
        char buf[16];
        if (cp >= 0x10000) {
            cp -= 0x10000;
            std::snprintf(buf, sizeof(buf), "\\u%04x\\u%04x", 0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
        } else {
            std::snprintf(buf, sizeof(buf), "\\u%04x", cp);
        }
        out += buf;
    }
    out += '"';
}

// Python float repr: shortest round-trip digits, scientific below 1e-4 and from 1e16.
std::string python_float(double value) {
    if (std::isnan(value)) return "NaN";
    if (std::isinf(value)) return value > 0 ? "Infinity" : "-Infinity";
    char buf[64];
    const auto result = std::to_chars(buf, buf + sizeof(buf), value, std::chars_format::scientific);
    std::string text(buf, result.ptr);
    std::string sign;
    if (text[0] == '-') { sign = "-"; text.erase(0, 1); }
    const std::size_t e = text.find('e');
    const int exponent = std::atoi(text.c_str() + e + 1);
    std::string digits;
    for (std::size_t k = 0; k < e; ++k) if (text[k] != '.') digits += text[k];
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    if (exponent >= -4 && exponent < 16) {
        std::string fixed;
        if (exponent < 0) fixed = "0." + std::string(static_cast<std::size_t>(-exponent - 1), '0') + digits;
        else if (digits.size() <= static_cast<std::size_t>(exponent) + 1)
            fixed = digits + std::string(exponent + 1 - digits.size(), '0') + ".0";
        else fixed = digits.substr(0, exponent + 1) + "." + digits.substr(exponent + 1);
        return sign + fixed;
    }
    std::string mantissa = digits.substr(0, 1);
    if (digits.size() > 1) mantissa += "." + digits.substr(1);
    char exp[8];
    std::snprintf(exp, sizeof(exp), "e%c%02d", exponent < 0 ? '-' : '+', exponent < 0 ? -exponent : exponent);
    return sign + mantissa + exp;
}

void dump(std::string& out, const Json& v, bool sorted, bool spaced) {
    switch (v.kind) {
    case Json::Kind::Null: out += "null"; break;
    case Json::Kind::Bool: out += v.boolean ? "true" : "false"; break;
    case Json::Kind::Int: out += std::to_string(v.integer); break;
    case Json::Kind::Uint: out += std::to_string(v.unsigned_value); break;
    case Json::Kind::Double: out += python_float(v.number); break;
    case Json::Kind::String: dump_string(out, v.text); break;
    case Json::Kind::Array:
        out += '[';
        for (std::size_t k = 0; k < v.items.size(); ++k) { if (k) out += spaced ? ", " : ","; dump(out, v.items[k], sorted, spaced); }
        out += ']';
        break;
    case Json::Kind::Object: {
        out += '{';
        std::vector<const std::pair<std::string, Json>*> order;
        for (const auto& m : v.members) order.push_back(&m);
        if (sorted) std::sort(order.begin(), order.end(), [](auto* a, auto* b) { return a->first < b->first; });
        for (std::size_t k = 0; k < order.size(); ++k) {
            if (k) out += spaced ? ", " : ",";
            dump_string(out, order[k]->first); out += spaced ? ": " : ":"; dump(out, order[k]->second, sorted, spaced);
        }
        out += '}';
        break;
    }
    }
}
} // namespace

Json parse_json(const std::string& text) {
    Parser p{text};
    Json value = p.value();
    p.space();
    if (p.i != text.size()) p.fail("trailing data");
    return value;
}
std::string dump_json(const Json& value) { std::string out; dump(out, value, false, false); return out; }
std::string dump_json_sorted(const Json& value) { std::string out; dump(out, value, true, false); return out; }
std::string dump_json_spaced(const Json& value) { std::string out; dump(out, value, false, true); return out; }
} // namespace ctr_online
