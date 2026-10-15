// SPDX-License-Identifier: GPL-2.0-or-later
// Online configuration and network policy (see online_config.h).
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include "ctr_hle.h"
#include "online_config.h"
#include "online_http.h"
#pragma comment(lib, "ws2_32.lib")

namespace ctr_hle::online {
namespace {
bool valid_online_id(const char* value) {
    const std::size_t length = value ? std::strlen(value) : 0;
    if (length < 3 || length > 16) {
        return false;
    }
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

bool ends_with(const std::string& value, const char* suffix) {
    const std::size_t length = std::strlen(suffix);
    return value.size() >= length && value.compare(value.size() - length, length, suffix) == 0;
}

bool private_or_local(std::uint32_t network_order) {
    const std::uint32_t a = ntohl(network_order);
    return (a >> 24) == 127 ||            // loopback
           (a >> 24) == 10 ||             // 10.0.0.0/8
           (a >> 20) == 0xAC1 ||          // 172.16.0.0/12
           (a >> 16) == 0xC0A8 ||         // 192.168.0.0/16
           (a >> 16) == 0xA9FE ||         // 169.254.0.0/16 link-local
           (a >> 22) == (100u << 2 | 1) || // 100.64.0.0/10 (CGNAT, VPN overlays)
           (a >> 28) == 0xE ||            // 224.0.0.0/4 multicast (LAN discovery: SSDP/UPnP)
           a == 0 || a == 0xFFFFFFFFu;    // any / limited broadcast
}
} // namespace

const Identity& identity() {
    static const Identity value = [] {
        Identity result;
        const char* configured = std::getenv("CTR_ONLINE_ID");
        result.online_id = valid_online_id(configured) ? configured : "CTRPlayer";
        // FNV-1a 64 over the online ID: stable per identity, never zero.
        std::uint64_t hash = 0xcbf29ce484222325ull;
        for (const unsigned char c : result.online_id) {
            hash = (hash ^ c) * 0x100000001b3ull;
        }
        result.account_id = hash ? hash : 1;
        return result;
    }();
    return value;
}

const std::string& room_token() {
    static const std::string value = [] {
        const char* configured = std::getenv("CTR_ONLINE_ROOM_TOKEN");
        if (!configured || std::strlen(configured) != 64) return std::string{};
        for (unsigned i = 0; i < 64; ++i)
            if (!((configured[i] >= '0' && configured[i] <= '9') ||
                  (configured[i] >= 'a' && configured[i] <= 'f'))) return std::string{};
        return std::string(configured);
    }();
    return value;
}

std::uint32_t server_address() {
    static const std::uint32_t value = [] {
        const char* configured = std::getenv("CTR_ONLINE_SERVER");
        in_addr address{};
        if (configured && InetPtonA(AF_INET, configured, &address) == 1) {
            return static_cast<std::uint32_t>(address.s_addr);
        }
        return static_cast<std::uint32_t>(htonl(INADDR_LOOPBACK));
    }();
    return value;
}

std::uint32_t local_address() {
    static const std::uint32_t value = [] {
        WSADATA data{};
        WSAStartup(MAKEWORD(2, 2), &data);
        std::uint32_t result = htonl(INADDR_LOOPBACK);
        // Route lookup only: a UDP connect sends nothing.
        const SOCKET probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (probe != INVALID_SOCKET) {
            for (const std::uint32_t destination :
                 {server_address(), static_cast<std::uint32_t>(htonl(0xC6336401u))}) {
                // 198.51.100.1 (TEST-NET-2) only selects the default route; nothing is sent.
                sockaddr_in target{};
                target.sin_family = AF_INET;
                target.sin_port = htons(9);
                target.sin_addr.s_addr = destination;
                sockaddr_in local{};
                int length = sizeof(local);
                if (connect(probe, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == 0 &&
                    getsockname(probe, reinterpret_cast<sockaddr*>(&local), &length) == 0 &&
                    local.sin_addr.s_addr != 0) {
                    result = local.sin_addr.s_addr;
                    if ((ntohl(result) >> 24) != 127) {
                        break;
                    }
                }
            }
            closesocket(probe);
        }
        return result;
    }();
    return value;
}

bool resolve(const char* hostname, std::uint32_t* address, const char** reason) {
    if (!hostname || !address) {
        *reason = "invalid_argument";
        return false;
    }
    in_addr numeric{};
    if (InetPtonA(AF_INET, hostname, &numeric) == 1) {
        *address = numeric.s_addr;
        *reason = "numeric";
        return true;
    }
    std::string name = hostname;
    for (auto& c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (name == "localhost") {
        *address = htonl(INADDR_LOOPBACK);
        *reason = "localhost";
        return true;
    }
    if (ends_with(name, ".demonware.net")) {
        *address = server_address();
        *reason = "project_server";
        return true;
    }
    *reason = "blocked_external_host";
    return false;
}

namespace {
bool strict_peers() {
    static const bool value = [] {
        const char* configured = std::getenv("CTR_ONLINE_STRICT_PEERS");
        return configured && !std::strcmp(configured, "1");
    }();
    return value;
}

// Public peers: asked to the project server once, then cached (allowed 10 min, refused 5 s
// so that a player who joins the room later becomes reachable).
bool peer_allowed(std::uint32_t address) {
    using clock = std::chrono::steady_clock;
    static std::mutex mutex;
    static std::map<std::uint32_t, std::pair<bool, clock::time_point>> cache;
    {
        std::scoped_lock lock{mutex};
        const auto it = cache.find(address);
        if (it != cache.end() && clock::now() < it->second.second) {
            return it->second.first;
        }
    }
    HttpResponse response;
    const bool reached = http_exchange("GET", "/ctr/v1/peer-allowed?ip=" + format_address(address),
                                       {}, "", &response);
    const bool allowed = reached && response.status == 200;
    {
        std::scoped_lock lock{mutex};
        cache[address] = {allowed, clock::now() + std::chrono::seconds(allowed ? 600 : 5)};
    }
    ctr_hle::log_call("ctr_online", "peer_allowed", allowed ? 1 : 0,
                      (",\"address\":\"" + format_address(address) + "\",\"server_status\":" +
                       std::to_string(reached ? response.status : 0))
                          .c_str());
    return allowed;
}

std::atomic<int> fetched_discovery_port{0};
std::atomic<std::uint32_t> fetched_public_address{0};
} // namespace

bool destination_allowed(std::uint32_t address) {
    if (address == server_address()) {
        return true;
    }
    const std::uint32_t a = ntohl(address);
    if ((a >> 24) == 127 || (a >> 28) == 0xE || a == 0xFFFFFFFFu || a == 0) {
        return true; // loopback, multicast, broadcast, any
    }
    if (!strict_peers() && private_or_local(address)) {
        return true;
    }
    return peer_allowed(address);
}

std::uint32_t server_public_address() {
    return fetched_public_address.load();
}

std::uint16_t known_discovery_port() {
    const int port = fetched_discovery_port.load();
    return static_cast<std::uint16_t>(port > 0 ? port : 3074);
}

std::uint16_t discovery_port() {
    if (fetched_discovery_port.load() == 0) {
        int port = 3074;
        HttpResponse response;
        if (http_exchange("GET", "/ctr/v1/config", {}, "", &response) && response.status == 200) {
            const auto key = response.body.find("\"discovery_port\"");
            const auto colon = key == std::string::npos ? key : response.body.find(':', key);
            const int value = colon == std::string::npos ? 0 : std::atoi(response.body.c_str() + colon + 1);
            if (value > 0 && value < 65536) {
                port = value;
            }
            const auto public_key = response.body.find("\"public_host\"");
            const auto quote = public_key == std::string::npos ? public_key
                                                                : response.body.find('"', response.body.find(':', public_key));
            const auto end = quote == std::string::npos ? quote : response.body.find('"', quote + 1);
            in_addr parsed{};
            if (end != std::string::npos &&
                InetPtonA(AF_INET, response.body.substr(quote + 1, end - quote - 1).c_str(), &parsed) == 1) {
                fetched_public_address.store(parsed.s_addr);
            }
        }
        int expected = 0;
        fetched_discovery_port.compare_exchange_strong(expected, port);
        ctr_hle::log_call("ctr_online", "discovery_port", known_discovery_port());
    }
    return known_discovery_port();
}

std::string format_address(std::uint32_t address) {
    char text[INET_ADDRSTRLEN] = {};
    in_addr value{};
    value.s_addr = address;
    InetNtopA(AF_INET, &value, text, sizeof(text));
    return text;
}
} // namespace ctr_hle::online
