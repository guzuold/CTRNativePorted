// SPDX-License-Identifier: GPL-2.0-or-later
#include "launcher_online.hpp"
#include "server.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

namespace ctr_online {
namespace {
struct Wsa {
    Wsa() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~Wsa() { WSACleanup(); }
};
} // namespace

std::vector<PortRequest> host_ports(std::uint16_t http_port, std::uint16_t discovery_port, std::uint16_t game_udp_port) {
    return {{"TCP", http_port, "server HTTP"},
            {"TCP", 3074, "lobby"},
            {"UDP", game_udp_port, "gioco"},
            {"UDP", discovery_port, "discovery"},
            {"UDP", static_cast<std::uint16_t>(discovery_port + 1), "discovery"},
            {"UDP", static_cast<std::uint16_t>(discovery_port + 2), "discovery"},
            {"UDP", static_cast<std::uint16_t>(discovery_port + 3), "discovery"},
            {"UDP", 3082, "relay"}};
}

Host::Host() = default;
Host::~Host() { stop(); }

HostStatus Host::start(const std::filesystem::path& root, const std::string& manual_public_ip, bool use_upnp) {
    stop();
    HostStatus status;
    status.lan_ip = local_lan_address();
    auto mapper = std::make_unique<PortMapper>();
    if (use_upnp) {
        status.upnp_router = mapper->open(host_ports(), "CTR Native Port");
        status.ports = mapper->results();
        if (!status.upnp_router) status.warnings.push_back("UPnP: " + mapper->error());
        for (const PortResult& port : status.ports)
            if (!port.ok) status.warnings.push_back("porta " + port.request.protocol + " " + std::to_string(port.request.port) + " non aperta: " + port.error);
    }
    if (!manual_public_ip.empty()) {
        status.public_ip = manual_public_ip;
        status.address_source = "manual";
    } else if (status.upnp_router && ipv4_scope(mapper->external_ip()) == "public") {
        status.public_ip = mapper->external_ip();
        status.address_source = "upnp";
    } else {
        if (status.upnp_router)
            status.warnings.push_back("il router riporta l'indirizzo " + mapper->external_ip() + " (" + ipv4_scope(mapper->external_ip()) +
                                      "): la connessione e' dietro un secondo NAT (per esempio CGNAT); da internet non e' raggiungibile");
        status.public_ip = status.lan_ip.empty() ? "127.0.0.1" : status.lan_ip;
        status.address_source = "lan";
    }
    ServerConfig config;
    config.public_host = status.public_ip;
    config.state_dir = root / "runtime" / "user" / "online";
    config.log_path = default_log_path(root);
    status.log = config.log_path;
    auto server = std::make_unique<Server>(config);
    try {
        const std::vector<std::string> listening = server->start();
        status.listen_failures = server->failures();
        status.traffic_key = server->traffic_key_path();
        bool http = false, lobby = false;
        for (const std::string& name : listening) {
            if (name.rfind("http/", 0) == 0) http = true;
            if (name == "lobby/tcp/3074") lobby = true;
        }
        if (!http || !lobby) {
            status.error = "porte del server occupate (un altro server online e' gia' in esecuzione?)";
            for (const std::string& failure : status.listen_failures) status.error += "\n" + failure;
            server->stop();
            mapper->close();
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = status;
            return status;
        }
        status.running = true;
    } catch (const std::exception& error) {
        status.error = error.what();
        mapper->close();
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = status;
        return status;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    server_ = std::move(server);
    mapper_ = std::move(mapper);
    status_ = status;
    return status;
}

void Host::stop() {
    std::unique_ptr<Server> server;
    std::unique_ptr<PortMapper> mapper;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        server = std::move(server_);
        mapper = std::move(mapper_);
        status_.running = false;
    }
    if (server) server->stop();
    if (mapper) mapper->close();
}

bool Host::running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_ != nullptr;
}
HostStatus Host::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}
std::size_t Host::players() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return server_ ? server_->connected_players() : 0;
}

std::string resolve_ipv4(const std::string& host) {
    if (host.empty()) return {};
    Wsa wsa;
    in_addr literal{};
    if (inet_pton(AF_INET, host.c_str(), &literal) == 1) return host;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &result) != 0 || !result) return {};
    char text[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr, text, sizeof(text));
    freeaddrinfo(result);
    return text;
}

int http_get(const std::string& ipv4, std::uint16_t port, const std::string& path, std::string& body, int timeout_ms) {
    Wsa wsa;
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, ipv4.c_str(), &address.sin_addr) != 1) return 0;
    const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return 0;
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
    connect(s, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    fd_set writable, failed;
    FD_ZERO(&writable); FD_SET(s, &writable);
    FD_ZERO(&failed); FD_SET(s, &failed);
    timeval tv{timeout_ms / 1000, (timeout_ms % 1000) * 1000};
    if (select(0, nullptr, &writable, &failed, &tv) <= 0 || !FD_ISSET(s, &writable)) { closesocket(s); return 0; }
    nonblocking = 0;
    ioctlsocket(s, FIONBIO, &nonblocking);
    const DWORD timeout = static_cast<DWORD>(timeout_ms);
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    const std::string request = "GET " + path + " HTTP/1.1\r\nHost: " + ipv4 + "\r\nConnection: close\r\n\r\n";
    send(s, request.data(), static_cast<int>(request.size()), 0);
    std::string raw;
    char buffer[8192];
    for (int n; (n = recv(s, buffer, sizeof(buffer), 0)) > 0 && raw.size() < (1u << 20);) raw.append(buffer, static_cast<std::size_t>(n));
    closesocket(s);
    const std::size_t end = raw.find("\r\n\r\n");
    if (end == std::string::npos || raw.compare(0, 5, "HTTP/") != 0) return 0;
    body = raw.substr(end + 4);
    return std::atoi(raw.c_str() + raw.find(' ') + 1);
}
} // namespace ctr_online
