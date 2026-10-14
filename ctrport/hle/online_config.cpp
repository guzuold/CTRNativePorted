// SPDX-License-Identifier: GPL-2.0-or-later
// Online configuration and network policy (see online_config.h).
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include "online_config.h"
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
            sockaddr_in target{};
            target.sin_family = AF_INET;
            target.sin_port = htons(9);
            target.sin_addr.s_addr = server_address();
            sockaddr_in local{};
            int length = sizeof(local);
            if (connect(probe, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == 0 &&
                getsockname(probe, reinterpret_cast<sockaddr*>(&local), &length) == 0 &&
                local.sin_addr.s_addr != 0) {
                result = local.sin_addr.s_addr;
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

bool destination_allowed(std::uint32_t address) {
    return address == server_address() || private_or_local(address);
}

std::string format_address(std::uint32_t address) {
    char text[INET_ADDRSTRLEN] = {};
    in_addr value{};
    value.s_addr = address;
    InetNtopA(AF_INET, &value, text, sizeof(text));
    return text;
}
} // namespace ctr_hle::online
