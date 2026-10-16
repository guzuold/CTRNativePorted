// SPDX-License-Identifier: GPL-2.0-or-later
// Wire formats of the project server, ported from the Python reference server (tools/online):
// typed byte buffers and lobby tasks (demonware_tasks.py), the HTTP proxy task
// (demonware_rest.py), auth3 tickets (demonware_auth3.py), the lobby protocol 210
// (demonware_lobby.py), discovery replies (discovery.py), room selectors (room.py) and the
// session multipart (local_sessions.py). The contracts are documented in those modules and in
// (development notes)
#pragma once
#include "crypto.hpp"
#include "json.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ctr_online {
// Typed byte buffer data types (as numbered in h1-mod byte_buffer, public source).
// In their own namespace: the names clash with Windows integer typedefs.
namespace typed {
enum Kind : int {
    BOOL = 1, INT8, UINT8, WCHAR16, INT16, UINT16, INT32, UINT32, INT64, UINT64,
    FLOAT32 = 13, FLOAT64 = 14, STRING = 16, USTRING = 17, MBSTRING = 18, BLOB = 19,
    // Struct buffer (protobuf message) parameter, observed in retail requests of services 80,
    // 125 and 194: type byte 23, typed u32 size, message bytes (decoded as hex).
    STRUCT = 23,
    UNPARSED = 0, // the rest of a payload the reader could not decode
};
} // namespace typed

// A decoded typed value: numbers as JSON integers or doubles, strings as text, blobs as hex.
struct Param {
    int kind = 0;
    Json value;
    std::string error; // UNPARSED only
};
Json params_json(const std::vector<Param>& params);

// Protocol errors the reference server catches as ValueError.
struct ProtocolError : std::runtime_error { using std::runtime_error::runtime_error; };

class Reader {
public:
    explicit Reader(const Bytes& data) : data_(data) {}
    unsigned raw_u8();
    Param value();
    std::vector<Param> values();
    std::size_t offset() const { return offset_; }
private:
    const Bytes& data_;
    std::size_t offset_ = 0;
};

class Writer {
public:
    Writer& u8(unsigned value);
    Writer& u32(std::uint32_t value);
    Writer& u64(std::uint64_t value);
    Writer& blob(const Bytes& value);
    Writer& structure(const Bytes& message);
    Writer& string(const std::string& value);
    Writer& boolean(bool value);
    Writer& raw(const Bytes& value);
    const Bytes& data() const { return data_; }
private:
    Bytes data_;
};

// HTTP proxy task (REST over LSG, services 194/1 and 193/8).
struct RestRequest {
    std::string method, url;
    std::vector<std::pair<std::string, std::string>> headers;
    Json json() const;
};
RestRequest parse_rest_request(const Bytes& data);
Bytes rest_response(const RestRequest& request);

struct TaskRequest {
    unsigned service = 0;
    std::uint64_t task = 0;
    std::vector<Param> params;
    std::optional<RestRequest> rest;
    Json params_json() const;
};
TaskRequest parse_task_request(const Bytes& payload);
bool is_rest_task(unsigned service, std::uint64_t task);
std::uint64_t next_transaction();
Bytes task_reply(std::uint64_t task, const Writer* results = nullptr, std::uint32_t error = 0);
// Reply carrying a struct buffer: header, struct buffer, end marker (as the REST proxy).
Bytes struct_reply(std::uint64_t task, const Bytes& message);
constexpr std::uint32_t UNSERVED_ERROR = 1;

// Protobuf-style message fields (demonware_rest.fields): number for wire type 0, bytes otherwise.
struct PbField { std::uint64_t tag; unsigned wire; std::uint64_t number; Bytes data; };
std::vector<PbField> pb_fields(const Bytes& data);
Bytes pb_varint(std::uint64_t value);
constexpr int LSG_TASK_REPLY = 1;
const char* service_name(unsigned service);
const char* task_name(unsigned service, std::uint64_t task);

// Room selector: SHA-256 of "ctr-room-v1\0" + password, hex.
std::string room_token(const std::string& password);
bool valid_room_token(const std::string& value);

// auth3.
std::uint64_t user_id(const std::string& online_id); // FNV-1a 64, as the runtime reports it
Bytes client_ticket(std::uint32_t title_id, const std::string& online_id, const Bytes& session_key,
                    std::uint32_t now, std::uint32_t lifetime = 86400);
Bytes server_ticket(const std::string& online_id, const Bytes& session_key);
struct AuthSession {
    std::string online_id;
    std::uint64_t user_id = 0;
    std::uint32_t title_id = 0;
    Bytes session_key;
    std::string room_token;
    unsigned wumpa_multiplier = 1;
};
struct Auth3Reply {
    int status = 400;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;
    std::optional<AuthSession> session;
};
Auth3Reply auth3_reply(const Json& request, const std::string& online_id, const RsaKey& key);

// Lobby protocol 210.
constexpr std::size_t LOBBY_CLIENT_HEADER_SIZE = 28;
Bytes lobby_frame(unsigned type, const Bytes& payload);
class LobbySession {
public:
    explicit LobbySession(const Bytes& keys);
    // data: frame after the size field (0xab 0x85 ...).
    std::pair<unsigned, Bytes> open_record(const Bytes& data);
    Bytes seal_record(const Bytes& payload, unsigned payload_type = 0x87);
private:
    Bytes client_mac_, server_mac_, client_aes_, server_aes_;
    std::uint32_t receive_counter_ = 1, send_counter_ = 1;
};
class LobbyHandshake {
public:
    explicit LobbyHandshake(const Bytes& client_header);
    Bytes server_header() const { return lobby_frame(0x81, header_payload_); }
    std::uint64_t connection_id() const { return connection_id_; }
    struct Result { std::size_t key_index; Bytes done_frame; Bytes keys; };
    // auth_frame: the whole 0x82 frame with its size field; candidates tried in order.
    std::optional<Result> complete(const Bytes& auth_frame, const std::vector<Bytes>& session_keys) const;
private:
    std::uint32_t max_receive_ = 0;
    Bytes client_nonce_, header_payload_;
    std::uint64_t connection_id_ = 0;
};

// Discovery.
struct Endpoint {
    std::string ip;
    std::uint16_t port = 0;
    bool operator==(const Endpoint& o) const { return ip == o.ip && port == o.port; }
    std::string text() const { return ip + ":" + std::to_string(port); }
};
struct DiscoveryLayout {
    Endpoint primary, secondary;
    std::uint16_t alternate_port = 0;
    std::optional<std::uint16_t> secondary_alternate_port;
    std::string public_host; // empty: none
    std::vector<Endpoint> endpoints() const; // request order 0..3
};
Bytes discovery_address(const Endpoint& endpoint, bool protected_form);
std::optional<std::pair<Endpoint, Bytes>> discovery_answer(const Bytes& data, const Endpoint& peer,
                                                           const DiscoveryLayout& layout);
bool is_loopback(const std::string& ip);

// Session multipart: (name, media type, data).
struct Part { std::string name, media; Bytes data; };
std::vector<Part> parse_session_parts(const std::string& content_type, const Bytes& body);

// Python helpers.
std::string utf8_replace(const std::string& raw); // bytes.decode('utf-8', 'replace')
Bytes base64_decode_lenient(const std::string& text); // base64.b64decode(validate=False)
std::string token_urlsafe(std::size_t bytes);
std::string uuid4_hex();
std::uint64_t random_bits(int bits); // secrets.randbits
} // namespace ctr_online
