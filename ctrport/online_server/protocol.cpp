// SPDX-License-Identifier: GPL-2.0-or-later
#include "protocol.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <ctime>
#include <map>
#include <sstream>

namespace ctr_online {
namespace {
std::uint64_t load_le(const unsigned char* p, std::size_t size) {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < size; ++i) v |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    return v;
}
void put_le(Bytes& out, std::uint64_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}
Bytes slice(const Bytes& data, std::size_t from, std::size_t to) {
    from = std::min(from, data.size());
    to = std::min(std::max(to, from), data.size());
    return Bytes(data.begin() + from, data.begin() + to);
}
void append(Bytes& out, const Bytes& more) { out.insert(out.end(), more.begin(), more.end()); }
void append(Bytes& out, const std::string& more) { out.insert(out.end(), more.begin(), more.end()); }

// Protobuf-style fields of the HTTP proxy task.
Bytes varint(std::uint64_t value) {
    Bytes out;
    while (value >= 128) { out.push_back(static_cast<unsigned char>((value & 127) | 128)); value >>= 7; }
    out.push_back(static_cast<unsigned char>(value));
    return out;
}
std::uint64_t read_varint(const Bytes& data, std::size_t& offset) {
    std::uint64_t value = 0;
    for (int shift = 0; shift < 70; shift += 7) {
        if (offset >= data.size()) throw ProtocolError("truncated varint");
        const unsigned byte = data[offset++];
        const std::uint64_t bits = byte & 127;
        if (shift == 63 && bits > 1) { if (!(byte & 128)) throw ProtocolError("varint overflow"); }
        else value |= bits << shift;
        if (!(byte & 128)) {
            if (shift == 63 && bits > 1) throw ProtocolError("varint overflow");
            return value;
        }
    }
    throw ProtocolError("varint overflow");
}
using Field = PbField;
std::vector<Field> fields(const Bytes& data) {
    std::vector<Field> result;
    std::size_t offset = 0;
    while (offset < data.size()) {
        const std::uint64_t key = read_varint(data, offset);
        Field f{key >> 3, static_cast<unsigned>(key & 7), 0, {}};
        if (!f.tag) throw ProtocolError("zero field tag");
        if (f.wire == 0) {
            f.number = read_varint(data, offset);
        } else {
            std::uint64_t size;
            if (f.wire == 2) size = read_varint(data, offset);
            else if (f.wire == 1 || f.wire == 5) size = f.wire == 1 ? 8 : 4;
            else throw ProtocolError("unsupported wire type");
            if (size > data.size() - offset) throw ProtocolError("truncated field");
            f.data = slice(data, offset, offset + static_cast<std::size_t>(size));
            offset += static_cast<std::size_t>(size);
        }
        result.push_back(std::move(f));
    }
    return result;
}
Bytes pb_uint(std::uint64_t tag, std::uint64_t value) { Bytes out = varint(tag << 3); append(out, varint(value)); return out; }
Bytes pb_blob(std::uint64_t tag, const Bytes& value) {
    Bytes out = varint((tag << 3) | 2);
    append(out, varint(value.size()));
    append(out, value);
    return out;
}
// Strict UTF-8 (bytes.decode('utf-8')).
std::string utf8_strict(const Bytes& data) {
    const std::string raw(data.begin(), data.end());
    if (utf8_replace(raw) != raw) throw ProtocolError("invalid UTF-8");
    return raw;
}
std::atomic<std::uint64_t> transactions{0};
} // namespace

std::vector<PbField> pb_fields(const Bytes& data) { return fields(data); }
Bytes pb_varint(std::uint64_t value) { return varint(value); }

Json params_json(const std::vector<Param>& params) {
    Json out = Json::array();
    for (const Param& p : params) {
        Json item = Json::array();
        if (p.kind == typed::UNPARSED) {
            item.items = {Json::of("unparsed"), p.value, Json::of(p.error)};
        } else {
            item.items = {Json::of(p.kind), p.value};
        }
        out.items.push_back(std::move(item));
    }
    return out;
}

unsigned Reader::raw_u8() {
    if (offset_ >= data_.size()) throw ProtocolError("truncated typed byte");
    return data_[offset_++];
}
Param Reader::value() {
    const int kind = static_cast<int>(raw_u8());
    static const std::map<int, std::size_t> sizes = {
        {typed::BOOL, 1}, {typed::INT8, 1}, {typed::UINT8, 1}, {typed::WCHAR16, 2}, {typed::INT16, 2}, {typed::UINT16, 2}, {typed::INT32, 4},
        {typed::UINT32, 4}, {typed::INT64, 8}, {typed::UINT64, 8}, {typed::FLOAT32, 4}, {typed::FLOAT64, 8}};
    Param p;
    p.kind = kind;
    if (const auto it = sizes.find(kind); it != sizes.end()) {
        const std::size_t size = it->second;
        if (data_.size() - offset_ < size) throw ProtocolError("truncated typed number");
        const unsigned char* raw = data_.data() + offset_;
        offset_ += size;
        if (kind == typed::FLOAT32) { float f; std::memcpy(&f, raw, 4); p.value.kind = Json::Kind::Double; p.value.number = f; return p; }
        if (kind == typed::FLOAT64) { double d; std::memcpy(&d, raw, 8); p.value.kind = Json::Kind::Double; p.value.number = d; return p; }
        std::uint64_t v = load_le(raw, size);
        if ((kind == typed::INT8 || kind == typed::INT16 || kind == typed::INT32 || kind == typed::INT64) && size < 8 && (v >> (size * 8 - 1)) & 1)
            v |= ~0ull << (size * 8);
        if (kind == typed::INT8 || kind == typed::INT16 || kind == typed::INT32 || kind == typed::INT64) p.value = Json::of(static_cast<std::int64_t>(v));
        else p.value = Json::of(v);
        return p;
    }
    if (kind == typed::STRING || kind == typed::USTRING || kind == typed::MBSTRING) {
        std::size_t end = offset_;
        while (end < data_.size() && data_[end] != 0) ++end;
        if (end >= data_.size()) throw ProtocolError("subsection not found");
        p.value = Json::of(utf8_replace(std::string(data_.begin() + offset_, data_.begin() + end)));
        offset_ = end + 1;
        return p;
    }
    if (kind == typed::BLOB) {
        const Param length = value();
        if (length.kind != typed::UINT32 || length.value.as_u64() > data_.size() - offset_)
            throw ProtocolError("invalid typed blob length");
        const std::size_t n = static_cast<std::size_t>(length.value.as_u64());
        p.value = Json::of(hex(slice(data_, offset_, offset_ + n)));
        offset_ += n;
        return p;
    }
    if (kind == typed::STRUCT) {
        const Param length = value();
        if (length.kind != typed::UINT32 || length.value.as_u64() > data_.size() - offset_)
            throw ProtocolError("invalid struct buffer length");
        const std::size_t n = static_cast<std::size_t>(length.value.as_u64());
        p.value = Json::of(hex(slice(data_, offset_, offset_ + n)));
        offset_ += n;
        return p;
    }
    throw ProtocolError("unknown data type " + std::to_string(kind) + " at " + std::to_string(offset_ - 1));
}
std::vector<Param> Reader::values() {
    std::vector<Param> out;
    while (offset_ < data_.size()) {
        if (data_[offset_] == 0) { ++offset_; continue; } // trailing zero byte after the last value
        out.push_back(value());
    }
    return out;
}

Writer& Writer::u8(unsigned value) {
    if (value > 255) throw ProtocolError("byte must be in range(0, 256)");
    data_.push_back(typed::UINT8);
    data_.push_back(static_cast<unsigned char>(value));
    return *this;
}
Writer& Writer::u32(std::uint32_t value) { data_.push_back(typed::UINT32); put_le(data_, value, 4); return *this; }
Writer& Writer::u64(std::uint64_t value) { data_.push_back(typed::UINT64); put_le(data_, value, 8); return *this; }
Writer& Writer::blob(const Bytes& value) {
    data_.push_back(typed::BLOB);
    u32(static_cast<std::uint32_t>(value.size()));
    append(data_, value);
    return *this;
}
Writer& Writer::structure(const Bytes& message) {
    data_.push_back(typed::STRUCT);
    u32(static_cast<std::uint32_t>(message.size()));
    append(data_, message);
    return *this;
}
Writer& Writer::string(const std::string& value) {
    data_.push_back(typed::STRING);
    append(data_, value);
    data_.push_back(0);
    return *this;
}
Writer& Writer::boolean(bool value) { data_.push_back(typed::BOOL); data_.push_back(value ? 1 : 0); return *this; }
Writer& Writer::raw(const Bytes& value) { append(data_, value); return *this; }

Json RestRequest::json() const {
    Json out = Json::object();
    out.set("method", Json::of(method));
    out.set("url", Json::of(url));
    Json list = Json::array();
    for (const auto& [name, value] : headers) {
        Json pair = Json::array();
        pair.items = {Json::of(name), Json::of(value)};
        list.items.push_back(std::move(pair));
    }
    out.set("headers", std::move(list));
    return out;
}
RestRequest parse_rest_request(const Bytes& data) {
    // Observed retail request: struct marker 23, typed u32 size, fields, end marker 0.
    if (data.size() < 7 || data[0] != 23 || data[1] != 8) throw ProtocolError("unsupported HTTP proxy envelope");
    const std::uint64_t size = load_le(data.data() + 2, 4);
    if (data.size() != size + 7 || data.back() != 0) throw ProtocolError("invalid HTTP proxy envelope length");
    const std::vector<Field> values = fields(slice(data, 6, data.size() - 1));
    auto text = [&](std::uint64_t tag) {
        const Field* match = nullptr;
        int count = 0;
        for (const Field& f : values) if (f.tag == tag && f.wire == 2) { match = &f; ++count; }
        if (count != 1) throw ProtocolError("missing or duplicate HTTP field");
        return utf8_strict(match->data);
    };
    RestRequest request;
    request.method = text(1);
    request.url = text(2);
    for (const Field& f : values) {
        if (f.tag != 4 || f.wire != 2) continue;
        std::map<std::uint64_t, std::string> header;
        for (const Field& h : fields(f.data)) if (h.wire == 2) header[h.tag] = utf8_strict(h.data);
        if (!header.count(1) || !header.count(2)) throw ProtocolError("invalid HTTP header");
        request.headers.emplace_back(header[1], header[2]);
    }
    return request;
}
namespace {
std::pair<int, std::string> rest_route(const RestRequest& request) {
    // urlsplit: scheme://netloc/path?query#fragment
    std::string scheme, netloc, path;
    const std::size_t colon = request.url.find("://");
    if (colon != std::string::npos) {
        scheme = request.url.substr(0, colon);
        for (char& c : scheme) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const std::string rest = request.url.substr(colon + 3);
        const std::size_t end = rest.find_first_of("/?#");
        netloc = rest.substr(0, end);
        if (end != std::string::npos) path = rest.substr(end, rest.find_first_of("?#", end) - end);
    }
    auto enrollments = [](const std::string& p) {
        const std::string head = "/v2/users/psn-", tail = "/enrollments/";
        if (p.size() <= head.size() + tail.size() || p.compare(0, head.size(), head) || p.compare(p.size() - tail.size(), tail.size(), tail))
            return false;
        for (std::size_t i = head.size(); i < p.size() - tail.size(); ++i) if (p[i] < '0' || p[i] > '9') return false;
        return true;
    };
    if (request.method == "POST" && scheme == "http" && netloc == "abtesting" && enrollments(path))
        return {200, R"({"ABToken":"ctr-local-ab","expiresIn":3600,"enrollments":[]})"};
    return {404, R"({"error":{"name":"NotFound"}})"};
}
} // namespace
Bytes rest_response(const RestRequest& request) {
    const auto [status, body] = rest_route(request);
    Bytes header = pb_blob(1, bytes_of("Content-Type"));
    append(header, pb_blob(2, bytes_of("application/json")));
    Bytes message = pb_blob(1, header);
    append(message, pb_uint(2, static_cast<std::uint64_t>(status)));
    append(message, pb_blob(3, bytes_of(body)));
    Bytes out{23, 8};
    put_le(out, message.size(), 4);
    append(out, message);
    out.push_back(0);
    return out;
}

bool is_rest_task(unsigned service, std::uint64_t task) {
    return (service == 194 && task == 1) || (service == 193 && task == 8);
}
Json TaskRequest::params_json() const { return rest ? rest->json() : ctr_online::params_json(params); }
TaskRequest parse_task_request(const Bytes& payload) {
    Reader reader(payload);
    TaskRequest request;
    request.service = reader.raw_u8();
    const Param task = reader.value();
    if (!task.value.is_integer() || task.value.kind == Json::Kind::Int) throw ProtocolError("task ID is not an unsigned integer");
    request.task = task.value.as_u64();
    if (is_rest_task(request.service, request.task)) {
        request.rest = parse_rest_request(slice(payload, reader.offset(), payload.size()));
        return request;
    }
    const std::size_t start = reader.offset();
    try {
        request.params = reader.values();
    } catch (const ProtocolError& error) {
        Param p;
        p.kind = typed::UNPARSED;
        p.value = Json::of(hex(slice(payload, start, payload.size())));
        p.error = error.what();
        request.params = {p};
    }
    return request;
}
std::uint64_t next_transaction() { return ++transactions; }
Bytes task_reply(std::uint64_t task, const Writer* results, std::uint32_t error) {
    Writer writer;
    writer.u64(next_transaction()).u32(error).u8(static_cast<unsigned>(task));
    if (!error) {
        if (!results) writer.u32(0);
        else writer.raw(results->data());
    }
    return writer.data();
}
Bytes struct_reply(std::uint64_t task, const Bytes& message) {
    Writer writer;
    writer.u64(next_transaction()).u32(0).u8(static_cast<unsigned>(task)).structure(message);
    Bytes out = writer.data();
    out.push_back(0);
    return out;
}
const char* service_name(unsigned service) {
    // "anticheat" is the project's name; reportExtendedAuthInfo appears in the retail eboot.
    if (service == 38) return "anticheat";
    if (service == 194) return "REST over LSG";
    return "?";
}
const char* task_name(unsigned service, std::uint64_t task) {
    return service == 38 && task == 6 ? "reportExtendedAuthInfo" : "?";
}

std::string room_token(const std::string& password) {
    if (password.empty() || password.size() > 256) throw std::invalid_argument("room password must contain 1..256 UTF-8 bytes");
    Bytes data = bytes_of(std::string("ctr-room-v1", 11));
    data.push_back(0);
    append(data, password);
    return hex(sha256(data));
}
// Public matchmaking: a player that sends no room token searches with everyone else on the
// server. A distinct prefix keeps it apart from every password-derived token.
const std::string& public_room_token() {
    static const std::string token = hex(sha256(bytes_of(std::string("ctr-public-room-v1", 18))));
    return token;
}
bool valid_room_token(const std::string& value) {
    if (value.size() != 64) return false;
    for (const char c : value) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

std::uint64_t user_id(const std::string& online_id) {
    std::uint64_t value = 0xcbf29ce484222325ull;
    for (const unsigned char byte : online_id) value = (value ^ byte) * 0x100000001b3ull;
    return value ? value : 1;
}
namespace {
Bytes name64(const std::string& online_id) {
    Bytes name(online_id.begin(), online_id.begin() + std::min<std::size_t>(online_id.size(), 63));
    name.resize(64, 0);
    return name;
}
} // namespace
Bytes client_ticket(std::uint32_t title_id, const std::string& online_id, const Bytes& session_key,
                    std::uint32_t now, std::uint32_t lifetime) {
    Bytes ticket;
    put_le(ticket, 0xEFBDADDEu, 4);
    ticket.push_back(0);
    put_le(ticket, title_id, 4);
    put_le(ticket, now, 4);
    put_le(ticket, static_cast<std::uint32_t>(now + lifetime), 4);
    put_le(ticket, 0, 8);
    put_le(ticket, user_id(online_id), 8);
    append(ticket, name64(online_id));
    append(ticket, session_key);
    ticket.resize(ticket.size() + 7, 0);
    if (ticket.size() != 0x80) throw std::logic_error("client ticket size");
    return ticket;
}
Bytes server_ticket(const std::string& online_id, const Bytes& session_key) {
    // Project format, read back by the project's lobby: session key | user ID | user name.
    Bytes ticket = session_key;
    put_le(ticket, user_id(online_id), 8);
    append(ticket, name64(online_id));
    ticket.resize(0x80, 0);
    return ticket;
}
namespace {
// Python int(value) for the auth3 request fields (strings of digits or numbers).
Json python_int(const Json* value) {
    if (!value) return Json::of(0);
    if (value->is_integer()) return *value;
    if (value->is_bool()) return Json::of(value->boolean ? 1 : 0);
    if (value->kind == Json::Kind::Double) return Json::of(static_cast<std::int64_t>(value->number));
    if (value->is_string()) {
        std::string t = value->text;
        const std::size_t a = t.find_first_not_of(" \t\r\n\f\v"), b = t.find_last_not_of(" \t\r\n\f\v");
        if (a == std::string::npos) throw ProtocolError("invalid literal for int()");
        t = t.substr(a, b - a + 1);
        std::string digits;
        bool negative = false;
        std::size_t i = 0;
        if (t[0] == '+' || t[0] == '-') { negative = t[0] == '-'; i = 1; }
        for (; i < t.size(); ++i) {
            if (t[i] >= '0' && t[i] <= '9') digits += t[i];
            else if (t[i] == '_' && !digits.empty() && i + 1 < t.size() && t[i + 1] >= '0' && t[i + 1] <= '9') continue;
            else throw ProtocolError("invalid literal for int()");
        }
        if (digits.empty()) throw ProtocolError("invalid literal for int()");
        return parse_json((negative ? "-" : "") + digits.substr(std::min(digits.find_first_not_of('0'), digits.size() - 1)));
    }
    throw ProtocolError("int() argument must be a string or a number");
}
} // namespace
Auth3Reply auth3_reply(const Json& request, const std::string& online_id, const RsaKey& key) {
    if (!request.is_object()) throw ProtocolError("auth3 request is not an object");
    Auth3Reply out;
    const Json task = python_int(request.find("auth_task"));
    if (!(task.is_unsigned() && task.as_u64() == 46)) {
        out.body = R"({"error":"unsupported auth_task"})";
        return out;
    }
    const Bytes session_key = random_bytes(24);
    const Json title = python_int(request.find("title_id"));
    if (!title.is_unsigned() || title.as_u64() > 0xffffffffull) throw ProtocolError("title_id outside uint32");
    const auto title_id = static_cast<std::uint32_t>(title.as_u64());
    const Json iv_seed = python_int(request.find("iv_seed"));
    Json extra = Json::object();
    extra.set("extended_data", Json::of("{}"));
    Json body = Json::object();
    body.set("auth_task", Json::of("47"));
    body.set("code", Json::of("700"));
    body.set("iv_seed", Json::of(dump_json(iv_seed)));
    body.set("client_ticket", Json::of(base64_encode(client_ticket(title_id, online_id, session_key,
                                                                   static_cast<std::uint32_t>(std::time(nullptr))))));
    body.set("server_ticket", Json::of(base64_encode(server_ticket(online_id, session_key))));
    body.set("client_id", Json::of(""));
    body.set("account_type", Json::of("psn"));
    body.set("extra_data", Json::of(dump_json_spaced(extra)));
    out.status = 200;
    out.body = dump_json(body);
    out.headers.emplace_back("X-Signature", base64_encode(key.sign_pss_sha256(bytes_of(out.body))));
    out.session = AuthSession{online_id, user_id(online_id), title_id, session_key, {}};
    return out;
}

Bytes lobby_frame(unsigned type, const Bytes& payload) {
    Bytes out;
    put_le(out, payload.size() + 2, 4);
    out.push_back(0xab);
    out.push_back(static_cast<unsigned char>(type));
    append(out, payload);
    return out;
}
LobbySession::LobbySession(const Bytes& keys)
    : client_mac_(slice(keys, 0, 20)), server_mac_(slice(keys, 20, 40)),
      client_aes_(slice(keys, 40, 56)), server_aes_(slice(keys, 56, 72)) {}
std::pair<unsigned, Bytes> LobbySession::open_record(const Bytes& data) {
    if (data.size() < 30) throw ProtocolError("short lobby record");
    Bytes whole;
    put_le(whole, data.size(), 4);
    append(whole, data);
    const Bytes expected = hmac_sha1(client_mac_, slice(whole, 0, whole.size() - 8));
    if (!constant_time_equal(expected.data(), whole.data() + whole.size() - 8, 8))
        throw ProtocolError("lobby record HMAC mismatch");
    const auto counter = static_cast<std::uint32_t>(load_le(data.data() + 2, 4));
    if (counter != receive_counter_)
        throw ProtocolError("lobby record counter " + std::to_string(counter) + ", expected " + std::to_string(receive_counter_));
    ++receive_counter_;
    const Bytes encrypted = slice(data, 22, data.size() - 8);
    if (encrypted.empty() || encrypted.size() % 16) throw ProtocolError("lobby record size");
    const Bytes plain = aes128_cbc_decrypt(client_aes_, slice(data, 6, 22), encrypted);
    const auto length = static_cast<std::size_t>(load_le(plain.data(), 4));
    return {plain[4], slice(plain, 5, 5 + length)};
}
Bytes LobbySession::seal_record(const Bytes& payload, unsigned payload_type) {
    const std::size_t size = (payload.size() + 0x14) & ~static_cast<std::size_t>(0xf);
    Bytes plain;
    put_le(plain, payload.size(), 4);
    plain.push_back(static_cast<unsigned char>(payload_type));
    append(plain, payload);
    plain.resize(size, 0);
    const Bytes iv = random_bytes(16);
    Bytes body{0xab, 0x85};
    put_le(body, send_counter_++, 4);
    append(body, iv);
    append(body, aes128_cbc_encrypt(server_aes_, iv, plain));
    Bytes head;
    put_le(head, body.size() + 8, 4);
    append(head, body);
    const Bytes mac = hmac_sha1(server_mac_, head);
    head.insert(head.end(), mac.begin(), mac.begin() + 8);
    return head;
}
LobbyHandshake::LobbyHandshake(const Bytes& header) {
    if (header.size() != LOBBY_CLIENT_HEADER_SIZE) throw ProtocolError("lobby client header size");
    const unsigned char* p = header.data();
    if (load_le(p, 4) != 200 || load_le(p + 4, 4) != 200 || load_le(p + 8, 4) != 0xd2 || load_le(p + 12, 4) != 0xdc)
        throw ProtocolError("unexpected lobby client header");
    max_receive_ = static_cast<std::uint32_t>(load_le(p + 16, 4));
    client_nonce_ = slice(header, 20, 28);
    connection_id_ = random_u64();
    put_le(header_payload_, 0xd2, 4); // lowest offered protocol: no 220 connection migration
    put_le(header_payload_, connection_id_, 8);
    append(header_payload_, random_bytes(8));
}
std::optional<LobbyHandshake::Result> LobbyHandshake::complete(const Bytes& auth_frame,
                                                               const std::vector<Bytes>& session_keys) const {
    if (auth_frame.size() < 14 || auth_frame[4] != 0xab || auth_frame[5] != 0x82)
        throw ProtocolError("not a lobby client auth frame");
    Bytes material;
    put_le(material, 0xd2, 4);
    put_le(material, 0xdc, 4);
    put_le(material, max_receive_, 4);
    append(material, client_nonce_);
    put_le(material, header_payload_.size() + 2, 4);
    material.push_back(0xab);
    material.push_back(0x81);
    append(material, header_payload_);
    append(material, slice(auth_frame, 0, auth_frame.size() - 8));
    const Bytes salt = sha1(material);
    for (std::size_t i = 0; i < session_keys.size(); ++i) {
        const Bytes prk = hmac_sha1(salt, session_keys[i]);
        const Bytes challenge = hkdf_expand_sha1(prk, "CLIENTCHAL", 16);
        if (constant_time_equal(challenge.data(), auth_frame.data() + auth_frame.size() - 8, 8))
            return Result{i, lobby_frame(0x83, slice(challenge, 8, 16)), hkdf_expand_sha1(prk, "BDDATA", 72)};
    }
    return std::nullopt;
}

namespace {
std::uint32_t parse_ipv4(const std::string& ip) {
    std::uint32_t value = 0;
    int parts = 0;
    std::size_t i = 0;
    while (parts < 4) {
        if (i >= ip.size() || ip[i] < '0' || ip[i] > '9') throw ProtocolError("invalid IPv4 address " + ip);
        unsigned part = 0, digits = 0;
        while (i < ip.size() && ip[i] >= '0' && ip[i] <= '9' && digits < 4) { part = part * 10 + (ip[i++] - '0'); ++digits; }
        if (part > 255 || digits > 3) throw ProtocolError("invalid IPv4 address " + ip);
        value = value << 8 | part;
        if (++parts < 4) { if (i >= ip.size() || ip[i] != '.') throw ProtocolError("invalid IPv4 address " + ip); ++i; }
    }
    if (i != ip.size()) throw ProtocolError("invalid IPv4 address " + ip);
    return value;
}
} // namespace
bool is_loopback(const std::string& ip) {
    try { return (parse_ipv4(ip) >> 24) == 127; } catch (const ProtocolError&) { return false; }
}
std::vector<Endpoint> DiscoveryLayout::endpoints() const {
    const std::uint16_t sa = secondary_alternate_port.value_or(alternate_port);
    return {primary, {primary.ip, alternate_port}, secondary, {secondary.ip, sa}};
}
Bytes discovery_address(const Endpoint& endpoint, bool protected_form) {
    std::uint32_t value = parse_ipv4(endpoint.ip);
    std::uint16_t port = endpoint.port;
    if (protected_form) {
        // The original x86 client XORs the in-memory IPv4 uint32, without ntohl.
        value ^= 0x42a41221u;
        port ^= 0x2112;
    }
    Bytes out{static_cast<unsigned char>(value >> 24), static_cast<unsigned char>(value >> 16),
              static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value)};
    put_le(out, port, 2);
    return out;
}
std::optional<std::pair<Endpoint, Bytes>> discovery_answer(const Bytes& data, const Endpoint& peer,
                                                           const DiscoveryLayout& layout) {
    const Endpoint seen = !layout.public_host.empty() && is_loopback(peer.ip) ? Endpoint{layout.public_host, peer.port} : peer;
    if (data == Bytes{30, 3, 0}) {
        Bytes reply{31, 3, 0};
        append(reply, discovery_address(seen, false));
        append(reply, discovery_address(seen, true));
        return std::make_pair(layout.primary, reply);
    }
    if (data.size() == 4 && data[0] == 20 && data[1] == 2 && data[2] == 0 && data[3] <= 3) {
        const bool change_address = data[3] & 2, change_port = data[3] & 1;
        const std::uint16_t sa = layout.secondary_alternate_port.value_or(layout.alternate_port);
        const Endpoint& base = change_address ? layout.secondary : layout.primary;
        const std::uint16_t port = change_port ? (change_address ? sa : layout.alternate_port) : base.port;
        Bytes reply{21, 2, 0};
        append(reply, discovery_address(seen, false));
        append(reply, discovery_address(layout.secondary, false));
        return std::make_pair(Endpoint{base.ip, port}, reply);
    }
    return std::nullopt;
}

namespace {
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r\n\f\v");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n\f\v") - a + 1);
}
std::size_t find(const Bytes& data, const std::string& needle, std::size_t from) {
    if (from > data.size()) return std::string::npos;
    const auto it = std::search(data.begin() + from, data.end(), needle.begin(), needle.end());
    return it == data.end() ? std::string::npos : static_cast<std::size_t>(it - data.begin());
}
bool at(const Bytes& data, std::size_t offset, const std::string& text) {
    return offset + text.size() <= data.size() && std::equal(text.begin(), text.end(), data.begin() + offset);
}
} // namespace
std::vector<Part> parse_session_parts(const std::string& content_type, const Bytes& body) {
    // email.message.Message content type and boundary parameter.
    std::vector<std::string> pieces;
    {
        std::string current;
        bool quoted = false;
        for (const char c : content_type) {
            if (c == '"') quoted = !quoted;
            if (c == ';' && !quoted) { pieces.push_back(current); current.clear(); } else current += c;
        }
        pieces.push_back(current);
    }
    std::string media = lower(trim(pieces[0]));
    if (std::count(media.begin(), media.end(), '/') != 1) media = "text/plain";
    std::string boundary;
    for (std::size_t i = 1; i < pieces.size(); ++i) {
        const std::size_t eq = pieces[i].find('=');
        if (eq == std::string::npos || lower(trim(pieces[i].substr(0, eq))) != "boundary") continue;
        boundary = trim(pieces[i].substr(eq + 1));
        if (boundary.size() >= 2 && boundary.front() == '"' && boundary.back() == '"') boundary = boundary.substr(1, boundary.size() - 2);
    }
    if (media != "multipart/mixed" || boundary.empty() || body.size() > 2 * 1024 * 1024)
        throw ProtocolError("unsupported session multipart");
    for (const unsigned char c : boundary) if (c >= 0x80) throw ProtocolError("non-ASCII boundary");
    const std::string marker = "--" + boundary;
    std::size_t offset = 0;
    std::vector<Part> parts;
    while (at(body, offset, marker)) {
        offset += marker.size();
        if (at(body, offset, "--")) {
            for (std::size_t i = offset + 2; i < body.size(); ++i)
                if (body[i] != '\r' && body[i] != '\n') throw ProtocolError("trailing multipart data");
            return parts;
        }
        if (!at(body, offset, "\r\n")) throw ProtocolError("invalid multipart delimiter");
        offset += 2;
        const std::size_t end = find(body, "\r\n\r\n", offset);
        if (end == std::string::npos) throw ProtocolError("substring not found");
        std::map<std::string, std::string> headers;
        const std::string block(body.begin() + offset, body.begin() + end);
        for (const unsigned char c : block) if (c >= 0x80) throw ProtocolError("non-ASCII part header");
        std::size_t line_start = 0;
        while (true) {
            const std::size_t line_end = block.find("\r\n", line_start);
            const std::string line = block.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
            const std::size_t colon = line.find(':');
            if (colon == std::string::npos) throw ProtocolError("part header without colon");
            headers[lower(line.substr(0, colon))] = trim(line.substr(colon + 1));
            if (line_end == std::string::npos) break;
            line_start = line_end + 2;
        }
        auto header = [&](const char* name) -> const std::string& {
            const auto it = headers.find(name);
            if (it == headers.end()) throw ProtocolError(std::string("missing part header ") + name);
            return it->second;
        };
        const std::string& length_text = header("content-length");
        long long size = 0;
        {
            std::size_t i = 0;
            bool negative = false;
            if (!length_text.empty() && (length_text[0] == '+' || length_text[0] == '-')) { negative = length_text[0] == '-'; i = 1; }
            if (i >= length_text.size()) throw ProtocolError("invalid content-length");
            for (; i < length_text.size(); ++i) {
                if (length_text[i] < '0' || length_text[i] > '9' || size > (1ll << 40)) throw ProtocolError("invalid content-length");
                size = size * 10 + (length_text[i] - '0');
            }
            if (negative) size = -size;
        }
        if (size < 0 || parts.size() >= 8) throw ProtocolError("invalid session part length");
        offset = end + 4;
        if (static_cast<std::size_t>(size) > body.size() - offset) throw ProtocolError("truncated session part");
        Part part{header("content-description"), header("content-type"),
                  Bytes(body.begin() + offset, body.begin() + offset + static_cast<std::size_t>(size))};
        offset += static_cast<std::size_t>(size);
        const std::string name = part.name;
        parts.push_back(std::move(part));
        // The original retail Toolkit sends this final part without a closing MIME delimiter.
        // Accept only exact EOF after its declared payload.
        if (offset == body.size() && name == "changeable-session-data") return parts;
        if (!at(body, offset, "\r\n")) throw ProtocolError("missing session part separator");
        while (at(body, offset, "\r\n")) offset += 2;
    }
    throw ProtocolError("missing multipart terminator");
}

std::string utf8_replace(const std::string& raw) {
    // Python's decoder: each maximal invalid subpart becomes one U+FFFD.
    static const std::string replacement = "\xEF\xBF\xBD";
    std::string out;
    const auto* s = reinterpret_cast<const unsigned char*>(raw.data());
    const std::size_t n = raw.size();
    std::size_t i = 0;
    while (i < n) {
        const unsigned char c = s[i];
        if (c < 0x80) { out += static_cast<char>(c); ++i; continue; }
        int need = 0;
        unsigned char lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) need = 1;
        else if (c >= 0xE0 && c <= 0xEF) { need = 2; if (c == 0xE0) lo = 0xA0; if (c == 0xED) hi = 0x9F; }
        else if (c >= 0xF0 && c <= 0xF4) { need = 3; if (c == 0xF0) lo = 0x90; if (c == 0xF4) hi = 0x8F; }
        else { out += replacement; ++i; continue; }
        std::size_t j = i + 1;
        int got = 0;
        while (got < need && j < n) {
            const unsigned char b = s[j];
            const unsigned char l = got == 0 ? lo : 0x80, h = got == 0 ? hi : 0xBF;
            if (b < l || b > h) break;
            ++j; ++got;
        }
        if (got == need) out.append(raw, i, j - i);
        else out += replacement;
        i = j;
    }
    return out;
}
Bytes base64_decode_lenient(const std::string& text) {
    // Non-alphabet characters are discarded, as binascii does without validate.
    std::string kept;
    for (const char c : text)
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=') kept += c;
    const std::size_t pad = kept.find('=');
    std::string data = kept.substr(0, pad);
    if (data.size() % 4 == 1) throw ProtocolError("invalid base64");
    while (data.size() % 4) {
        if (pad == std::string::npos) throw ProtocolError("Incorrect padding");
        data += '=';
    }
    return base64_decode(data);
}
std::string token_urlsafe(std::size_t bytes) {
    std::string text = base64_encode(random_bytes(bytes));
    for (char& c : text) { if (c == '+') c = '-'; else if (c == '/') c = '_'; }
    while (!text.empty() && text.back() == '=') text.pop_back();
    return text;
}
std::string uuid4_hex() {
    Bytes b = random_bytes(16);
    b[6] = static_cast<unsigned char>((b[6] & 0x0f) | 0x40);
    b[8] = static_cast<unsigned char>((b[8] & 0x3f) | 0x80);
    return hex(b);
}
std::uint64_t random_bits(int bits) {
    const std::uint64_t v = random_u64();
    return bits >= 64 ? v : v & ((1ull << bits) - 1);
}
} // namespace ctr_online
