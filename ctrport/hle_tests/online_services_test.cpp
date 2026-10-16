// SPDX-License-Identifier: GPL-2.0-or-later
// Unit test of the R8 online services in ctr_hle.dll (net_socket.cpp, net_ctl_online.cpp,
// np_online.cpp, online_config.cpp). Calls each exported guest function through its SysV
// entry point, exactly as the guest does, on loopback only.
// Run from the repository root: build/CTRPort-gpu/ctr_hle_online_test.exe
// --relay and --relay-block check the relay transport (online_relay.h) against a fake project
// server (relay-session over HTTP, relay over UDP) in a process of their own, because the
// runtime reads its switches once.
// Exit code 0 and a final {"scope":"online_services_test","passed":true} line on success.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#define SYSV __attribute__((sysv_abi))

namespace {
using Resolve = std::uintptr_t (*)(const char*, const char*);
Resolve resolve = nullptr;
int failures = 0;
bool common_ready = false, common_used = false;
int common_operation(int op) {
    if (op == 0) return common_ready;
    if (op == 1) { if (common_used) return 0; common_used = true; return 1; }
    if (op == 2) { common_used = false; return 0; }
    return -1;
}

template <typename F>
F bind(const char* nid, const char* library) {
    const auto address = resolve(nid, library);
    if (!address) {
        std::printf("{\"scope\":\"online_services_test\",\"missing\":\"%s %s\"}\n", library, nid);
        std::exit(2);
    }
    return reinterpret_cast<F>(address);
}

void check(bool condition, const char* what) {
    std::printf("{\"scope\":\"online_services_test\",\"check\":\"%s\",\"ok\":%s}\n", what,
                condition ? "true" : "false");
    if (!condition) {
        ++failures;
    }
}

struct OrbisSockaddrIn {
    std::uint8_t len, family;
    std::uint16_t port;
    std::uint32_t addr;
    std::uint16_t vport;
    char zero[6];
};
OrbisSockaddrIn loopback(std::uint16_t port) {
    OrbisSockaddrIn a{};
    a.len = sizeof(a);
    a.family = 2;
    a.port = htons(port);
    a.addr = htonl(INADDR_LOOPBACK);
    return a;
}
struct EpollEvent {
    std::uint32_t events, pad;
    std::uint64_t ident;
    std::uint64_t data;
};
constexpr int kErrorBase = static_cast<int>(0x80410100u);

void set_env(const char* name, const char* value) {
    _putenv_s(name, value);
    SetEnvironmentVariableA(name, value);
}
OrbisSockaddrIn guest_address(const char* ip, std::uint16_t port) {
    OrbisSockaddrIn a{};
    a.len = sizeof(a);
    a.family = 2;
    a.port = htons(port);
    inet_pton(AF_INET, ip, &a.addr);
    return a;
}
std::string bytes_text(const char* data, int size) { return std::string(data, data + (size > 0 ? size : 0)); }

// Relay transport against a fake project server. block: CTR_ONLINE_RELAY_TEST_BLOCK_DIRECT=1
// without the relay (direct peer traffic dropped both ways).
int relay_test(const char* test_mode) {
    const bool block = !std::strcmp(test_mode, "relay-block");
    const bool auto_direct = !std::strcmp(test_mode, "relay-auto-direct");
    const bool auto_fallback = !std::strcmp(test_mode, "relay-auto-fallback");
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
    auto udp = [](const char* ip) {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        inet_pton(AF_INET, ip, &a.sin_addr);
        bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a));
        DWORD timeout = 1500;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        return s;
    };
    auto port_of = [](SOCKET s) {
        sockaddr_in a{};
        int n = sizeof(a);
        getsockname(s, reinterpret_cast<sockaddr*>(&a), &n);
        return ntohs(a.sin_port);
    };
    const SOCKET relay = udp("0.0.0.0"), direct_peer = udp("127.0.0.9");
    const std::uint16_t relay_port = port_of(relay), peer_port = port_of(direct_peer);
    // Next datagram at the fake relay other than a keepalive HELLO (22 bytes, type 1).
    auto recv_relay = [&](char* out, int capacity) {
        for (;;) {
            const int got = recv(relay, out, capacity, 0);
            if (got != 22 || out[5] != 1) return got;
        }
    };

    // Fake HTTP: answers every request with the relay session and records the request heads.
    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(listener, reinterpret_cast<sockaddr*>(&local), sizeof(local));
    listen(listener, 8);
    const std::string http = std::to_string(port_of(listener));
    std::vector<std::string> requests;
    std::atomic<bool> stop{false};
    std::thread server([&] {
        while (!stop) {
            const SOCKET c = accept(listener, nullptr, nullptr);
            if (c == INVALID_SOCKET) return;
            std::string head;
            char ch;
            while (head.find("\r\n\r\n") == std::string::npos && recv(c, &ch, 1, 0) == 1) head += ch;
            requests.push_back(head);
            // /ctr/v1/config: discovery on the relay socket's port, public address 127.0.0.5.
            const std::string body = head.rfind("GET /ctr/v1/config ", 0) == 0
                                         ? "{\"discovery_port\": " + std::to_string(relay_port) + ", \"public_host\": \"127.0.0.5\"}"
                                         : "{\"token\": \"00112233445566778899aabbccddeeff\", \"port\": " +
                                               std::to_string(relay_port) + ", \"lease\": 60}";
            const std::string reply = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                                      std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            send(c, reply.data(), static_cast<int>(reply.size()), 0);
            closesocket(c);
        }
    });

    set_env("CTR_ONLINE", "1");
    set_env("CTR_ONLINE_SERVER", "127.0.0.1");
    set_env("CTR_ONLINE_ID", "RelayRacer");
    set_env("CTR_ONLINE_HTTP_PORT", http.c_str());
    set_env("CTR_ONLINE_PORT_OFFSET", "2100");
    set_env("CTR_ONLINE_ROOM_TOKEN", "abababababababababababababababababababababababababababababababab");
    if (block) {
        set_env("CTR_ONLINE_RELAY_TEST_BLOCK_DIRECT", "1");
    } else if (auto_direct) {
        set_env("CTR_ONLINE_NAT_TRANSPORT", "auto");
        set_env("CTR_ONLINE_DIRECT_PROBE_MS", "500");
    } else if (auto_fallback) {
        set_env("CTR_ONLINE_NAT_TRANSPORT", "auto");
        set_env("CTR_ONLINE_DIRECT_PROBE_MS", "50");
    } else {
        set_env("CTR_ONLINE_NAT_TRANSPORT", "relay");
    }
    HMODULE library = LoadLibraryW(L"build/CTRPort-gpu/ctr_hle.dll");
    resolve = library ? reinterpret_cast<Resolve>(GetProcAddress(library, "ctr_hle_resolve")) : nullptr;
    if (!resolve) {
        std::printf("{\"scope\":\"online_services_test\",\"error\":\"ctr_hle.dll not loaded\"}\n");
        return 2;
    }
    auto net_init = bind<int(SYSV*)()>("Nlev7Lg8k3A", "libSceNet");
    auto net_socket = bind<int(SYSV*)(const char*, int, int, int)>("Q4qBuN-c0ZM", "libSceNet");
    auto net_bind = bind<int(SYSV*)(int, const void*, std::uint32_t)>("bErx49PgxyY", "libSceNet");
    auto net_sendto = bind<int(SYSV*)(int, const void*, std::uint64_t, int, const void*, std::uint32_t)>("gvD1greCu0A", "libSceNet");
    auto net_recvfrom = bind<int(SYSV*)(int, void*, std::uint64_t, int, void*, std::uint32_t*)>("304ooNZxWDY", "libSceNet");
    auto net_setsockopt = bind<int(SYSV*)(int, int, int, const void*, std::uint32_t)>("2mKX2Spso7I", "libSceNet");
    auto net_close = bind<int(SYSV*)(int)>("45ggEzakPJQ", "libSceNet");
    net_init();
    const int game = net_socket("game", 2, 2, 0);
    const OrbisSockaddrIn any = guest_address("0.0.0.0", 3074);
    check(game > 0 && net_bind(game, &any, sizeof(any)) == 0, "relay: game socket bound on the peer port");
    sockaddr_in game_host{};
    game_host.sin_family = AF_INET;
    game_host.sin_port = htons(3074 + 2100);
    inet_pton(AF_INET, "127.0.0.1", &game_host.sin_addr);
    const OrbisSockaddrIn peer = guest_address("127.0.0.9", peer_port);
    char buffer[2048];
    sockaddr_in from{};
    int from_length = sizeof(from);

    if (block) {
        check(net_sendto(game, "lost", 4, 0, &peer, sizeof(peer)) == 4, "relay test switch: direct send reported as sent");
        check(recv(direct_peer, buffer, sizeof(buffer), 0) == SOCKET_ERROR, "relay test switch: nothing reaches the peer directly");
        sendto(direct_peer, "in", 2, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        const SOCKET discovery = udp("127.0.0.2"); // a project server address in development
        sendto(discovery, "server", 6, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        OrbisSockaddrIn source{};
        std::uint32_t source_length = sizeof(source);
        const int got = net_recvfrom(game, buffer, sizeof(buffer), 0, &source, &source_length);
        check(got == 6 && bytes_text(buffer, got) == "server" && ntohs(source.port) == port_of(discovery),
              "relay test switch: direct peer datagram dropped, server datagram delivered");
        closesocket(discovery);
    } else if (auto_direct) {
        int n = recvfrom(relay, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_length);
        check(n == 22 && !std::memcmp(buffer, "CTRR\x01\x01", 6) && ntohs(from.sin_port) == 3074 + 2100,
              "relay auto: registered with relay before sending");
        check(net_sendto(game, "direct_probe", 12, 0, &peer, sizeof(peer)) == 12, "relay auto: probe sent directly");
        n = recv(direct_peer, buffer, sizeof(buffer), 0);
        check(n == 12 && bytes_text(buffer, n) == "direct_probe", "relay auto: direct probe reached peer directly");
        sendto(direct_peer, "direct_ack", 10, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        OrbisSockaddrIn source{};
        std::uint32_t source_length = sizeof(source);
        int got = net_recvfrom(game, buffer, sizeof(buffer), 0, &source, &source_length);
        check(got == 10 && bytes_text(buffer, got) == "direct_ack" && ntohs(source.port) == peer_port,
              "relay auto: direct reply received, direct confirmed");
        check(net_sendto(game, "stream_data", 11, 0, &peer, sizeof(peer)) == 11,
              "relay auto: subsequent send after direct confirmed");
        n = recv(direct_peer, buffer, sizeof(buffer), 0);
        check(n == 11 && bytes_text(buffer, n) == "stream_data", "relay auto: subsequent send delivered directly");
        DWORD fast_to = 200;
        setsockopt(relay, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&fast_to), sizeof(fast_to));
        n = recv_relay(buffer, sizeof(buffer));
        check(n == SOCKET_ERROR, "relay auto: zero data packets sent to relay on confirmed direct path");
    } else if (auto_fallback) {
        int n = recvfrom(relay, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_length);
        check(n == 22 && !std::memcmp(buffer, "CTRR\x01\x01", 6) && ntohs(from.sin_port) == 3074 + 2100,
              "relay auto fallback: registered with relay before sending");
        check(net_sendto(game, "probe_lost", 10, 0, &peer, sizeof(peer)) == 10,
              "relay auto fallback: initial probe sent directly");
        n = recv(direct_peer, buffer, sizeof(buffer), 0);
        check(n == 10 && bytes_text(buffer, n) == "probe_lost", "relay auto fallback: direct peer drops probe (no reply)");
        Sleep(80); // exceed 50ms probe timeout
        check(net_sendto(game, "relayed_after_to", 16, 0, &peer, sizeof(peer)) == 16,
              "relay auto fallback: send after probe timeout");
        do {
            n = recvfrom(relay, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_length);
        } while (n == 22);
        check(n == 28 && !std::memcmp(buffer, "CTRR\x01\x03\x7f\x00\x00\x09", 10) &&
                  bytes_text(buffer + 12, n - 12) == "relayed_after_to",
              "relay auto fallback: packet routed via relay after timeout");
        const char welcome[] = "CTRR\x01\x02\x7f\x00\x00\x01\x14\x1e\x00\x3c";
        const char deliver[] = "CTRR\x01\x04\x7f\x00\x00\x09\x12\x34relayed_reply";
        sendto(relay, welcome, 14, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        sendto(relay, deliver, 25, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        OrbisSockaddrIn source{};
        std::uint32_t source_length = sizeof(source);
        int got = net_recvfrom(game, buffer, sizeof(buffer), 0, &source, &source_length);
        check(got == 13 && bytes_text(buffer, got) == "relayed_reply",
              "relay auto fallback: reply received through relay");
    } else {
        // The game socket registers by itself (HELLO) before the game sends anything.
        int n = recvfrom(relay, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_length);
        check(n == 22 && !std::memcmp(buffer, "CTRR\x01\x01", 6) &&
                  !std::memcmp(buffer + 6, "\x00\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff", 16) &&
                  ntohs(from.sin_port) == 3074 + 2100,
              "relay: HELLO with the token from the game socket before any game send");
        check(net_sendto(game, "race data", 9, 0, &peer, sizeof(peer)) == 9, "relay: game send reported as sent");
        do { // repeated HELLOs (no WELCOME yet) are skipped
            n = recvfrom(relay, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&from), &from_length);
        } while (n == 22);
        check(n == 21 && !std::memcmp(buffer, "CTRR\x01\x03\x7f\x00\x00\x09", 10) &&
                  (static_cast<unsigned char>(buffer[10]) << 8 | static_cast<unsigned char>(buffer[11])) == peer_port &&
                  bytes_text(buffer + 12, n - 12) == "race data",
              "relay: SEND carries the game's destination and payload");
        check(recv(direct_peer, buffer, sizeof(buffer), 0) == SOCKET_ERROR, "relay: nothing sent directly to the peer");
        check(!requests.empty() && requests[0].rfind("POST /ctr/v1/relay-session ", 0) == 0 &&
                  requests[0].find("RelayRacer") != std::string::npos &&
                  requests[0].find("abababababababababababababababababababababababababababababababab") != std::string::npos,
              "relay: session asked with identity and room token");

        const char welcome[] = "CTRR\x01\x02\x7f\x00\x00\x01\x14\x1e\x00\x3c";
        const char deliver[] = "CTRR\x01\x04\xc6\x33\x64\x07\x0c\xca" "from host";
        sendto(relay, welcome, 14, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        sendto(relay, deliver, 21, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        OrbisSockaddrIn source{};
        std::uint32_t source_length = sizeof(source);
        int got = net_recvfrom(game, buffer, sizeof(buffer), 0, &source, &source_length);
        char text[16] = {};
        inet_ntop(AF_INET, &source.addr, text, sizeof(text));
        check(got == 9 && bytes_text(buffer, got) == "from host" && !std::strcmp(text, "198.51.100.7") && ntohs(source.port) == 3274,
              "relay: WELCOME skipped, DELIVER unwrapped with the sender address from the relay");
        // A refused destination falls back to the direct path.
        char refused[13];
        std::memcpy(refused, "CTRR\x01\x05\x7f\x00\x00\x09", 10);
        refused[10] = static_cast<char>(peer_port >> 8);
        refused[11] = static_cast<char>(peer_port & 0xFF);
        refused[12] = 2; // unknown destination
        sendto(relay, refused, 13, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        sendto(relay, deliver, 21, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        got = net_recvfrom(game, buffer, sizeof(buffer), 0, &source, &source_length);
        check(got == 9, "relay: REFUSED consumed before the next delivered datagram");
        check(net_sendto(game, "direct", 6, 0, &peer, sizeof(peer)) == 6, "relay: send after refusal");
        n = recv(direct_peer, buffer, sizeof(buffer), 0);
        check(n == 6 && bytes_text(buffer, n) == "direct", "relay: refused destination reached directly");
        // Non-blocking: a control message alone leaves nothing for the game.
        const int one = 1;
        check(net_setsockopt(game, 0xFFFF, 0x1200, &one, 4) == 0, "relay: socket non-blocking");
        sendto(relay, welcome, 14, 0, reinterpret_cast<sockaddr*>(&game_host), sizeof(game_host));
        Sleep(200);
        got = net_recvfrom(game, buffer, sizeof(buffer), 0, &source, &source_length);
        check(got == (kErrorBase | 35), "relay: control message alone gives EWOULDBLOCK");
        // The server's own ports are never wrapped.
        const OrbisSockaddrIn service = guest_address("127.0.0.1", relay_port);
        check(net_sendto(game, "raw", 3, 0, &service, sizeof(service)) == 3, "relay: send to a server port");
        n = recv_relay(buffer, sizeof(buffer));
        check(n == 3 && bytes_text(buffer, n) == "raw", "relay: datagram to the server's own port sent as is");
        // Discovery to the server's port 3074 (mapped to its discovery port) fetches the
        // configuration; afterwards the server's public address is a server address too.
        const OrbisSockaddrIn discovery = guest_address("127.0.0.1", 3074);
        check(net_sendto(game, "probe", 5, 0, &discovery, sizeof(discovery)) == 5, "relay: discovery send");
        n = recv_relay(buffer, sizeof(buffer));
        check(n == 5 && bytes_text(buffer, n) == "probe", "relay: discovery sent as is to the configured discovery port");
        const OrbisSockaddrIn public_service = guest_address("127.0.0.5", relay_port);
        check(net_sendto(game, "public", 6, 0, &public_service, sizeof(public_service)) == 6, "relay: send to the public address");
        n = recv_relay(buffer, sizeof(buffer));
        check(n == 6 && bytes_text(buffer, n) == "public", "relay: server's public address with a service port not relayed");
    }
    net_close(game);
    stop = true;
    closesocket(listener);
    server.join();
    std::printf("{\"scope\":\"online_services_test\",\"mode\":\"%s\",\"passed\":%s}\n", test_mode,
                failures ? "false" : "true");
    return failures ? 1 : 0;
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && (!std::strcmp(argv[1], "--relay") ||
                     !std::strcmp(argv[1], "--relay-block") ||
                     !std::strcmp(argv[1], "--relay-auto-direct") ||
                     !std::strcmp(argv[1], "--relay-auto-fallback"))) {
        return relay_test(argv[1] + 2);
    }
    // --offline: without CTR_ONLINE no online registration is resolved, so the offline
    // runtime keeps its existing bindings; non-online ctr_hle functions still resolve.
    if (argc > 1 && !std::strcmp(argv[1], "--offline")) {
        _putenv_s("CTR_ONLINE", "");
        SetEnvironmentVariableA("CTR_ONLINE", nullptr);
        HMODULE library = LoadLibraryW(L"build/CTRPort-gpu/ctr_hle.dll");
        resolve = library ? reinterpret_cast<Resolve>(GetProcAddress(library, "ctr_hle_resolve"))
                          : nullptr;
        if (!resolve) {
            return 2;
        }
        const char* online[][2] = {{"Q4qBuN-c0ZM", "libSceNet"},
                                   {"Nlev7Lg8k3A", "libSceNet"},
                                   {"uBPlr0lbuiI", "libSceNetCtl"},
                                   {"u5oqtlIP+Fw", "libSceNetCtlForNpToolkit"},
                                   {"3Zl8BePTh9Y", "libSceNpManager"},
                                   {"p-o74CnoNzY", "libSceNpManager"},
                                   {"qAUXQ9GdWp8", "libSceNpAuth"}};
        bool none = true;
        for (const auto& entry : online) {
            none = none && !resolve(entry[0], entry[1]);
        }
        check(none, "offline: online services not resolved");
        check(resolve("8Kcp5d-q1Uo", "libSceNet") != 0, "offline: sceNetInetPton still resolved");
        std::printf("{\"scope\":\"online_services_test\",\"mode\":\"offline\",\"passed\":%s}\n",
                    failures ? "false" : "true");
        return failures ? 1 : 0;
    }
    _putenv_s("CTR_ONLINE", "1");
    _putenv_s("CTR_ONLINE_SERVER", "127.0.0.1");
    _putenv_s("CTR_ONLINE_ID", "TestRacer");
    _putenv_s("CTR_ONLINE_PORT_OFFSET", "2000");
    SetEnvironmentVariableA("CTR_ONLINE", "1");
    SetEnvironmentVariableA("CTR_ONLINE_SERVER", "127.0.0.1");
    SetEnvironmentVariableA("CTR_ONLINE_ID", "TestRacer");
    SetEnvironmentVariableA("CTR_ONLINE_PORT_OFFSET", "2000");

    HMODULE library = LoadLibraryW(L"build/CTRPort-gpu/ctr_hle.dll");
    if (!library) {
        std::printf("{\"scope\":\"online_services_test\",\"error\":\"ctr_hle.dll not loaded\"}\n");
        return 2;
    }
    resolve = reinterpret_cast<Resolve>(GetProcAddress(library, "ctr_hle_resolve"));
    if (!resolve) {
        return 2;
    }

    // --- libSceNet ---
    auto net_init = bind<int(SYSV*)()>("Nlev7Lg8k3A", "libSceNet");
    auto errno_loc = bind<int*(SYSV*)()>("HQOwnfMGipQ", "libSceNet");
    auto net_socket = bind<int(SYSV*)(const char*, int, int, int)>("Q4qBuN-c0ZM", "libSceNet");
    auto net_close = bind<int(SYSV*)(int)>("45ggEzakPJQ", "libSceNet");
    auto net_bind = bind<int(SYSV*)(int, const void*, std::uint32_t)>("bErx49PgxyY", "libSceNet");
    auto net_listen = bind<int(SYSV*)(int, int)>("kOj1HiAGE54", "libSceNet");
    auto net_accept = bind<int(SYSV*)(int, void*, std::uint32_t*)>("PIWqhn9oSxc", "libSceNet");
    auto net_connect =
        bind<int(SYSV*)(int, const void*, std::uint32_t)>("OXXX4mUk3uk", "libSceNet");
    auto net_getsockname =
        bind<int(SYSV*)(int, void*, std::uint32_t*)>("hoOAofhhRvE", "libSceNet");
    auto net_send = bind<int(SYSV*)(int, const void*, std::uint64_t, int)>("beRjXBn-z+o",
                                                                            "libSceNet");
    auto net_recv = bind<int(SYSV*)(int, void*, std::uint64_t, int)>("9wO9XrMsNhc", "libSceNet");
    auto net_sendto = bind<int(SYSV*)(int, const void*, std::uint64_t, int, const void*,
                                      std::uint32_t)>("gvD1greCu0A", "libSceNet");
    auto net_recvfrom = bind<int(SYSV*)(int, void*, std::uint64_t, int, void*, std::uint32_t*)>(
        "304ooNZxWDY", "libSceNet");
    auto net_setsockopt = bind<int(SYSV*)(int, int, int, const void*, std::uint32_t)>(
        "2mKX2Spso7I", "libSceNet");
    auto net_getsockopt =
        bind<int(SYSV*)(int, int, int, void*, std::uint32_t*)>("xphrZusl78E", "libSceNet");
    auto resolver_create = bind<int(SYSV*)(const char*, int, int)>("C4UgDHHPvdw", "libSceNet");
    auto resolver_start = bind<int(SYSV*)(int, const char*, std::uint32_t*, int, int, int)>(
        "Nd91WaWmG2w", "libSceNet");
    auto resolver_destroy = bind<int(SYSV*)(int)>("kJlYH5uMAWI", "libSceNet");
    auto epoll_create = bind<int(SYSV*)(const char*, int)>("SF47kB2MNTo", "libSceNet");
    auto epoll_control =
        bind<int(SYSV*)(int, int, int, const EpollEvent*)>("ZVw46bsasAk", "libSceNet");
    auto epoll_wait = bind<int(SYSV*)(int, EpollEvent*, int, int)>("drjIbDbA7UQ", "libSceNet");
    auto epoll_destroy = bind<int(SYSV*)(int)>("Inp1lfL+Jdw", "libSceNet");
    auto htons_ = bind<std::uint16_t(SYSV*)(std::uint16_t)>("iWQWrwiSt8A", "libSceNet");
    auto ntop = bind<const char*(SYSV*)(int, const void*, char*, std::uint32_t)>("9vA2aW+CHuA",
                                                                                  "libSceNet");

    check(net_init() == 0, "sceNetInit");
    check(htons_(0x1234) == 0x3412, "sceNetHtons");
    std::uint32_t four = htonl(0x0a000102);
    char text[16] = {};
    check(ntop(2, &four, text, sizeof(text)) == text && !std::strcmp(text, "10.0.1.2"),
          "sceNetInetNtop");

    // UDP: two sockets, non-blocking receive before data, then sendto/recvfrom with source.
    const int a = net_socket("udp_a", 2, 2, 0), b = net_socket("udp_b", 2, 2, 0);
    check(a > 0 && b > 0 && a != b, "sceNetSocket dgram");
    auto any_a = loopback(0), any_b = loopback(0);
    check(net_bind(a, &any_a, sizeof(any_a)) == 0 && net_bind(b, &any_b, sizeof(any_b)) == 0,
          "sceNetBind");
    OrbisSockaddrIn name_b{};
    std::uint32_t name_length = sizeof(name_b);
    check(net_getsockname(b, &name_b, &name_length) == 0 && name_b.family == 2 &&
              name_b.port != 0 && name_length == 16,
          "sceNetGetsockname");
    int nbio = 1;
    check(net_setsockopt(b, 0xFFFF, 0x1200, &nbio, 4) == 0, "sceNetSetsockopt SO_NBIO");
    char buffer[64] = {};
    const int would_block = net_recv(b, buffer, sizeof(buffer), 0);
    check(would_block == (kErrorBase | 35) && *errno_loc() == 35,
          "sceNetRecv EWOULDBLOCK on empty non-blocking socket");
    int got = 0;
    std::uint32_t got_length = 4;
    check(net_getsockopt(b, 0xFFFF, 0x1200, &got, &got_length) == 0 && got == 1,
          "sceNetGetsockopt SO_NBIO");
    const char payload[] = "ctr-online";
    check(net_sendto(a, payload, sizeof(payload), 0, &name_b, sizeof(name_b)) ==
              static_cast<int>(sizeof(payload)),
          "sceNetSendto loopback");

    // Epoll: the readable UDP socket is reported with its user data.
    const int ep = epoll_create("test_epoll", 0);
    EpollEvent want{1, 0, 0, 0xC0FFEEull};
    check(ep > 0 && epoll_control(ep, 1, b, &want) == 0, "sceNetEpollControl add");
    EpollEvent events[4] = {};
    const int ready = epoll_wait(ep, events, 4, 500000);
    check(ready == 1 && (events[0].events & 1) && events[0].ident == static_cast<unsigned>(b) &&
              events[0].data == 0xC0FFEEull,
          "sceNetEpollWait readable");
    OrbisSockaddrIn from{};
    std::uint32_t from_length = sizeof(from);
    const int received = net_recvfrom(b, buffer, sizeof(buffer), 0, &from, &from_length);
    check(received == static_cast<int>(sizeof(payload)) && !std::memcmp(buffer, payload, received) &&
              from.family == 2 && from.addr == htonl(INADDR_LOOPBACK),
          "sceNetRecvfrom data and source");
    check(epoll_wait(ep, events, 4, 0) == 0, "sceNetEpollWait empty after read");
    check(epoll_control(ep, 3, b, nullptr) == 0, "sceNetEpollControl del");

    // Policy: sendto and connect towards a public address are refused, nothing is sent.
    OrbisSockaddrIn external{};
    external.len = 16;
    external.family = 2;
    external.port = htons(3074);
    InetPtonA(AF_INET, "8.8.8.8", &external.addr);
    check(net_sendto(a, payload, sizeof(payload), 0, &external, sizeof(external)) ==
              (kErrorBase | 65),
          "sceNetSendto external address blocked");

    // TCP: listen, connect, accept, send, receive.
    const int server = net_socket("tcp_server", 2, 1, 0);
    auto any_server = loopback(0);
    check(net_bind(server, &any_server, sizeof(any_server)) == 0 && net_listen(server, 4) == 0,
          "sceNetListen");
    OrbisSockaddrIn server_name{};
    std::uint32_t server_name_length = sizeof(server_name);
    net_getsockname(server, &server_name, &server_name_length);
    const int client = net_socket("tcp_client", 2, 1, 0);
    check(net_connect(client, &external, sizeof(external)) == (kErrorBase | 65),
          "sceNetConnect external address blocked");
    check(net_connect(client, &server_name, sizeof(server_name)) == 0, "sceNetConnect loopback");
    OrbisSockaddrIn peer{};
    std::uint32_t peer_length = sizeof(peer);
    const int accepted = net_accept(server, &peer, &peer_length);
    check(accepted > 0 && peer.family == 2, "sceNetAccept");
    check(net_send(client, payload, sizeof(payload), 0) == static_cast<int>(sizeof(payload)),
          "sceNetSend");
    std::memset(buffer, 0, sizeof(buffer));
    check(net_recv(accepted, buffer, sizeof(payload), 0x40) == static_cast<int>(sizeof(payload)) &&
              !std::strcmp(buffer, payload),
          "sceNetRecv WAITALL");
    int type = 0;
    std::uint32_t type_length = 4;
    check(net_getsockopt(client, 0xFFFF, 0x1008, &type, &type_length) == 0 && type == 1,
          "sceNetGetsockopt SO_TYPE");
    const int unknown_option = 0x7777;
    check(net_setsockopt(client, 0xFFFF, unknown_option, &nbio, 4) == (kErrorBase | 42),
          "sceNetSetsockopt unknown option fails");
    for (const int s : {a, b, server, client, accepted}) {
        net_close(s);
    }
    check(net_close(a) == (kErrorBase | 9), "sceNetSocketClose twice EBADF");

    // Resolver policy.
    const int resolver = resolver_create("test_resolver", 0, 0);
    std::uint32_t resolved = 0;
    check(resolver_start(resolver, "lavender-ps4-auth3.live.demonware.net", &resolved, 0, 0, 0) ==
                  0 &&
              resolved == htonl(INADDR_LOOPBACK),
          "resolver demonware -> project server");
    resolved = 0;
    check(resolver_start(resolver, "STUN.EU.DEMONWARE.NET", &resolved, 0, 0, 0) == 0 &&
              resolved == htonl(INADDR_LOOPBACK),
          "resolver case-insensitive demonware");
    check(resolver_start(resolver, "auth.np.ac.playstation.net", &resolved, 0, 0, 0) ==
              (kErrorBase | 230),
          "resolver Sony host blocked");
    check(resolver_start(resolver, "www.activision.com", &resolved, 0, 0, 0) == (kErrorBase | 230),
          "resolver Activision host blocked");
    check(resolver_start(resolver, "evildemonware.net", &resolved, 0, 0, 0) == (kErrorBase | 230),
          "resolver suffix must be a domain boundary");
    // Async resolution completes through epoll with EPOLLDESCID.
    const int ep2 = epoll_create("resolver_epoll", 0);
    EpollEvent resolver_event{0x10000, 0, 0, 77};
    resolved = 0;
    check(epoll_control(ep2, 1, resolver, &resolver_event) == 0 &&
              resolver_start(resolver, "lavender-ps4-lobby.live.demonware.net", &resolved, 0, 0,
                             1) == 0,
          "resolver async start");
    check(epoll_wait(ep2, events, 4, 100000) == 1 && events[0].events == 0x10000 &&
              events[0].data == 77 && resolved == htonl(INADDR_LOOPBACK),
          "resolver async completion via epoll");
    check(resolver_destroy(resolver) == 0 && epoll_destroy(ep) == 0 && epoll_destroy(ep2) == 0,
          "resolver and epoll destroy");

    // --- libSceNetCtl ---
    auto netctl_state = bind<int(SYSV*)(int*)>("uBPlr0lbuiI", "libSceNetCtl");
    auto netctl_info = bind<int(SYSV*)(int, void*)>("obuxdTiwkF8", "libSceNetCtl");
    int state = -1;
    check(netctl_state(&state) == 0 && state == 3, "sceNetCtlGetState IPOBTAINED");
    char info[256] = {};
    // With the project server on loopback the console reports the host's LAN address (the
    // default-route source), loopback only on a machine without any network route.
    in_addr reported{};
    check(netctl_info(14, info) == 0 && InetPtonA(AF_INET, info, &reported) == 1 &&
              reported.s_addr != 0,
          "sceNetCtlGetInfo IP_ADDRESS (LAN address with a loopback server)");
    check(netctl_info(6, info) == static_cast<int>(0x80412109u), "sceNetCtlGetInfo SSID not avail");

    // --- libSceNpManager / libSceNpAuth ---
    auto np_state = bind<int(SYSV*)(int, int*)>("eQH7nWPcAgc", "libSceNpManager");
    auto np_online_id = bind<int(SYSV*)(int, char*)>("XDncXQIJUSk", "libSceNpManager");
    auto np_account = bind<int(SYSV*)(int, std::uint64_t*)>("rbknaUjpqWo", "libSceNpManager");
    auto np_user_by_account =
        bind<int(SYSV*)(std::uint64_t, int*)>("VgYczPGB5ss", "libSceNpManager");
    auto np_create_async = bind<int(SYSV*)(const void*)>("eiqMCt9UshI", "libSceNpManager");
    auto np_availability = bind<int(SYSV*)(int, int)>("8Z2Jc5GvGDI", "libSceNpManager");
    auto np_poll = bind<int(SYSV*)(int, int*)>("uqcPJLWL08M", "libSceNpManager");
    auto np_delete = bind<int(SYSV*)(int)>("S7QTn72PrDw", "libSceNpManager");
    auto register_toolkit =
        bind<int(SYSV*)(void(SYSV*)(int, int, void*), void*)>("0c7HbXRKUt4",
                                                              "libSceNpManagerForToolkit");
    auto check_for_lib = bind<int(SYSV*)()>("JELHf4xPufo", "libSceNpManagerForToolkit");
    auto auth_create = bind<int(SYSV*)()>("6bwFkosYRQg", "libSceNpAuth");
    auto auth_code = bind<int(SYSV*)(int, const void*, char*, int*)>("qAUXQ9GdWp8", "libSceNpAuth");

    check(np_state(1, &state) == 0 && state == 2, "sceNpGetState SignedIn");
    char online_id[20] = {};
    check(np_online_id(1, online_id) == 0 && !std::strcmp(online_id, "TestRacer"),
          "sceNpGetOnlineId local identity");
    std::uint64_t account = 0;
    int user = 0;
    check(np_account(1, &account) == 0 && account != 0 &&
              np_user_by_account(account, &user) == 0 && user == 1,
          "sceNpGetAccountIdA round trip");
    struct {
        std::uint64_t size, mask;
        std::int32_t priority;
        std::uint8_t pad[4];
    } async_parameter{24, 0, 0, {}};
    const int request = np_create_async(&async_parameter);
    int result = -1;
    check(request == 0x20000001 && np_poll(request, &result) == static_cast<int>(0x80550015u),
          "sceNpPollAsync before completion INVALID_ID");
    check(np_availability(request, 1) == 0 && np_poll(request, &result) == 0 && result == 0,
          "sceNpCheckNpAvailabilityA async OK");
    check(np_delete(request) == 0, "sceNpDeleteRequest");
    static int toolkit_events = 0;
    static int toolkit_state = 0;
    register_toolkit(
        [](int, int s, void*) SYSV {
            ++toolkit_events;
            toolkit_state = s;
        },
        nullptr);
    check_for_lib();
    check_for_lib();
    check(toolkit_events == 1 && toolkit_state == 2, "toolkit SignedIn event delivered once");
    const char client_id[129] = "test-client";
    struct {
        std::uint64_t size;
        std::int32_t user;
        std::uint8_t pad[4];
        const void* client;
        const char* scope;
    } code_parameter{32, 1, {}, client_id, "psn:s2s"};
    char code[136] = {};
    int issuer = 0;
    const int auth_request = auth_create();
    check(auth_code(auth_request, &code_parameter, code, &issuer) == 0 &&
              !std::strcmp(code, "ctrlocal:TestRacer"),
          "sceNpAuthGetAuthorizationCodeA local token");
    {
        auto id_token = bind<int(SYSV*)(int, const void*, char*)>("CocbHVIKPE8", "libSceNpAuth");
        const char secret[264] = "test-secret";
        struct {
            std::uint64_t size;
            std::int32_t user;
            std::uint8_t pad[4];
            const void* client;
            const void* secret;
            const char* scope;
        } token_parameter{40, 1, {}, client_id, secret, "openid,openid:duid"};
        static char token[4104] = {};
        const int token_request = auth_create();
        const bool issued = id_token(token_request, &token_parameter, token) == 0;
        const std::string text = token;
        const auto first = text.find('.'), second = text.find('.', first + 1);
        std::string payload = second == std::string::npos ? "" : text.substr(first + 1, second - first - 1);
        DWORD decoded_size = 0;
        std::string decoded;
        if (CryptStringToBinaryA(payload.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &decoded_size, nullptr, nullptr)) {
            decoded.resize(decoded_size);
            CryptStringToBinaryA(payload.c_str(), 0, CRYPT_STRING_BASE64,
                                 reinterpret_cast<BYTE*>(decoded.data()), &decoded_size, nullptr, nullptr);
        }
        const auto duid = decoded.find("\"duid\":\"");
        check(issued && payload.find('=') == std::string::npos &&
                  decoded.find("\"env_iss_id\":256") != std::string::npos &&
                  duid != std::string::npos && decoded.find('"', duid + 8) == duid + 8 + 64 &&
                  decoded.find("\"online_id\":\"TestRacer\"") != std::string::npos,
              "sceNpAuthGetIdTokenA JWT-shaped local token");
    }

    // --- libSceNpWebApi against an in-process HTTP server on loopback ---
    {
        WSADATA data{};
        WSAStartup(MAKEWORD(2, 2), &data);
        const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(listener, reinterpret_cast<sockaddr*>(&local), sizeof(local));
        listen(listener, 4);
        int local_length = sizeof(local);
        getsockname(listener, reinterpret_cast<sockaddr*>(&local), &local_length);
        const std::string port = std::to_string(ntohs(local.sin_port));
        _putenv_s("CTR_ONLINE_WEBAPI_PORT", port.c_str());
        static std::string seen_requests;
        std::thread server([listener] {
            const char* answers[] = {
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nX-Test: yes\r\n"
                "Content-Length: 17\r\n\r\n{\"profile\":\"ok\"}\n",
                "HTTP/1.1 404 Not Found\r\nContent-Length: 30\r\n\r\n{\"error\":{\"code\":2105858}}    "};
            for (const char* answer : answers) {
                const SOCKET client = accept(listener, nullptr, nullptr);
                std::string request;
                char buffer[2048];
                while (request.find("\r\n\r\n") == std::string::npos) {
                    const int n = recv(client, buffer, sizeof(buffer), 0);
                    if (n <= 0) break;
                    request.append(buffer, n);
                }
                const auto length_at = request.find("Content-Length: ");
                const std::size_t body_length =
                    length_at == std::string::npos ? 0 : std::atoi(request.c_str() + length_at + 16);
                while (request.size() < request.find("\r\n\r\n") + 4 + body_length) {
                    const int n = recv(client, buffer, sizeof(buffer), 0);
                    if (n <= 0) break;
                    request.append(buffer, n);
                }
                seen_requests += request + "\n---\n";
                send(client, answer, static_cast<int>(std::strlen(answer)), 0);
                closesocket(client);
            }
        });
        auto web_init = bind<int(SYSV*)(int, std::uint64_t)>("G3AnLNdRBjE", "libSceNpWebApi");
        auto web_context = bind<int(SYSV*)(int, int)>("zk6c65xoyO0", "libSceNpWebApi");
        auto web_request =
            bind<int(SYSV*)(int, const char*, const char*, int, const void*, std::int64_t*)>(
                "rdgs5Z1MyFw", "libSceNpWebApi");
        auto web_header =
            bind<int(SYSV*)(std::int64_t, const char*, const char*)>("joRjtRXTFoc", "libSceNpWebApi");
        auto web_send = bind<int(SYSV*)(std::int64_t, const void*, std::uint64_t, void*)>(
            "KjNeZ-29ysQ", "libSceNpWebApi");
        auto web_read =
            bind<int(SYSV*)(std::int64_t, void*, std::uint64_t)>("CQtPRSF6Ds8", "libSceNpWebApi");
        auto web_header_value = bind<int(SYSV*)(std::int64_t, const char*, char*, std::uint64_t)>(
            "VwJ5L0Higg0", "libSceNpWebApi");
        auto web_delete = bind<int(SYSV*)(std::int64_t)>("noQgleu+KLE", "libSceNpWebApi");
        struct {
            std::int32_t status;
            char* error_object;
            std::uint64_t error_size, data_size;
        } information{};
        char error_object[64] = {};
        const int library_context = web_init(1, 0x10000);
        const int user_context = web_context(library_context, 1);
        check(library_context > 0 && user_context == (library_context << 16 | 1),
              "sceNpWebApiCreateContextA id");
        std::int64_t request = 0;
        check(web_request(user_context, "userProfile", "/v1/users/me/profile2", 0, nullptr,
                          &request) == 0 &&
                  (request >> 32) == user_context,
              "sceNpWebApiCreateRequest id");
        web_header(request, "Accept", "application/json");
        check(web_send(request, nullptr, 0, &information) == 0 && information.status == 200,
              "sceNpWebApiSendRequest2 200 from project server");
        char body[64] = {};
        check(web_read(request, body, sizeof(body)) == 17 && !std::strcmp(body, "{\"profile\":\"ok\"}\n"),
              "sceNpWebApiReadData body");
        char value[8] = {};
        check(web_header_value(request, "x-test", value, sizeof(value)) == 0 &&
                  !std::strcmp(value, "yes"),
              "sceNpWebApiGetHttpResponseHeaderValue");
        check(web_delete(request) == 0, "sceNpWebApiDeleteRequest");
        std::int64_t missing = 0;
        web_request(user_context, "sessionInvitation", "/v1/users/me/invitations", 1, nullptr,
                    &missing);
        information = {};
        information.error_object = error_object;
        information.error_size = sizeof(error_object);
        const char post_body[] = "{\"x\":1}";
        check(web_send(missing, post_body, 7, &information) == static_cast<int>(0x82000000u | 2105858) &&
                  information.status == 404 && std::strstr(error_object, "2105858"),
              "sceNpWebApiSendRequest2 error object code");
        server.join();
        check(seen_requests.find("GET /webapi/userProfile/v1/users/me/profile2 HTTP/1.1") !=
                      std::string::npos &&
                  seen_requests.find("Accept: application/json") != std::string::npos &&
                  seen_requests.find("X-Ctr-Online-Id: TestRacer") != std::string::npos &&
                  seen_requests.find("POST /webapi/sessionInvitation/v1/users/me/invitations") !=
                      std::string::npos &&
                  seen_requests.find("{\"x\":1}") != std::string::npos,
              "web API requests reach the project server");
        closesocket(listener);
    }

    // --- libSceHttp: an https Demonware URL goes in plain HTTP to the project server ---
    {
        const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(listener, reinterpret_cast<sockaddr*>(&local), sizeof(local));
        listen(listener, 4);
        int local_length = sizeof(local);
        getsockname(listener, reinterpret_cast<sockaddr*>(&local), &local_length);
        const std::string port = std::to_string(ntohs(local.sin_port));
        _putenv_s("CTR_ONLINE_HTTP_PORT", port.c_str());
        static std::string seen;
        std::thread server([listener] {
            const SOCKET client = accept(listener, nullptr, nullptr);
            char buffer[4096];
            while (seen.find("hello") == std::string::npos) {
                const int n = recv(client, buffer, sizeof(buffer), 0);
                if (n <= 0) break;
                seen.append(buffer, n);
            }
            const char answer[] = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nX-Dw: v1\r\n\r\nworld";
            send(client, answer, sizeof(answer) - 1, 0);
            closesocket(client);
        });
        auto http_init = bind<int(SYSV*)(int, int, std::uint64_t)>("A9cVMUtEp4Y", "libSceHttp");
        auto http_template =
            bind<int(SYSV*)(int, const char*, int, int)>("0gYjPTR-6cY", "libSceHttp");
        auto http_connection =
            bind<int(SYSV*)(int, const char*, bool)>("qgxDBjorUxs", "libSceHttp");
        auto http_request2 = bind<int(SYSV*)(int, const char*, const char*, std::uint64_t)>(
            "Cnp77podkCU", "libSceHttp");
        auto http_header =
            bind<int(SYSV*)(int, const char*, const char*, int)>("EY28T2bkN7k", "libSceHttp");
        auto http_send =
            bind<int(SYSV*)(int, const void*, std::uint64_t)>("1e2BNwI-XzE", "libSceHttp");
        auto http_status = bind<int(SYSV*)(int, int*)>("0a2TBNfE3BU", "libSceHttp");
        auto http_length =
            bind<int(SYSV*)(int, int*, std::uint64_t*)>("yuO2H2Uvnos", "libSceHttp");
        auto http_read = bind<int(SYSV*)(int, void*, std::uint64_t)>("P5pdoykPYTk", "libSceHttp");
        auto http_all_headers =
            bind<int(SYSV*)(int, char**, std::uint64_t*)>("aCYPMSUIaP8", "libSceHttp");
        auto http_parse = bind<int(SYSV*)(const char*, std::uint64_t, const char*, const char**,
                                          std::uint64_t*)>("hPTXo3bICzI", "libSceHttp");
        auto http_escape = bind<int(SYSV*)(char*, std::uint64_t*, std::uint64_t, const char*)>(
            "YuOW3dDAKYc", "libSceHttp");

        const int context = http_init(0, 0, 0x10000);
        const int tmpl = http_template(context, "CTR-Test/1.0", 2, 0);
        http_header(tmpl, "X-From-Template", "t", 1);
        const int connection =
            http_connection(tmpl, "https://umbrella.live.demonware.net/v1.0/auth", true);
        const int request =
            http_request2(connection, "POST", "https://umbrella.live.demonware.net/v1.0/auth/x", 5);
        http_header(request, "X-From-Request", "r", 0);
        int status = 0;
        check(context > 0 && tmpl > 0 && connection > 0 && request > 0 &&
                  http_status(request, &status) == static_cast<int>(0x80431065u),
              "sceHttp objects, status before send");
        check(http_send(request, "hello", 5) == 0 && http_status(request, &status) == 0 &&
                  status == 200,
              "sceHttpSendRequest to project server");
        int length_result = -1;
        std::uint64_t length = 0;
        char data[16] = {};
        check(http_length(request, &length_result, &length) == 0 && length_result == 0 &&
                  length == 5 && http_read(request, data, sizeof(data)) == 5 &&
                  !std::strcmp(data, "world") && http_read(request, data, sizeof(data)) == 0,
              "sceHttp content length and read");
        char* block = nullptr;
        std::uint64_t block_size = 0;
        const char* value = nullptr;
        std::uint64_t value_length = 0;
        check(http_all_headers(request, &block, &block_size) == 0 && block &&
                  http_parse(block, block_size, "x-dw", &value, &value_length) > 0 &&
                  value_length == 2 && !std::strncmp(value, "v1", 2) &&
                  http_parse(block, block_size, "x-none", &value, &value_length) ==
                      static_cast<int>(0x80432025u),
              "sceHttpGetAllResponseHeaders and ParseResponseHeader");
        server.join();
        check(seen.find("POST /v1.0/auth/x HTTP/1.1") != std::string::npos &&
                  seen.find("Host: umbrella.live.demonware.net") != std::string::npos &&
                  seen.find("X-Ctr-Original-Url: https://umbrella.live.demonware.net/v1.0/auth/x") !=
                      std::string::npos &&
                  seen.find("User-Agent: CTR-Test/1.0") != std::string::npos &&
                  seen.find("X-From-Template: t") != std::string::npos &&
                  seen.find("X-From-Request: r") != std::string::npos,
              "sceHttp request reaches project server with inherited headers");
        const int sony_connection = http_connection(tmpl, "https://auth.api.playstation.com/", true);
        const int sony = http_request2(sony_connection, "GET", "https://auth.api.playstation.com/", 0);
        check(http_send(sony, nullptr, 0) == static_cast<int>(0x80436007u),
              "sceHttpSendRequest Sony host blocked");
        char escaped[32] = {};
        std::uint64_t needed = 0;
        check(http_escape(escaped, &needed, sizeof(escaped), "a b/c") == 0 &&
                  !std::strcmp(escaped, "a%20b%2Fc") && needed == 10,
              "sceHttpUriEscape");
        closesocket(listener);
    }

    {
        auto copy = bind<void*(SYSV*)(void*,const void*,std::uint64_t)>("Q3VBxCXhUHs", "libSceLibcInternal");
        unsigned char source[4050]{}, target[4050]{};
        for (unsigned i=0;i<4050;++i)source[i]=static_cast<unsigned char>(i*17);
        target[0]=0xa5;target[4049]=0x5a;
        check(copy(target+1,source+1,4048)==target+1 && !std::memcmp(target+1,source+1,4048) &&
              target[0]==0xa5 && target[4049]==0x5a, "libc memcpy unaligned retail-size copy and boundaries");
        check(copy(target,source,0)==target && target[0]==0xa5, "libc memcpy zero length preserves destination");
        auto append = bind<char*(SYSV*)(char*,const char*,std::uint64_t)>("kHg45qPC6f0", "libSceLibcInternal");
        char text[10] = "ab"; text[9] = 'Z';
        check(append(text,"cdef",2)==text && !std::strcmp(text,"abcd") && text[9]=='Z',
              "libc strncat bounded append returns destination and terminates");
        check(append(text,"e",4)==text && !std::strcmp(text,"abcde") && append(text,"xyz",0)==text &&
              !std::strcmp(text,"abcde"), "libc strncat source terminator and zero count");
    }
    {
        auto socket_fn = bind<int(SYSV*)(const char*,int,int,int)>("Q4qBuN-c0ZM", "libSceNet");
        auto bind_fn = bind<int(SYSV*)(int,const void*,std::uint32_t)>("bErx49PgxyY", "libSceNet");
        auto name_fn = bind<int(SYSV*)(int,void*,std::uint32_t*)>("hoOAofhhRvE", "libSceNet");
        auto close_fn = bind<int(SYSV*)(int)>("45ggEzakPJQ", "libSceNet");
        const int fd = socket_fn("instance-port", 2, 2, 0);
        auto local = loopback(3074);
        check(bind_fn(fd, &local, sizeof(local)) == 0, "instance UDP bind remaps 3074");
        std::uint32_t size = sizeof(local);
        check(name_fn(fd, &local, &size) == 0 && ntohs(local.port) == 5074,
              "instance getsockname exposes reachable remapped port");
        SOCKET peer = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in target{}; target.sin_family = AF_INET; target.sin_addr.s_addr = local.addr; target.sin_port = local.port;
        sendto(peer, "instance", 8, 0, reinterpret_cast<sockaddr*>(&target), sizeof(target));
        auto recv_fn = bind<int(SYSV*)(int,void*,std::uint64_t,int,void*,std::uint32_t*)>("304ooNZxWDY", "libSceNet");
        char data[16]{}; OrbisSockaddrIn from{}; size = sizeof(from);
        check(recv_fn(fd, data, sizeof(data), 0, &from, &size) == 8 && !std::strcmp(data, "instance"),
              "instance recvfrom reaches remapped bind");
        auto send_fn = bind<int(SYSV*)(int,const void*,std::uint64_t,int,const void*,std::uint32_t)>("gvD1greCu0A", "libSceNet");
        check(send_fn(fd, "reply", 5, 0, &from, sizeof(from)) == 5,
              "instance sendto preserves peer destination port");
        char response[16]{}; int target_size = sizeof(target);
        DWORD timeout = 2000; setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        check(recvfrom(peer, response, sizeof(response), 0, reinterpret_cast<sockaddr*>(&target), &target_size) == 5 && ntohs(target.sin_port) == 5074,
              "instance reply has actual mapped source port");
        closesocket(peer); close_fn(fd);
    }
    {
        using Call = int (SYSV*)();
        auto init = bind<Call>("Lg+NCE6pTwQ", "libSceNpProfileDialog");
        auto state = bind<Call>("3BqoiFOjSsk", "libSceNpProfileDialog");
        auto update = bind<Call>("haVZE9FgKqE", "libSceNpProfileDialog");
        auto term = bind<Call>("0Sp9vJcB1-w", "libSceNpProfileDialog");
        auto load = reinterpret_cast<int(*)(int(*)(int))>(GetProcAddress(library, "ctr_hle_profile_load"));
        check(load && load(common_operation) == 0, "profile lifecycle module loads");
        check(init() == static_cast<int>(0x80B80001u), "profile rejects absent CommonDialog initialization");
        common_ready = true; common_used = true;
        check(init() == static_cast<int>(0x80B80008u), "profile respects another dialog owner");
        common_used = false;
        check(init() == 0 && common_used && state() == 1 && update() == 1, "profile claims CommonDialog and keeps initialized status");
        check(init() == static_cast<int>(0x80B80004u), "profile rejects double initialization");
        check(term() == 0 && !common_used && state() == 0, "profile termination releases CommonDialog");
        check(term() == static_cast<int>(0x80B80003u), "profile rejects termination before initialization");
        check(!resolve("uj9Cz7Tk0cc", "libSceNpProfileDialog"), "unimplemented profile UI remains missing");
        auto invitation_init = bind<Call>("XvA5KS56wcs", "libSceInvitationDialog");
        auto invitation_term = bind<Call>("B6HVJtDYxEE", "libSceInvitationDialog");
        auto invitation_load = reinterpret_cast<int(*)(int(*)(int))>(GetProcAddress(library, "ctr_hle_invitation_load"));
        check(invitation_load && invitation_load(common_operation)==0, "invitation lifecycle module loads");
        check(init()==0 && invitation_init()==static_cast<int>(0x80B80008u), "profile owner excludes invitation initialization");
        check(term()==0 && invitation_init()==0 && init()==static_cast<int>(0x80B80008u), "invitation owner excludes profile initialization");
        check(invitation_term()==0 && !common_used, "invitation termination releases shared owner");
    }

    // --- Internet play: peers confirmed by the project server, discovery port mapping ---
    {
        const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in local{};
        local.sin_family = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(listener, reinterpret_cast<sockaddr*>(&local), sizeof(local));
        listen(listener, 8);
        int local_length = sizeof(local);
        getsockname(listener, reinterpret_cast<sockaddr*>(&local), &local_length);
        const std::string http_port = std::to_string(ntohs(local.sin_port));
        _putenv_s("CTR_ONLINE_HTTP_PORT", http_port.c_str());
        // Project discovery socket on loopback (the test's project server address).
        const SOCKET discovery = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in discovery_address{};
        discovery_address.sin_family = AF_INET;
        discovery_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(discovery, reinterpret_cast<sockaddr*>(&discovery_address), sizeof(discovery_address));
        int discovery_length = sizeof(discovery_address);
        getsockname(discovery, reinterpret_cast<sockaddr*>(&discovery_address), &discovery_length);
        const int discovery_port = ntohs(discovery_address.sin_port);
        static std::string seen_paths;
        std::atomic<bool> serving{true};
        std::thread server([listener, discovery_port, &serving] {
            while (serving) {
                const SOCKET client = accept(listener, nullptr, nullptr);
                if (client == INVALID_SOCKET) break;
                std::string request;
                char buffer[2048];
                while (request.find("\r\n\r\n") == std::string::npos) {
                    const int n = recv(client, buffer, sizeof(buffer), 0);
                    if (n <= 0) break;
                    request.append(buffer, n);
                }
                const std::string path = request.substr(4, request.find(' ', 4) - 4);
                seen_paths += path + "\n";
                std::string body = "{}", status = "403 Forbidden";
                if (path == "/ctr/v1/config") {
                    status = "200 OK";
                    body = "{\"discovery_port\":" + std::to_string(discovery_port) + "}";
                } else if (path == "/ctr/v1/peer-allowed?ip=203.0.113.5") {
                    status = "200 OK";
                }
                const std::string answer = "HTTP/1.1 " + status + "\r\nContent-Length: " +
                                           std::to_string(body.size()) + "\r\n\r\n" + body;
                send(client, answer.data(), static_cast<int>(answer.size()), 0);
                closesocket(client);
            }
        });
        const int peer_socket = net_socket("internet_udp", 2, 2, 0);
        auto any = loopback(0);
        any.addr = 0; // INADDR_ANY, as the game binds its UDP socket
        net_bind(peer_socket, &any, sizeof(any));
        OrbisSockaddrIn confirmed{}, refused{};
        confirmed.len = refused.len = 16;
        confirmed.family = refused.family = 2;
        confirmed.port = refused.port = htons(3074);
        InetPtonA(AF_INET, "203.0.113.5", &confirmed.addr);
        InetPtonA(AF_INET, "203.0.113.6", &refused.addr);
        const char probe[] = "peer";
        // TEST-NET-3 addresses (RFC 5737): never assigned to a host.
        check(net_sendto(peer_socket, probe, 4, 0, &confirmed, sizeof(confirmed)) == 4,
              "sendto public peer confirmed by the project server");
        check(net_sendto(peer_socket, probe, 4, 0, &refused, sizeof(refused)) == (kErrorBase | 65),
              "sendto public address refused by the project server");
        check(net_sendto(peer_socket, probe, 4, 0, &confirmed, sizeof(confirmed)) == 4 &&
                  seen_paths.find("/ctr/v1/peer-allowed?ip=203.0.113.5\n") ==
                      seen_paths.rfind("/ctr/v1/peer-allowed?ip=203.0.113.5\n"),
              "confirmed peer cached (one server query)");
        auto server_3074 = loopback(3074);
        const char discovery_request[] = {30, 3, 0};
        check(net_sendto(peer_socket, discovery_request, 3, 0, &server_3074, sizeof(server_3074)) == 3,
              "sendto project server port 3074");
        char received[16] = {};
        sockaddr_in client_address{};
        int client_length = sizeof(client_address);
        const DWORD wait = 2000;
        setsockopt(discovery, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&wait), sizeof(wait));
        const int got = recvfrom(discovery, received, sizeof(received), 0,
                                 reinterpret_cast<sockaddr*>(&client_address), &client_length);
        check(got == 3 && !std::memcmp(received, discovery_request, 3),
              "datagram to server:3074 delivered to the announced discovery port");
        const char reply[] = {31, 3, 0};
        sendto(discovery, reply, 3, 0, reinterpret_cast<sockaddr*>(&client_address), client_length);
        OrbisSockaddrIn source{};
        std::uint32_t source_length = sizeof(source);
        char answer[16] = {};
        check(net_recvfrom(peer_socket, answer, sizeof(answer), 0, &source, &source_length) == 3 &&
                  source.port == htons(3074) && source.addr == htonl(INADDR_LOOPBACK),
              "discovery reply reported from server port 3074");
        serving = false;
        closesocket(listener);
        server.join();
        closesocket(discovery);
        net_close(peer_socket);
    }

    std::printf("{\"scope\":\"online_services_test\",\"passed\":%s,\"failures\":%d}\n",
                failures ? "false" : "true", failures);
    return failures ? 1 : 0;
}
