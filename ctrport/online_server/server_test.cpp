// SPDX-License-Identifier: GPL-2.0-or-later
// Unit checks of the C++ project server's building blocks against published test vectors and
// against byte strings produced by the Python reference modules (tools/online). The
// network-level comparison of the two servers is tools/online/e2e_server_check.py.
#include "crypto.hpp"
#include "json.hpp"
#include "protocol.hpp"
#include "relay.hpp"
#include "stores.hpp"
#include "launcher_online.hpp"
#include "upnp.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "bcrypt.lib")

using namespace ctr_online;
namespace {
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("{\"scope\": \"online_server_test\", \"check\": \"%s\", \"ok\": %s}\n", what, ok ? "true" : "false");
    if (!ok) ++failures;
}
Bytes b(const std::string& h) { return from_hex(h); }
Param param(int kind, Json value) { Param p; p.kind = kind; p.value = std::move(value); return p; }
} // namespace

int main() {
    // JSON: order, 64-bit integers, Python compact and default separators, ensure_ascii.
    const Json parsed = parse_json("{\"b\":18446744073709551615,\"a\":[-5,1.5,true,null],\"t\":\"\xC3\xA8\xF0\x9F\x98\x80\\\"\\n\"}");
    check(dump_json(parsed) == "{\"b\":18446744073709551615,\"a\":[-5,1.5,true,null],\"t\":\"\\u00e8\\ud83d\\ude00\\\"\\n\"}",
          "JSON round trip keeps order, u64 and ensure_ascii escapes");
    check(dump_json(parse_json("\"\\u00e8\\ud83d\\ude00\"")) == "\"\\u00e8\\ud83d\\ude00\"", "JSON escapes parsed and written back");
    check(dump_json_spaced(parse_json(R"({"x":[1,2],"y":{}})")) == R"({"x": [1, 2], "y": {}})", "JSON default separators");
    check(dump_json_sorted(parse_json(R"({"z":1,"a":{"d":1,"c":2}})")) == R"({"a":{"c":2,"d":1},"z":1})", "JSON sorted keys");
    check(dump_json(parse_json("[100.0,1e16,0.0001,1760480000.123]")) == "[100.0,1e+16,0.0001,1760480000.123]", "JSON float repr");
    check(parse_json(R"({"a":1,"b":2})") == parse_json(R"({"b":2,"a":1})"), "JSON object equality ignores order");

    // Hashes and HMAC (FIPS 180 "abc", RFC 2202 case 2), AES-128-CBC (SP 800-38A F.2.1).
    check(hex(sha1(bytes_of("abc"))) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 abc");
    check(hex(sha256(bytes_of("abc"))) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 abc");
    check(hex(hmac_sha1(bytes_of("Jefe"), bytes_of("what do ya want for nothing?"))) == "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79",
          "HMAC-SHA1 RFC 2202");
    const Bytes key = b("2b7e151628aed2a6abf7158809cf4f3c"), iv = b("000102030405060708090a0b0c0d0e0f");
    const Bytes plain = b("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51");
    const Bytes cipher = aes128_cbc_encrypt(key, iv, plain);
    check(hex(cipher) == "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2", "AES-128-CBC SP 800-38A");
    check(aes128_cbc_decrypt(key, iv, cipher) == plain, "AES-128-CBC decrypt");
    check(base64_encode(bytes_of("ab")) == "YWI=" && base64_decode("YWI=") == bytes_of("ab"), "base64");
    bool rejected = false;
    try { base64_decode("YW I="); } catch (const std::exception&) { rejected = true; }
    check(rejected, "strict base64 rejects non-alphabet characters");

    // RSA key: SPKI size, persistence, PSS signature verified by CNG with salt 0.
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ctr_online_server_test";
    std::filesystem::remove_all(dir);
    {
        RsaKey rsa(dir / "key.cng");
        check(rsa.spki().size() == 0x126, "SPKI DER is 0x126 bytes");
        RsaKey again(dir / "key.cng");
        check(again.spki() == rsa.spki(), "server key persists across starts");
        const Bytes message = bytes_of(R"({"auth_task":"47"})");
        const Bytes signature = rsa.sign_pss_sha256(message);
        BCRYPT_KEY_HANDLE pub = nullptr;
        CERT_PUBLIC_KEY_INFO* info = nullptr;
        DWORD info_size = 0;
        const bool decoded = CryptDecodeObjectEx(X509_ASN_ENCODING, X509_PUBLIC_KEY_INFO, rsa.spki().data(), static_cast<DWORD>(rsa.spki().size()),
                                                 CRYPT_DECODE_ALLOC_FLAG, nullptr, &info, &info_size);
        bool verified = false, tampered = true;
        if (decoded && CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING, info, 0, nullptr, &pub)) {
            const Bytes digest = sha256(message);
            BCRYPT_PSS_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM, 0};
            verified = BCryptVerifySignature(pub, &padding, const_cast<PUCHAR>(digest.data()), 32, const_cast<PUCHAR>(signature.data()),
                                             static_cast<ULONG>(signature.size()), BCRYPT_PAD_PSS) == 0;
            const Bytes other = sha256(bytes_of(R"({"auth_task":"48"})"));
            tampered = BCryptVerifySignature(pub, &padding, const_cast<PUCHAR>(other.data()), 32, const_cast<PUCHAR>(signature.data()),
                                             static_cast<ULONG>(signature.size()), BCRYPT_PAD_PSS) == 0;
            BCryptDestroyKey(pub);
        }
        if (info) LocalFree(info);
        check(verified && !tampered, "PSS-SHA256 salt 0 signature verifies from the SPKI, altered message fails");
    }

    // Room selector and user ID: values printed by tools/online/room.py and demonware_auth3.py.
    check(room_token("same") == "97f972ee94f7ad1f72c34b02a6a566364b43884c57aae15d8459775e7497de91", "room selector as room.py");
    check(user_id("CTRTester1") == 0xa4d0dbda95b833e6ull, "user ID as demonware_auth3.user_id");

    // auth3 tickets: layout of demonware_auth3.client_ticket.
    const Bytes session_key(24, 0x11);
    const Bytes ticket = client_ticket(5760, "CTRTester1", session_key, 1000);
    check(ticket.size() == 0x80 && hex(Bytes(ticket.begin(), ticket.begin() + 4)) == "deadbdef" && ticket[4] == 0 &&
          Bytes(ticket.begin() + 0x61, ticket.begin() + 0x79) == session_key, "client ticket layout");
    const Bytes server = server_ticket("CTRTester1", session_key);
    check(server.size() == 0x80 && Bytes(server.begin(), server.begin() + 24) == session_key, "server ticket carries the session key");

    // Typed buffers.
    Writer w;
    w.u64(7).u32(0).u8(18).string("{\"mm_id\":5}").boolean(true).blob(b("0102"));
    Reader r(w.data());
    const auto values = r.values();
    check(values.size() == 6 && values[0].value.as_u64() == 7 && values[3].value.text == "{\"mm_id\":5}" && values[4].kind == typed::BOOL &&
          values[5].value.text == "0102", "typed writer and reader");

    // HTTP proxy task: the bytes of demonware_rest.response.
    RestRequest rest{"POST", "http://abtesting/v2/users/psn-123/enrollments/", {}};
    check(hex(rest_response(rest)) == "1708630000000a200a0c436f6e74656e742d5479706512106170706c69636174696f6e2f6a736f6e10c8011a3c7b224142546f6b656e223a226374722d6c6f63616c2d6162222c2265787069726573496e223a333630302c22656e726f6c6c6d656e7473223a5b5d7d00",
          "REST A/B response bytes as demonware_rest.response");
    check(hex(rest_response(RestRequest{"GET", "http://x/", {}})) == "1708440000000a200a0c436f6e74656e742d5479706512106170706c69636174696f6e2f6a736f6e1094031a1d7b226572726f72223a7b226e616d65223a224e6f74466f756e64227d7d00",
          "REST 404 response bytes");

    // Discovery: the cases of tools/online/test_discovery.py and test_internet.py.
    DiscoveryLayout one{{"203.0.113.7", 3078}, {"203.0.113.7", 3080}, 3079, std::uint16_t(3081), "203.0.113.7"};
    auto ip = discovery_answer(Bytes{30, 3, 0}, {"127.0.0.1", 3174}, one);
    check(ip && ip->first == Endpoint{"203.0.113.7", 3078} && hex(ip->second) == "1f0300cb007107660c89a46326742d",
          "IP request answered from primary, loopback reported with the public address (discovery.py bytes)");
    auto nat = discovery_answer(Bytes{20, 2, 0, 3}, {"198.51.100.9", 4000}, one);
    check(nat && nat->first == Endpoint{"203.0.113.7", 3081} && hex(nat->second) == "150200c6336409a00fcb007107080c",
          "change address+port from the fourth endpoint (discovery.py bytes)");
    auto same = discovery_answer(Bytes{20, 2, 0, 0}, {"198.51.100.9", 4000}, one);
    check(same && same->first == Endpoint{"203.0.113.7", 3078}, "NAT request without change answered from primary");
    check(!discovery_answer(Bytes{20, 2, 0, 4}, {"198.51.100.9", 4000}, one), "unknown NAT request ignored");
    check(hex(discovery_address({"1.2.3.4", 0x1234}, true)) == "43a611252633", "protected address form");

    // Lobby handshake: a client built from the same contract, then records both ways.
    {
        Bytes header;
        for (const std::uint32_t v : {200u, 200u, 0xd2u, 0xdcu, 0x10000u}) for (int i = 0; i < 4; ++i) header.push_back(static_cast<unsigned char>(v >> (8 * i)));
        const Bytes nonce = random_bytes(8);
        header.insert(header.end(), nonce.begin(), nonce.end());
        const LobbyHandshake handshake(header);
        const Bytes server_header = handshake.server_header();
        Bytes auth = lobby_frame(0x82, Bytes(40, 0x5a));
        // Client proof: CLIENTCHAL[:8] over the material that includes the auth frame itself.
        Bytes material;
        for (const std::uint32_t v : {0xd2u, 0xdcu, 0x10000u}) for (int i = 0; i < 4; ++i) material.push_back(static_cast<unsigned char>(v >> (8 * i)));
        material.insert(material.end(), nonce.begin(), nonce.end());
        material.insert(material.end(), server_header.begin(), server_header.end());
        auth.insert(auth.end(), 8, 0);
        auth[0] = static_cast<unsigned char>(auth.size() - 4);
        material.insert(material.end(), auth.begin(), auth.end() - 8);
        const Bytes prk = hmac_sha1(sha1(material), session_key);
        const Bytes challenge = hkdf_expand_sha1(prk, "CLIENTCHAL", 16);
        std::copy(challenge.begin(), challenge.begin() + 8, auth.end() - 8);
        const auto result = handshake.complete(auth, {Bytes(24, 0x22), session_key});
        check(result && result->key_index == 1 && result->done_frame == lobby_frame(0x83, Bytes(challenge.begin() + 8, challenge.end())),
              "lobby handshake finds the session key and proves the server");
        if (result) {
            LobbySession server_side(result->keys);
            // Client record with the client keys.
            const Bytes keys = result->keys;
            const Bytes client_mac(keys.begin(), keys.begin() + 20), client_aes(keys.begin() + 40, keys.begin() + 56);
            const Bytes payload = b("26080a");
            Bytes inner{3, 0, 0, 0, 0x86};
            inner.insert(inner.end(), payload.begin(), payload.end());
            inner.resize(16, 0);
            const Bytes rec_iv = random_bytes(16);
            Bytes body{0xab, 0x85, 1, 0, 0, 0};
            body.insert(body.end(), rec_iv.begin(), rec_iv.end());
            const Bytes enc = aes128_cbc_encrypt(client_aes, rec_iv, inner);
            body.insert(body.end(), enc.begin(), enc.end());
            Bytes whole;
            const std::size_t size = body.size() + 8;
            for (int i = 0; i < 4; ++i) whole.push_back(static_cast<unsigned char>(size >> (8 * i)));
            whole.insert(whole.end(), body.begin(), body.end());
            const Bytes mac = hmac_sha1(client_mac, whole);
            body.insert(body.end(), mac.begin(), mac.begin() + 8);
            const auto opened = server_side.open_record(body);
            check(opened.first == 0x86 && opened.second == payload, "client record opened");
            bool replay = false;
            try { server_side.open_record(body); } catch (const ProtocolError&) { replay = true; }
            check(replay, "replayed record counter rejected");
            const Bytes sealed = server_side.seal_record(b("01"), 1);
            const Bytes server_mac(keys.begin() + 20, keys.begin() + 40);
            const Bytes expected_mac = hmac_sha1(server_mac, Bytes(sealed.begin(), sealed.end() - 8));
            check(std::equal(expected_mac.begin(), expected_mac.begin() + 8, sealed.end() - 8), "server record MAC");
        }
    }

    // Matchmaking: the scenario of tools/online/test_room_search.py and test_lobby_sync.py.
    {
        MatchmakingStore store;
        const std::string first = room_token("same"), other = room_token("different");
        const std::string zero67 = base64_encode(Bytes(67, 0));
        const std::string info = R"({"listen_server":{"local_address":")" + zero67 + R"(","security_id":")" + base64_encode(Bytes(8, 0)) +
                                 R"(","security_key":")" + base64_encode(Bytes(16, 0)) + R"(","nat":1},"dedicated_server":{"build_name":"synthetic"}})";
        std::map<std::uint64_t, std::vector<Param>> searches;
        for (const auto& [account, selector] : std::vector<std::pair<std::uint64_t, std::string>>{{42, first}, {43, first}, {44, other}}) {
            store.advertise({param(typed::STRING, Json::of(info))}, account, selector);
            const std::string document = R"({"members":[)" + std::to_string(account) + R"(],"tokens":[)" + std::to_string(store.player_token(account)) +
                                         R"(],"lobby_slots":{"min":2,"max":8},"dedicated_server":{"request_level":"not_needed"},"team_balance":{"num_teams":0},)"
                                         R"("ruleset_payload":{"filter":{"matchmaking_version":200,"playlist_id":0}},"player_id_to_slots":{")" +
                                         std::to_string(account) + R"(":0},"performance":{"game_type":1}})";
            searches[account] = {param(typed::STRING, Json::of(document))};
        }
        store.start_search(searches[42], 42, first);
        store.start_search(searches[44], 44, other);
        const std::map<std::uint64_t, std::string> available{{42, first}, {43, first}, {44, other}};
        check(store.match_ready(available).empty(), "one player per room: no lobby");
        bool wrong_room = false;
        try { store.start_search(searches[43], 43, other); } catch (const std::exception&) { wrong_room = true; }
        check(wrong_room, "search in another room than the advertisement rejected");
        const std::uint64_t id = store.start_search(searches[43], 43, first);
        check(id > 0 && id < (1ull << 52) && store.start_search(searches[43], 43, first) == id, "repeated search keeps its ID");
        check(store.match_ready({{42, other}, {43, first}, {44, other}}).empty(), "player connected in another room not paired");
        const auto plans = store.match_ready(available);
        check(plans.size() == 1 && plans[0].players.size() == 2 && plans[0].players[0].account == 42 && plans[0].players[1].account == 43 &&
              plans[0].host == 42, "same password pairs 42 and 43, 42 hosts");
        check(store.match_ready(available).empty(), "paired searches not paired again");
        if (plans.size() == 1) {
            const auto [host, backend] = lobby_documents(plans[0], parse_json(info), 100);
            check(dump_json(host).rfind(R"({"update_time":100,"update_id":1,"lobby_open":true,"lobby_open_for_pres_join":false,"player_state":{"42":0,"43":0},)", 0) == 0,
                  "host document order and values");
            store.save_documents(plans[0].lobby_id, host, backend, 1);
            Json next = host;
            next.set("attachment", Json::of(""));
            next.find("player_state")->set("43", Json::of(1));
            std::vector<Param> sync{param(typed::UINT64, Json::of(plans[0].lobby_id)), param(typed::UINT64, Json::of(1)),
                                    param(typed::STRING, Json::of(dump_json(next)))};
            bool not_host = false;
            try { store.sync_document(sync, 43, first); } catch (const std::exception&) { not_host = true; }
            check(not_host, "guest cannot update the lobby document");
            check(store.sync_document(sync, 42, first) && store.documents(plans[0].lobby_id)->update_id == 2, "host update accepted, version 2");
            check(!store.sync_document(sync, 42, first), "stale version refused");
            next.set("attachment", Json::null());
            next.set("update_id", Json::of(2));
            sync[1] = param(typed::UINT64, Json::of(2));
            sync[2] = param(typed::STRING, Json::of(dump_json(next)));
            check(store.sync_document(sync, 42, first) && store.documents(plans[0].lobby_id)->update_id == 3, "null attachment after race start");
        }
    }

    // CTR_ONLINE_MIN_PLAYERS environment variable overrides minimum players before pairing.
    {
        MatchmakingStore min_store;
        const std::string room_tok = room_token("triotest");
        const std::string min_info = R"({"listen_server":{"local_address":")" + base64_encode(Bytes(67, 0)) + R"(","security_id":")" +
                                     base64_encode(Bytes(8, 0)) + R"(","security_key":")" + base64_encode(Bytes(16, 0)) +
                                     R"(","nat":1},"dedicated_server":{"build_name":"synthetic"}})";
        std::map<std::uint64_t, std::vector<Param>> psearches;
        for (std::uint64_t a : {101ull, 102ull, 103ull}) {
            min_store.advertise({param(typed::STRING, Json::of(min_info))}, a, room_tok);
            const std::string sdoc = R"({"members":[)" + std::to_string(a) + R"(],"tokens":[)" + std::to_string(min_store.player_token(a)) +
                                     R"(],"lobby_slots":{"min":2,"max":8},"dedicated_server":{"request_level":"not_needed"},"team_balance":{"num_teams":0},)"
                                     R"("ruleset_payload":{"filter":{"matchmaking_version":200,"playlist_id":0}},"player_id_to_slots":{")" +
                                     std::to_string(a) + R"(":0},"performance":{"game_type":1}})";
            psearches[a] = {param(typed::STRING, Json::of(sdoc))};
        }
        _putenv("CTR_ONLINE_MIN_PLAYERS=3");
        min_store.start_search(psearches[101], 101, room_tok);
        min_store.start_search(psearches[102], 102, room_tok);
        const std::map<std::uint64_t, std::string> avail2{{101, room_tok}, {102, room_tok}};
        check(min_store.match_ready(avail2).empty(), "CTR_ONLINE_MIN_PLAYERS=3: 2 players do not match");
        min_store.start_search(psearches[103], 103, room_tok);
        const std::map<std::uint64_t, std::string> avail3{{101, room_tok}, {102, room_tok}, {103, room_tok}};
        const auto trio_plans = min_store.match_ready(avail3);
        check(trio_plans.size() == 1 && trio_plans[0].players.size() == 3, "CTR_ONLINE_MIN_PLAYERS=3: 3 players match together");
        _putenv("CTR_ONLINE_MIN_PLAYERS=");
    }

    // Relay (relay.hpp): tokens, bindings, forwarding within one room, addresses shown, refusals.
    {
        RelayLimits limits;
        limits.bytes_per_second = 1000;
        limits.burst = 2000;
        limits.binding_timeout = 60;
        Relay relay(limits);
        const std::string room_a = room_token("relay a"), room_b = room_token("relay b");
        const Endpoint pub1{"198.51.100.1", 3174}, pub2{"198.51.100.2", 3274}, pub3{"198.51.100.3", 3374};
        const Endpoint loc1{"192.168.1.10", 3074}, loc2{"192.168.1.20", 3074};
        const Endpoint nat1{"203.0.113.1", 40001}, nat2{"203.0.113.2", 40002}, nat3{"203.0.113.3", 40003};
        std::vector<RelayMember> members_a{{1, pub1, loc1}, {2, pub2, loc2}}, members_b{{3, pub3, std::nullopt}};
        const RelayRoster roster = [&](const std::string& room) { return room == room_a ? members_a : room == room_b ? members_b : std::vector<RelayMember>{}; };
        auto type_of = [](const Relay::Outcome& o) { return o.out.size() == 1 && o.out[0].data.size() >= 6 ? int(o.out[0].data[5]) : -1; };
        auto reason_of = [](const Relay::Outcome& o) { return o.out.size() == 1 && o.out[0].data.size() == 13 ? int(o.out[0].data[12]) : -1; };
        auto send = [](const Endpoint& to, const std::string& text) { return relay::encode(relay::SEND, to, bytes_of(text)); };

        const Bytes t1 = relay.issue(1, room_a), t2 = relay.issue(2, room_a), t3 = relay.issue(3, room_b);
        Relay::Outcome o = relay.handle(relay::hello(Bytes(16, 7)), nat1, 0, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: HELLO with an unknown token refused");
        check(relay.handle(bytes_of("not relay"), nat1, 0, roster).out.empty(), "relay: foreign datagrams get no answer");
        o = relay.handle(relay::hello(t1), nat1, 0, roster);
        check(type_of(o) == relay::WELCOME && o.out[0].to == nat1 && o.out[0].data == relay::encode(relay::WELCOME, nat1, Bytes{0, 60}) &&
              o.event == "relay_bound", "relay: HELLO binds the endpoint and reports what the server saw");
        o = relay.handle(send(pub2, "x"), nat1, 1, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::DESTINATION_OFFLINE, "relay: player without binding is offline");
        relay.handle(relay::hello(t2), nat2, 1, roster);
        relay.handle(relay::hello(t3), nat3, 1, roster);
        o = relay.handle(send(pub2, "hello host"), nat1, 2, roster);
        check(o.out.size() == 1 && o.out[0].to == nat2 && o.out[0].data == relay::encode(relay::DELIVER, pub1, bytes_of("hello host")),
              "relay: SEND to the advertised public endpoint delivered, sender shown by its public endpoint");
        o = relay.handle(send(pub1, "reply"), nat2, 2, roster);
        check(o.out.size() == 1 && o.out[0].to == nat1 && o.out[0].data == relay::encode(relay::DELIVER, pub2, bytes_of("reply")),
              "relay: reply delivered with the address the first sender used");
        o = relay.handle(send(loc2, "lan"), nat1, 3, roster);
        check(o.out.size() == 1 && o.out[0].to == nat2, "relay: advertised local endpoint resolves when unambiguous");
        o = relay.handle(send(pub1, "back"), nat2, 3, roster);
        check(o.out.size() == 1 && o.out[0].data == relay::encode(relay::DELIVER, loc2, bytes_of("back")),
              "relay: source shown is the address the receiving game used last");
        o = relay.handle(send(pub3, "other room"), nat1, 4, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::UNKNOWN_DESTINATION, "relay: player of another room unreachable");
        o = relay.handle(send(pub1, "stranger"), Endpoint{"203.0.113.9", 1}, 4, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: unregistered sender refused");
        o = relay.handle(relay::encode(relay::SEND, pub2, Bytes(relay::kMaxPayload + 1, 0)), nat1, 5, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::MALFORMED, "relay: oversized payload refused");
        o = relay.handle(relay::encode(relay::SEND, pub2, Bytes(1400, 0)), nat1, 6, roster);
        check(o.out.size() == 1 && o.out[0].to == nat2, "relay: datagram within the byte budget forwarded");
        o = relay.handle(relay::encode(relay::SEND, pub2, Bytes(1400, 0)), nat1, 6, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::RATE_LIMITED && o.event == "relay_rate_limited", "relay: byte rate limited");
        o = relay.handle(relay::encode(relay::SEND, pub2, Bytes(1000, 0)), nat1, 8, roster);
        check(o.out.size() == 1 && o.out[0].to == nat2, "relay: budget refilled over time");
        o = relay.handle(relay::hello(t1), Endpoint{"203.0.113.1", 40999}, 9, roster);
        check(type_of(o) == relay::WELCOME && o.event == "relay_bound", "relay: new NAT mapping rebinds the player");
        o = relay.handle(send(pub2, "old mapping"), nat1, 9, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: previous endpoint no longer accepted");
        members_a = {{2, pub2, loc2}};
        o = relay.handle(send(pub1, "after leaving"), nat2, 10, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::UNKNOWN_DESTINATION, "relay: player who left the room unreachable");
        o = relay.handle(send(pub2, "expired"), nat2, 100, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: binding expires without traffic");
        relay.forget(3);
        o = relay.handle(relay::hello(t3), nat3, 100, roster);
        check(type_of(o) == relay::REFUSED && reason_of(o) == relay::NOT_REGISTERED, "relay: token withdrawn when the player leaves the lobby");
        const Bytes block = b("c0a8017d020c" + std::string(108, '0') + "c0a8017dca0c01");
        check(block_endpoint(block, 0) == Endpoint{"192.168.1.125", 3074} && block_endpoint(block, 60) == Endpoint{"192.168.1.125", 3274} &&
              !block_endpoint(Bytes(67, 0), 60), "relay: endpoints read from the retail advertisement block");
    }

    // Statistics and sessions.
    {
        StatsStore stats(dir / "stats.txt");
        stats.write({param(typed::UINT32, Json::of(7)), param(typed::UINT64, Json::of(42)), param(typed::UINT8, Json::of(0)),
                     param(typed::INT64, Json::of(-3))}, 42);
        bool other_account = false;
        try {
            stats.write({param(typed::UINT32, Json::of(7)), param(typed::UINT64, Json::of(43)), param(typed::UINT8, Json::of(0)),
                         param(typed::INT64, Json::of(1))}, 42);
        } catch (const std::exception&) { other_account = true; }
        StatsStore reloaded(dir / "stats.txt");
        check(other_account && reloaded.size() == 1, "statistics persisted, other account refused");
        SessionStore sessions;
        const std::string meta = R"({"sessionPrivacy":"private","sessionMaxUser":8,"sessionName":"x","availablePlatforms":["PS4"]})";
        const std::string body = "--B\r\nContent-Type: application/json\r\nContent-Description: session-request\r\nContent-Length: " +
                                 std::to_string(meta.size()) + "\r\n\r\n" + meta + "\r\n--B\r\nContent-Type: application/octet-stream\r\n"
                                 "Content-Description: changeable-session-data\r\nContent-Length: 2\r\n\r\nab";
        const Json created = sessions.create(42, "multipart/mixed; boundary=B", bytes_of(body));
        check(created.find("sessionId") && created.at("sessionId").text.size() == 32, "retail session multipart without final delimiter");
        bool bad = false;
        try { sessions.create(42, "multipart/mixed; boundary=B", bytes_of(body + "x")); } catch (const std::exception&) { bad = true; }
        check(bad, "data after the final part refused");
    }

    // The launcher's hosting: server started in process on the default ports, then stopped.
    {
        const std::filesystem::path root = dir / "root";
        Host host;
        const HostStatus status = host.start(root, "192.0.2.10", false);
        std::string body;
        const int config = http_get("127.0.0.1", 8780, "/ctr/v1/config", body);
        check(status.running && status.address_source == "manual" && config == 200 &&
              body == R"({"discovery_port": 3078, "public_host": "192.0.2.10"})", "hosted server answers with the public address");
        std::string key_body;
        std::ifstream key_file(status.traffic_key, std::ios::binary);
        const std::string key_on_disk{std::istreambuf_iterator<char>(key_file), {}};
        check(http_get("127.0.0.1", 8780, "/ctr/v1/traffic-key", key_body) == 200 && key_body.size() == 0x126 && key_body == key_on_disk,
              "traffic key served equals the key file the host's game uses");
        Host second;
        const HostStatus busy = second.start(root, "192.0.2.10", false);
        check(!busy.running && !busy.error.empty(), "second host on the same ports refused with a reason");
        host.stop();
        check(http_get("127.0.0.1", 8780, "/ctr/v1/config", body, 1000) == 0, "ports released when hosting stops");
        const HostStatus again = host.start(root, "192.0.2.10", false);
        check(again.running, "hosting starts again after a stop");
        host.stop();
    }
    check(resolve_ipv4("127.0.0.1") == "127.0.0.1" && resolve_ipv4("localhost") == "127.0.0.1" && resolve_ipv4("no such host.invalid").empty(),
          "server names resolved to IPv4");
    check(ipv4_scope("192.168.1.5") == "private" && ipv4_scope("100.70.1.1") == "cgnat" && ipv4_scope("203.0.113.9") == "public",
          "address scope classification");
    std::filesystem::remove_all(dir);
    std::printf("{\"scope\": \"online_server_test\", \"passed\": %s, \"failures\": %d}\n", failures ? "false" : "true", failures);
    return failures ? 1 : 0;
}
