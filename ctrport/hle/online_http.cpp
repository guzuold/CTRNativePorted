// SPDX-License-Identifier: GPL-2.0-or-later
// Plain HTTP/1.1 exchange with the project server (see online_http.h).
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include "online_config.h"
#include "online_http.h"

namespace ctr_hle::online {
namespace {
int http_port() {
    for (const char* name : {"CTR_ONLINE_HTTP_PORT", "CTR_ONLINE_WEBAPI_PORT"}) {
        const char* value = std::getenv(name);
        const int port = value ? std::atoi(value) : 0;
        if (port > 0 && port < 65536) {
            return port;
        }
    }
    return 8780;
}
} // namespace

std::string lower(std::string text) {
    for (auto& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string json_escape(const std::string& text) {
    std::string out;
    for (const char c : text) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) >= 0x7f) {
            char escape[8];
            std::snprintf(escape, sizeof(escape), "\\u%04x", static_cast<unsigned char>(c));
            out += escape;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

bool http_exchange(const std::string& method, const std::string& target,
                   const std::vector<std::pair<std::string, std::string>>& headers,
                   const std::string& body, HttpResponse* response) {
    static const int startup = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data); }();
    if (startup) return false;
    const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        return false;
    }
    const DWORD timeout = 15000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(static_cast<u_short>(http_port()));
    server.sin_addr.s_addr = server_address();
    if (connect(s, reinterpret_cast<sockaddr*>(&server), sizeof(server))) {
        closesocket(s);
        return false;
    }
    bool has_host = false;
    std::string message = method + " " + target + " HTTP/1.1\r\n";
    for (const auto& [name, value] : headers) {
        const std::string key = lower(name);
        if (key == "content-length" || key == "connection" || key == "transfer-encoding") {
            continue;
        }
        has_host = has_host || key == "host";
        message += name + ": " + value + "\r\n";
    }
    if (!has_host) {
        message += "Host: ctr-online\r\n";
    }
    const char* multiplier = std::getenv("CTR_WUMPA_MULTIPLIER");
    message += std::string("X-Ctr-Wumpa-Multiplier: ") +
        (multiplier && std::string(multiplier) == "100" ? "100" : "1") + "\r\n";
    message += "X-Ctr-Online-Id: " + identity().online_id + "\r\nConnection: close\r\n";
    if (!room_token().empty()) message += "X-Ctr-Room-Token: " + room_token() + "\r\n";
    message += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    for (std::size_t sent = 0; sent < message.size();) {
        const int n = send(s, message.data() + sent, static_cast<int>(message.size() - sent), 0);
        if (n <= 0) {
            closesocket(s);
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }
    std::string raw;
    char buffer[8192];
    for (;;) {
        const int n = recv(s, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            break;
        }
        raw.append(buffer, static_cast<std::size_t>(n));
    }
    closesocket(s);
    const auto header_end = raw.find("\r\n\r\n");
    if (raw.compare(0, 5, "HTTP/") || header_end == std::string::npos) {
        return false;
    }
    *response = {};
    response->status = std::atoi(raw.c_str() + raw.find(' ') + 1);
    const std::size_t first_line_end = raw.find("\r\n") + 2;
    response->header_block = raw.substr(first_line_end, header_end + 2 - first_line_end);
    for (std::size_t line = first_line_end; line < header_end;) {
        const auto end = raw.find("\r\n", line);
        const auto colon = raw.find(':', line);
        if (colon != std::string::npos && colon < end) {
            std::size_t value = colon + 1;
            while (value < end && raw[value] == ' ') {
                ++value;
            }
            response->headers[lower(raw.substr(line, colon - line))] = raw.substr(value, end - value);
        }
        line = end + 2;
    }
    response->body = raw.substr(header_end + 4);
    return true;
}
} // namespace ctr_hle::online
