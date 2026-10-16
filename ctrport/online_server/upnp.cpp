// SPDX-License-Identifier: GPL-2.0-or-later
#include "upnp.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <optional>
#pragma comment(lib, "ws2_32.lib")

namespace ctr_online {
namespace {
struct Wsa {
    bool ok;
    Wsa() { WSADATA d; ok = WSAStartup(MAKEWORD(2, 2), &d) == 0; }
    ~Wsa() { if (ok) WSACleanup(); }
};
struct Socket {
    SOCKET s = INVALID_SOCKET;
    explicit Socket(SOCKET v) : s(v) {}
    ~Socket() { if (s != INVALID_SOCKET) closesocket(s); }
};
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::string xml_escape(const std::string& s) {
    std::string out;
    for (const char c : s) {
        if (c == '&') out += "&amp;"; else if (c == '<') out += "&lt;"; else if (c == '>') out += "&gt;"; else out += c;
    }
    return out;
}
// Text of the first <tag>...</tag> (any namespace prefix) after `from`.
std::optional<std::string> xml_value(const std::string& xml, const std::string& tag, std::size_t from = 0, std::size_t to = std::string::npos) {
    const std::string lx = lower(xml.substr(0, std::min(to, xml.size()))), lt = lower(tag);
    for (std::size_t open = lx.find('<', from); open != std::string::npos; open = lx.find('<', open + 1)) {
        const std::size_t close = lx.find('>', open);
        if (close == std::string::npos) return std::nullopt;
        std::string name = lx.substr(open + 1, close - open - 1);
        if (const std::size_t colon = name.find(':'); colon != std::string::npos) name = name.substr(colon + 1);
        if (name != lt) continue;
        const std::size_t end = lx.find("</", close);
        if (end == std::string::npos) return std::nullopt;
        return trim(xml.substr(close + 1, end - close - 1));
    }
    return std::nullopt;
}
struct Url { std::string host; std::uint16_t port = 80; std::string path = "/"; };
std::optional<Url> parse_url(const std::string& text) {
    if (lower(text.substr(0, 7)) != "http://") return std::nullopt;
    const std::string rest = text.substr(7);
    const std::size_t slash = rest.find('/');
    const std::string authority = rest.substr(0, slash);
    Url url;
    url.path = slash == std::string::npos ? "/" : rest.substr(slash);
    const std::size_t colon = authority.rfind(':');
    url.host = authority.substr(0, colon);
    if (colon != std::string::npos) {
        try { url.port = static_cast<std::uint16_t>(std::stoi(authority.substr(colon + 1))); } catch (...) { return std::nullopt; }
    }
    return url;
}
std::optional<sockaddr_in> resolve(const std::string& host, std::uint16_t port) {
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &a.sin_addr) == 1) return a;
    return std::nullopt; // routers advertise numeric addresses; no name lookups
}
bool connect_timeout(SOCKET s, const sockaddr_in& a, int ms) {
    u_long nonblocking = 1;
    ioctlsocket(s, FIONBIO, &nonblocking);
    connect(s, reinterpret_cast<const sockaddr*>(&a), sizeof(a));
    fd_set write_set, error_set;
    FD_ZERO(&write_set); FD_SET(s, &write_set);
    FD_ZERO(&error_set); FD_SET(s, &error_set);
    timeval tv{ms / 1000, (ms % 1000) * 1000};
    const int ready = select(0, nullptr, &write_set, &error_set, &tv);
    nonblocking = 0;
    ioctlsocket(s, FIONBIO, &nonblocking);
    return ready > 0 && FD_ISSET(s, &write_set);
}
struct HttpResult { int status = 0; std::string body; std::string local_ip; };
std::optional<HttpResult> http_request(const Url& url, const std::string& request) {
    const auto address = resolve(url.host, url.port);
    if (!address) return std::nullopt;
    Socket s(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (s.s == INVALID_SOCKET || !connect_timeout(s.s, *address, 3000)) return std::nullopt;
    DWORD timeout = 5000;
    setsockopt(s.s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(s.s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    if (send(s.s, request.data(), static_cast<int>(request.size()), 0) != static_cast<int>(request.size())) return std::nullopt;
    HttpResult result;
    sockaddr_in local{};
    int local_size = sizeof(local);
    if (getsockname(s.s, reinterpret_cast<sockaddr*>(&local), &local_size) == 0) {
        char buf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
        result.local_ip = buf;
    }
    std::string raw;
    char buffer[8192];
    while (raw.size() < (4u << 20)) {
        const int n = recv(s.s, buffer, sizeof(buffer), 0);
        if (n <= 0) break;
        raw.append(buffer, static_cast<std::size_t>(n));
        // Stop at Content-Length when the router keeps the connection open.
        const std::size_t end = raw.find("\r\n\r\n");
        if (end != std::string::npos) {
            const std::string head = lower(raw.substr(0, end));
            const std::size_t cl = head.find("\r\ncontent-length:");
            if (cl != std::string::npos && raw.size() >= end + 4 + std::strtoull(head.c_str() + cl + 17, nullptr, 10)) break;
        }
    }
    const std::size_t end = raw.find("\r\n\r\n");
    if (end == std::string::npos || raw.compare(0, 5, "HTTP/") != 0) return std::nullopt;
    const std::size_t space = raw.find(' ');
    result.status = std::atoi(raw.c_str() + space + 1);
    const std::string head = lower(raw.substr(0, end));
    std::string body = raw.substr(end + 4);
    if (head.find("transfer-encoding: chunked") != std::string::npos || head.find("transfer-encoding:chunked") != std::string::npos) {
        std::string decoded;
        std::size_t p = 0;
        while (p < body.size()) {
            const std::size_t line = body.find("\r\n", p);
            if (line == std::string::npos) break;
            const std::size_t size = std::strtoull(body.c_str() + p, nullptr, 16);
            if (!size) break;
            decoded += body.substr(line + 2, size);
            p = line + 2 + size + 2;
        }
        body = decoded;
    } else if (const std::size_t cl = head.find("\r\ncontent-length:"); cl != std::string::npos) {
        body = body.substr(0, std::strtoull(head.c_str() + cl + 17, nullptr, 10));
    }
    result.body = body;
    return result;
}
struct Service { std::string type; Url control; };
struct SoapReply { bool ok = false; int error_code = 0; std::string error, body; };
SoapReply soap(const Service& service, const std::string& action, const std::vector<std::pair<std::string, std::string>>& args) {
    std::string inner;
    for (const auto& [name, value] : args) inner += "<" + name + ">" + xml_escape(value) + "</" + name + ">";
    const std::string body =
        "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:" + action + " xmlns:u=\"" + service.type + "\">" +
        inner + "</u:" + action + "></s:Body></s:Envelope>\r\n";
    const std::string request = "POST " + service.control.path + " HTTP/1.1\r\nHost: " + service.control.host + ":" +
                                std::to_string(service.control.port) + "\r\nContent-Type: text/xml; charset=\"utf-8\"\r\n"
                                "SOAPAction: \"" + service.type + "#" + action + "\"\r\nContent-Length: " + std::to_string(body.size()) +
                                "\r\nConnection: close\r\n\r\n" + body;
    SoapReply reply;
    const auto result = http_request(service.control, request);
    if (!result) { reply.error = "router did not answer"; return reply; }
    reply.body = result->body;
    if (result->status == 200) { reply.ok = true; return reply; }
    reply.error_code = std::atoi(xml_value(result->body, "errorCode").value_or("0").c_str());
    reply.error = "HTTP " + std::to_string(result->status) + " UPnP error " + std::to_string(reply.error_code) + " " +
                  xml_value(result->body, "errorDescription").value_or("");
    return reply;
}
// SSDP search: LOCATION URLs of Internet gateway devices.
std::vector<std::string> ssdp_locations(const std::string& interface_ip) {
    std::vector<std::string> found;
    Socket s(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (s.s == INVALID_SOCKET) return found;
    sockaddr_in local{};
    local.sin_family = AF_INET;
    if (!interface_ip.empty()) inet_pton(AF_INET, interface_ip.c_str(), &local.sin_addr);
    bind(s.s, reinterpret_cast<const sockaddr*>(&local), sizeof(local));
    DWORD ttl = 2;
    setsockopt(s.s, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));
    if (!interface_ip.empty()) {
        in_addr iface{};
        inet_pton(AF_INET, interface_ip.c_str(), &iface);
        setsockopt(s.s, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&iface), sizeof(iface));
    }
    sockaddr_in group{};
    group.sin_family = AF_INET;
    group.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &group.sin_addr);
    const char* targets[] = {"urn:schemas-upnp-org:device:InternetGatewayDevice:1", "urn:schemas-upnp-org:device:InternetGatewayDevice:2",
                             "urn:schemas-upnp-org:service:WANIPConnection:1", "urn:schemas-upnp-org:service:WANIPConnection:2",
                             "urn:schemas-upnp-org:service:WANPPPConnection:1",
                             // Any UPnP device: tells "router without port forwarding" from "no answer".
                             "upnp:rootdevice"};
    for (int round = 0; round < 2; ++round)
        for (const char* st : targets) {
            const std::string m = std::string("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\nST: ") + st + "\r\n\r\n";
            sendto(s.s, m.data(), static_cast<int>(m.size()), 0, reinterpret_cast<const sockaddr*>(&group), sizeof(group));
        }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2500);
    char buffer[2048];
    while (true) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) break;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(s.s, &set);
        timeval tv{static_cast<long>(left / 1000), static_cast<long>((left % 1000) * 1000)};
        if (select(0, &set, nullptr, nullptr, &tv) <= 0) break;
        const int n = recv(s.s, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) continue;
        const std::string reply(buffer, static_cast<std::size_t>(n));
        const std::string lr = lower(reply);
        const std::size_t at = lr.find("\r\nlocation:");
        if (at == std::string::npos) continue;
        const std::size_t end = reply.find("\r\n", at + 2);
        const std::string location = trim(reply.substr(at + 11, end - at - 11));
        if (std::find(found.begin(), found.end(), location) == found.end()) found.push_back(location);
    }
    return found;
}
std::vector<Service> gateway_services(const std::string& location) {
    std::vector<Service> services;
    const auto url = parse_url(location);
    if (!url) return services;
    const auto result = http_request(*url, "GET " + url->path + " HTTP/1.1\r\nHost: " + url->host + ":" + std::to_string(url->port) +
                                               "\r\nConnection: close\r\n\r\n");
    if (!result || result->status != 200) return services;
    const std::string& xml = result->body;
    std::string base = "http://" + url->host + ":" + std::to_string(url->port);
    if (const auto url_base = xml_value(xml, "URLBase"); url_base && !url_base->empty()) {
        if (const auto parsed = parse_url(*url_base)) base = "http://" + parsed->host + ":" + std::to_string(parsed->port);
    }
    const std::string lx = lower(xml);
    for (std::size_t open = lx.find("<service>"); open != std::string::npos; open = lx.find("<service>", open + 1)) {
        const std::size_t close = lx.find("</service>", open);
        if (close == std::string::npos) break;
        const auto type = xml_value(xml, "serviceType", open, close);
        const auto control = xml_value(xml, "controlURL", open, close);
        if (!type || !control) continue;
        if (type->find(":service:WANIPConnection:") == std::string::npos && type->find(":service:WANPPPConnection:") == std::string::npos) continue;
        std::string full = *control;
        if (lower(full.substr(0, 7)) != "http://") full = base + (full.empty() || full[0] != '/' ? "/" : "") + full;
        if (const auto parsed = parse_url(full)) services.push_back({*type, *parsed});
    }
    // WAN IP connections first.
    std::stable_sort(services.begin(), services.end(), [](const Service& a, const Service& b) {
        return (a.type.find("WANIPConnection") != std::string::npos) > (b.type.find("WANIPConnection") != std::string::npos);
    });
    return services;
}
} // namespace

std::string ipv4_scope(const std::string& ip) {
    in_addr a{};
    if (inet_pton(AF_INET, ip.c_str(), &a) != 1) return {};
    const std::uint32_t v = ntohl(a.s_addr);
    if (v == 0) return "unspecified";
    if ((v >> 24) == 127) return "loopback";
    if ((v >> 24) == 10 || (v >> 20) == 0xAC1 || (v >> 16) == 0xC0A8) return "private";
    if ((v >> 22) == (100u << 2 | 1)) return "cgnat"; // 100.64.0.0/10
    if ((v >> 16) == 0xA9FE) return "link-local";
    if ((v >> 28) == 14) return "multicast";
    return "public";
}
std::string local_lan_address() {
    Wsa wsa;
    Socket s(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
    if (s.s == INVALID_SOCKET) return {};
    // Route lookup toward TEST-NET-2 (RFC 5737): connect() on UDP sends nothing.
    sockaddr_in target{};
    target.sin_family = AF_INET;
    target.sin_port = htons(9);
    inet_pton(AF_INET, "198.51.100.1", &target.sin_addr);
    if (connect(s.s, reinterpret_cast<const sockaddr*>(&target), sizeof(target)) != 0) return {};
    sockaddr_in local{};
    int size = sizeof(local);
    if (getsockname(s.s, reinterpret_cast<sockaddr*>(&local), &size) != 0) return {};
    char buf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
    return buf;
}

PortMapper::PortMapper(Log log) : log_(std::move(log)) {}
PortMapper::~PortMapper() { close(); }

bool PortMapper::add(const PortRequest& port, std::string& error) {
    const Service service{service_type_, *parse_url(control_url_)};
    auto attempt = [&] {
        return soap(service, "AddPortMapping",
                    {{"NewRemoteHost", ""}, {"NewExternalPort", std::to_string(port.port)}, {"NewProtocol", port.protocol},
                     {"NewInternalPort", std::to_string(port.port)}, {"NewInternalClient", local_ip_}, {"NewEnabled", "1"},
                     {"NewPortMappingDescription", description_ + " " + port.purpose}, {"NewLeaseDuration", std::to_string(lease_)}});
    };
    SoapReply reply = attempt();
    if (!reply.ok && reply.error_code == 725 && lease_) { // OnlyPermanentLeasesSupported
        lease_ = 0;
        reply = attempt();
    }
    if (!reply.ok && reply.error_code == 718) { // ConflictInMappingEntry: ours from a previous run?
        const SoapReply entry = soap(service, "GetSpecificPortMappingEntry",
                                     {{"NewRemoteHost", ""}, {"NewExternalPort", std::to_string(port.port)}, {"NewProtocol", port.protocol}});
        const std::string owner = entry.ok ? xml_value(entry.body, "NewInternalClient").value_or("") : "";
        if (owner == local_ip_) {
            soap(service, "DeletePortMapping", {{"NewRemoteHost", ""}, {"NewExternalPort", std::to_string(port.port)}, {"NewProtocol", port.protocol}});
            reply = attempt();
        } else if (!owner.empty()) {
            error = "port already forwarded to another device (" + owner + ")";
            return false;
        }
    }
    if (!reply.ok) error = reply.error;
    return reply.ok;
}

bool PortMapper::open(const std::vector<PortRequest>& ports, const std::string& description, const std::string& location_override) {
    close();
    closing_ = false;
    Wsa wsa;
    description_ = description;
    results_.clear();
    const std::string lan = local_lan_address();
    const std::vector<std::string> locations = location_override.empty() ? ssdp_locations(lan) : std::vector<std::string>{location_override};
    if (log_) log_("upnp: " + std::to_string(locations.size()) + " dispositivi UPnP " + (location_override.empty() ? "hanno risposto da " + (lan.empty() ? std::string("?") : lan) : "indicati"));
    for (const std::string& location : locations) {
        for (const Service& service : gateway_services(location)) {
            const SoapReply ip = soap(service, "GetExternalIPAddress", {});
            const std::string external = ip.ok ? xml_value(ip.body, "NewExternalIPAddress").value_or("") : "";
            if (!ip.ok || external.empty() || external == "0.0.0.0") {
                if (log_) log_("upnp: " + service.type + " at " + location + " has no external address (" + ip.error + ")");
                continue;
            }
            control_url_ = "http://" + service.control.host + ":" + std::to_string(service.control.port) + service.control.path;
            service_type_ = service.type;
            external_ip_ = external;
            // The LAN address the router sees for this PC.
            const auto probe = http_request(service.control, "GET / HTTP/1.1\r\nHost: " + service.control.host + "\r\nConnection: close\r\n\r\n");
            local_ip_ = probe && !probe->local_ip.empty() ? probe->local_ip : lan;
            if (log_) log_("upnp: router " + control_url_ + " (" + service_type_ + "), external address " + external_ip_ + ", this PC " + local_ip_);
            for (const PortRequest& port : ports) {
                PortResult result{port};
                result.ok = add(port, result.error);
                if (log_) log_("upnp: " + port.protocol + " " + std::to_string(port.port) + " (" + port.purpose + "): " +
                               (result.ok ? "opened" : "not opened: " + result.error));
                results_.push_back(result);
            }
            if (lease_) renewer_ = std::thread([this] { renew_loop(); });
            return true;
        }
    }
    error_ = locations.empty() ? "no UPnP device responded (UPnP disabled on the router or blocked by the network)"
                               : "none of the " + std::to_string(locations.size()) +
                                     " UPnP devices on the network offers port forwarding (UPnP IGD / "
                                     "\"automatic port forwarding\"): enable it on the router or forward ports manually";
    if (log_) log_("upnp: " + error_);
    return false;
}

void PortMapper::renew_loop() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!wake_.wait_for(lock, std::chrono::minutes(30), [this] { return closing_; })) {
        lock.unlock();
        Wsa wsa;
        for (PortResult& result : results_) {
            if (!result.ok) continue;
            std::string error;
            if (!add(result.request, error) && log_) log_("upnp: renewal " + result.request.protocol + " " + std::to_string(result.request.port) + " failed: " + error);
        }
        lock.lock();
    }
}

void PortMapper::close() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closing_ = true;
    }
    wake_.notify_all();
    if (renewer_.joinable()) renewer_.join();
    if (control_url_.empty()) return;
    Wsa wsa;
    const Service service{service_type_, *parse_url(control_url_)};
    for (const PortResult& result : results_) {
        if (!result.ok) continue;
        const SoapReply reply = soap(service, "DeletePortMapping",
                                     {{"NewRemoteHost", ""}, {"NewExternalPort", std::to_string(result.request.port)}, {"NewProtocol", result.request.protocol}});
        if (log_) log_("upnp: " + result.request.protocol + " " + std::to_string(result.request.port) + " chiusa" + (reply.ok ? "" : " (" + reply.error + ")"));
    }
    results_.clear();
    control_url_.clear();
}
} // namespace ctr_online
