// SPDX-License-Identifier: GPL-2.0-or-later
// Automatic port forwarding on the home router with UPnP IGD (public specification: UPnP
// Forum InternetGatewayDevice:1/2, WANIPConnection:1/2, WANPPPConnection:1). SSDP discovery on
// the LAN, then SOAP AddPortMapping / GetExternalIPAddress / DeletePortMapping to the router's
// control URL. Only the local network (the router) is contacted. Mappings carry a one-hour
// lease renewed every 30 minutes and are deleted by close(); a router that only accepts
// permanent leases gets permanent ones, also deleted by close().
#pragma once
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ctr_online {
struct PortRequest {
    std::string protocol; // "TCP" or "UDP"
    std::uint16_t port;
    std::string purpose;
};
struct PortResult {
    PortRequest request;
    bool ok = false;
    std::string error;
};

class PortMapper {
public:
    using Log = std::function<void(const std::string&)>;
    explicit PortMapper(Log log = {});
    ~PortMapper();
    PortMapper(const PortMapper&) = delete;
    PortMapper& operator=(const PortMapper&) = delete;
    // Finds the router and maps every port to this PC. Returns true when a router answered
    // (individual mappings may still fail: see results()).
    // location: a device description URL to use instead of SSDP discovery (routers that do not
    // answer the search, and the synthetic router of tools/online/test_upnp_client.py).
    bool open(const std::vector<PortRequest>& ports, const std::string& description, const std::string& location = {});
    void close();
    const std::vector<PortResult>& results() const { return results_; }
    const std::string& external_ip() const { return external_ip_; }
    const std::string& local_ip() const { return local_ip_; }
    const std::string& router() const { return control_url_; }
    const std::string& error() const { return error_; }

private:
    bool add(const PortRequest& port, std::string& error);
    void renew_loop();
    Log log_;
    std::string control_url_, service_type_, local_ip_, external_ip_, description_, error_;
    std::vector<PortResult> results_;
    unsigned lease_ = 3600;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool closing_ = false;
    std::thread renewer_;
};

// Classification of an IPv4 text: private (RFC 1918), shared CGNAT (RFC 6598), loopback,
// link-local or public. Empty when not an IPv4 address.
std::string ipv4_scope(const std::string& ip);
// The LAN address of this PC (route lookup only, no packet sent).
std::string local_lan_address();
} // namespace ctr_online
