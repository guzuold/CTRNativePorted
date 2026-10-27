// SPDX-License-Identifier: GPL-2.0-or-later
// Minimal JSON for the project server: objects keep insertion order, integers keep all 64 bits
// (account IDs exceed 2^63), output matches Python json.dumps(separators=(',', ':'),
// ensure_ascii=True), which the Python reference server (tools/online) produced.
#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ctr_online {
struct Json {
    enum class Kind { Null, Bool, Int, Uint, Double, String, Array, Object };
    Kind kind = Kind::Null;
    bool boolean = false;
    std::int64_t integer = 0;         // Kind::Int (negative or small)
    std::uint64_t unsigned_value = 0; // Kind::Uint (>= 0)
    double number = 0;
    std::string text;
    std::vector<Json> items;
    std::vector<std::pair<std::string, Json>> members;

    Json() = default;
    static Json null() { return {}; }
    static Json of(bool value) {
        Json j;
        j.kind = Kind::Bool;
        j.boolean = value;
        return j;
    }
    static Json of(std::uint64_t value) {
        Json j;
        j.kind = Kind::Uint;
        j.unsigned_value = value;
        return j;
    }
    static Json of(std::int64_t value) {
        if (value >= 0) return of(static_cast<std::uint64_t>(value));
        Json j;
        j.kind = Kind::Int;
        j.integer = value;
        return j;
    }
    static Json of(int value) { return of(static_cast<std::int64_t>(value)); }
    static Json of(unsigned value) { return of(static_cast<std::uint64_t>(value)); }
    static Json of(const std::string& value) {
        Json j;
        j.kind = Kind::String;
        j.text = value;
        return j;
    }
    static Json of(const char* value) { return of(std::string(value)); }
    static Json array() {
        Json j;
        j.kind = Kind::Array;
        return j;
    }
    static Json object() {
        Json j;
        j.kind = Kind::Object;
        return j;
    }

    bool is_object() const { return kind == Kind::Object; }
    bool is_array() const { return kind == Kind::Array; }
    bool is_string() const { return kind == Kind::String; }
    bool is_bool() const { return kind == Kind::Bool; }
    bool is_null() const { return kind == Kind::Null; }
    // Python `type(v) is int`: an integer literal (not a bool, not a float).
    bool is_integer() const { return kind == Kind::Int || kind == Kind::Uint; }
    bool is_unsigned() const { return kind == Kind::Uint; }
    std::uint64_t as_u64() const;
    std::int64_t as_i64() const;

    const Json* find(const std::string& key) const;
    Json* find(const std::string& key);
    const Json& at(const std::string& key) const;
    Json& set(const std::string& key, Json value); // replaces or appends
    std::vector<std::string> keys() const;
    bool operator==(const Json& other) const;
    bool operator!=(const Json& other) const { return !(*this == other); }
};

Json parse_json(const std::string& text); // throws std::invalid_argument
std::string dump_json(const Json& value); // compact, ensure_ascii
// Python json.dumps(..., sort_keys=True, separators=(',',':')).
std::string dump_json_sorted(const Json& value);
// Python json.dumps(value) with its default separators (', ', ': ').
std::string dump_json_spaced(const Json& value);
} // namespace ctr_online
