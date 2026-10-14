// SPDX-License-Identifier: GPL-2.0-or-later
// libSceNet sockets, epoll and resolver over Winsock (R8, active only with CTR_ONLINE=1).
//
// Behaviour follows pinned shadPS4 v0.18.0 src/core/libraries/network (net.cpp,
// posix_sockets.cpp, sys_net.cpp, net_resolver.cpp, net.h, net_error.h; GPL-2.0-or-later):
// PS4 BSD socket structures translated to Winsock, errors returned as
// ORBIS_NET_ERROR_BASE | errno with the errno also stored in the per-thread sceNetErrnoLoc
// slot. Epoll is implemented with WSAPoll; resolution follows the R8 network policy in
// online_config.h (never Sony or Activision hosts).
// CTR_ONLINE_PORT_OFFSET remaps the local UDP 3074 bind for same-PC instances;
// getsockname reports the actual port, and peer destination/source ports stay intact.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "ctr_hle.h"
#include "online_config.h"
#pragma comment(lib, "ws2_32.lib")

namespace {
constexpr int kErrorBase = static_cast<int>(0x80410100u);
// PS4 errno values (net_error.h).
enum : int {
    E_INTR = 4, E_BADF = 9, E_NOMEM = 12, E_ACCES = 13, E_FAULT = 14, E_EXIST = 17,
    E_INVAL = 22, E_MFILE = 24, E_NOSPC = 28, E_PIPE = 32, E_WOULDBLOCK = 35,
    E_INPROGRESS = 36, E_ALREADY = 37, E_NOTSOCK = 38, E_DESTADDRREQ = 39, E_MSGSIZE = 40,
    E_PROTOTYPE = 41, E_NOPROTOOPT = 42, E_PROTONOSUPPORT = 43, E_OPNOTSUPP = 45,
    E_AFNOSUPPORT = 47, E_ADDRINUSE = 48, E_ADDRNOTAVAIL = 49, E_NETDOWN = 50,
    E_NETUNREACH = 51, E_NETRESET = 52, E_CONNABORTED = 53, E_CONNRESET = 54,
    E_NOBUFS = 55, E_ISCONN = 56, E_NOTCONN = 57, E_TIMEDOUT = 60, E_CONNREFUSED = 61,
    E_LOOP = 62, E_NAMETOOLONG = 63, E_HOSTDOWN = 64, E_HOSTUNREACH = 65,
    E_NOTEMPTY = 66, E_INTERNAL = 204, E_RESOLVER_EBUSY = 221, E_RESOLVER_ENOHOST = 230,
};
// Socket types, levels and options (net.h).
enum : int {
    SOCK_STREAM_ = 1, SOCK_DGRAM_ = 2, SOCK_RAW_ = 3, SOCK_DGRAM_P2P = 6, SOCK_STREAM_P2P = 10,
    AF_INET_ = 2, LEVEL_SOCKET = 0xFFFF, LEVEL_IP = 0, LEVEL_TCP = 6,
    SO_NBIO_ = 0x1200, SO_SNDTIMEO_ = 0x1105, SO_RCVTIMEO_ = 0x1106, SO_ERROR_EX_ = 0x1107,
    SO_CONNECTTIMEO_ = 0x1109, SO_REUSEPORT_ = 0x200, SO_ONESBCAST_ = 0x10000,
    SO_USECRYPTO_ = 0x20000, SO_USESIGNATURE_ = 0x40000, SO_NAME_ = 0x1202,
    SO_POLICY_ = 0x1201, SO_PRIORITY_ = 0x1203, IP_TTLCHK_ = 23, IP_MAXTTL_ = 24,
    TCP_MSS_TO_ADVERTISE_ = 3,
    MSG_PEEK_ = 0x2, MSG_WAITALL_ = 0x40, MSG_DONTWAIT_ = 0x80,
    EPOLLIN_ = 0x1, EPOLLOUT_ = 0x2, EPOLLERR_ = 0x8, EPOLLHUP_ = 0x10, EPOLLDESCID_ = 0x10000,
    EPOLL_CTL_ADD_ = 1, EPOLL_CTL_MOD_ = 2, EPOLL_CTL_DEL_ = 3, RESOLVER_ASYNC_ = 0x1,
};

struct OrbisSockaddrIn {
    std::uint8_t sin_len;
    std::uint8_t sin_family;
    std::uint16_t sin_port;
    std::uint32_t sin_addr;
    std::uint16_t sin_vport;
    char sin_zero[6];
};
static_assert(sizeof(OrbisSockaddrIn) == 16);
union OrbisEpollData {
    void* ptr;
    std::uint32_t u32;
    int fd;
    std::uint64_t u64;
};
struct OrbisEpollEvent {
    std::uint32_t events;
    std::uint32_t pad;
    std::uint64_t ident;
    OrbisEpollData data;
};
static_assert(sizeof(OrbisEpollEvent) == 24);

thread_local int net_errno = 0;

struct Socket {
    SOCKET handle = INVALID_SOCKET;
    int type = 0;
    std::string name;
    int nbio = 0, connect_timeout = 0, reuseport = 0, onesbcast = 0, usecrypto = 0,
        usesignature = 0, ttlchk = 0, maxttl = 0, mss = 0;
    std::uint32_t peer = 0; // last connect destination, network order
    std::uint16_t peer_port = 0;
};
struct Resolver {
    std::string name;
    bool pending = false;
    std::string hostname;
    std::uint32_t* output = nullptr;
    int error = 0;
};
struct Epoll {
    std::string name;
    std::vector<std::pair<int, OrbisEpollEvent>> entries;
};

std::mutex table_mutex;
std::map<int, std::shared_ptr<Socket>> sockets;
std::map<int, std::shared_ptr<Resolver>> resolvers;
std::map<int, std::shared_ptr<Epoll>> epolls;
int next_id = 1;
std::atomic<bool> winsock_ready{false};

void ensure_winsock() {
    if (!winsock_ready.exchange(true)) {
        WSADATA data{};
        WSAStartup(MAKEWORD(2, 2), &data);
    }
}

int fail(int error) {
    net_errno = error;
    return kErrorBase | error;
}

int map_wsa(int wsa) {
    switch (wsa) {
    case WSAEINTR: return E_INTR;
    case WSAEBADF: return E_BADF;
    case WSAEACCES: return E_ACCES;
    case WSAEFAULT: return E_FAULT;
    case WSAEINVAL: return E_INVAL;
    case WSAEMFILE: return E_MFILE;
    case WSAEWOULDBLOCK: return E_WOULDBLOCK;
    case WSAEINPROGRESS: return E_INPROGRESS;
    case WSAEALREADY: return E_ALREADY;
    case WSAENOTSOCK: return E_BADF;
    case WSAEDESTADDRREQ: return E_DESTADDRREQ;
    case WSAEMSGSIZE: return E_MSGSIZE;
    case WSAEPROTOTYPE: return E_PROTOTYPE;
    case WSAENOPROTOOPT: return E_NOPROTOOPT;
    case WSAEPROTONOSUPPORT: return E_PROTONOSUPPORT;
    case WSAEOPNOTSUPP: return E_OPNOTSUPP;
    case WSAEAFNOSUPPORT: return E_AFNOSUPPORT;
    case WSAEADDRINUSE: return E_ADDRINUSE;
    case WSAEADDRNOTAVAIL: return E_ADDRNOTAVAIL;
    case WSAENETDOWN: return E_NETDOWN;
    case WSAENETUNREACH: return E_NETUNREACH;
    case WSAENETRESET: return E_NETRESET;
    case WSAECONNABORTED: return E_CONNABORTED;
    case WSAECONNRESET: return E_CONNRESET;
    case WSAENOBUFS: return E_NOBUFS;
    case WSAEISCONN: return E_ISCONN;
    case WSAENOTCONN: return E_NOTCONN;
    case WSAETIMEDOUT: return E_TIMEDOUT;
    case WSAECONNREFUSED: return E_CONNREFUSED;
    case WSAELOOP: return E_LOOP;
    case WSAENAMETOOLONG: return E_NAMETOOLONG;
    case WSAEHOSTDOWN: return E_HOSTDOWN;
    case WSAEHOSTUNREACH: return E_HOSTUNREACH;
    case WSAENOTEMPTY: return E_NOTEMPTY;
    default: return E_INTERNAL;
    }
}
int fail_wsa() {
    return fail(map_wsa(WSAGetLastError()));
}

std::shared_ptr<Socket> find_socket(int id) {
    std::scoped_lock lock{table_mutex};
    const auto it = sockets.find(id);
    return it == sockets.end() ? nullptr : it->second;
}

bool to_host(const void* guest, std::uint32_t length, sockaddr_in* host) {
    if (!guest || length < 8) {
        return false;
    }
    const auto* in = static_cast<const OrbisSockaddrIn*>(guest);
    if (in->sin_family != AF_INET_) {
        return false;
    }
    *host = {};
    host->sin_family = AF_INET;
    host->sin_port = in->sin_port;
    host->sin_addr.s_addr = in->sin_addr;
    return true;
}
void to_guest(const sockaddr_in& host, void* guest, std::uint32_t* length) {
    if (!guest) {
        return;
    }
    OrbisSockaddrIn out{};
    out.sin_len = sizeof(OrbisSockaddrIn);
    out.sin_family = AF_INET_;
    out.sin_port = host.sin_port;
    out.sin_addr = host.sin_addr.s_addr;
    const std::uint32_t capacity = length ? *length : sizeof(out);
    std::memcpy(guest, &out, capacity < sizeof(out) ? capacity : sizeof(out));
    if (length) {
        *length = sizeof(out);
    }
}

std::string endpoint(std::uint32_t address, std::uint16_t port_network) {
    return ctr_hle::online::format_address(address) + ":" + std::to_string(ntohs(port_network));
}

// Logged prefix of transferred bytes (hex, at most 96) for the phase 2 traffic study.
std::string hex_prefix(const void* data, int length) {
    static const char digits[] = "0123456789abcdef";
    std::string text;
    const auto* bytes = static_cast<const unsigned char*>(data);
    const int count = length < 96 ? length : 96;
    for (int i = 0; i < count; ++i) {
        text.push_back(digits[bytes[i] >> 4]);
        text.push_back(digits[bytes[i] & 15]);
    }
    return text;
}

void log_net(const char* function, long long result, const std::string& extra) {
    ctr_hle::log_call("libSceNet", function, result, extra.c_str());
}

bool ready(SOCKET handle, bool read) {
    WSAPOLLFD entry{handle, static_cast<SHORT>(read ? POLLRDNORM : POLLWRNORM), 0};
    return WSAPoll(&entry, 1, 0) > 0;
}
} // namespace

CTR_SYSV int* sceNetErrnoLoc() {
    return &net_errno;
}
CTR_HLE_REGISTER_ONLINE("HQOwnfMGipQ", "libSceNet", sceNetErrnoLoc);

CTR_SYSV int sceNetInit() {
    ensure_winsock();
    log_net("sceNetInit", 0, "");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("Nlev7Lg8k3A", "libSceNet", sceNetInit);

CTR_SYSV int sceNetTerm() {
    log_net("sceNetTerm", 0, "");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("cTGkc6-TBlI", "libSceNet", sceNetTerm);

CTR_SYSV std::uint16_t sceNetHtons(std::uint16_t value) {
    return htons(value);
}
CTR_HLE_REGISTER_ONLINE("iWQWrwiSt8A", "libSceNet", sceNetHtons);

CTR_SYSV std::uint16_t sceNetNtohs(std::uint16_t value) {
    return ntohs(value);
}
CTR_HLE_REGISTER_ONLINE("Rbvt+5Y2iEw", "libSceNet", sceNetNtohs);

CTR_SYSV const char* sceNetInetNtop(int af, const void* src, char* dst, std::uint32_t size) {
    if (!src || !dst) {
        net_errno = E_NOSPC;
        return nullptr;
    }
    const int family = af == AF_INET_ ? AF_INET : af == 28 ? AF_INET6 : -1;
    if (family < 0) {
        net_errno = E_AFNOSUPPORT;
        return nullptr;
    }
    const char* result = InetNtopA(family, src, dst, size);
    if (!result) {
        net_errno = E_NOSPC;
    }
    return result;
}
CTR_HLE_REGISTER_ONLINE("9vA2aW+CHuA", "libSceNet", sceNetInetNtop);

CTR_SYSV int sceNetSocket(const char* name, int family, int type, int protocol) {
    ensure_winsock();
    int host_type = 0, host_protocol = protocol;
    switch (type) {
    case SOCK_STREAM_:
    case SOCK_STREAM_P2P:
        host_type = SOCK_STREAM;
        host_protocol = IPPROTO_TCP;
        break;
    case SOCK_DGRAM_:
    case SOCK_DGRAM_P2P:
        host_type = SOCK_DGRAM;
        host_protocol = IPPROTO_UDP;
        break;
    default:
        break;
    }
    std::string extra = ",\"name\":\"" + std::string(name ? name : "") +
                        "\",\"family\":" + std::to_string(family) +
                        ",\"type\":" + std::to_string(type) +
                        ",\"protocol\":" + std::to_string(protocol);
    if (family != AF_INET_ || !host_type) {
        const int result = fail(family != AF_INET_ ? E_AFNOSUPPORT : E_PROTONOSUPPORT);
        log_net("sceNetSocket", result, extra + ",\"unsupported\":true");
        return result;
    }
    const SOCKET handle = socket(AF_INET, host_type, host_protocol);
    if (handle == INVALID_SOCKET) {
        const int result = fail_wsa();
        log_net("sceNetSocket", result, extra);
        return result;
    }
    auto entry = std::make_shared<Socket>();
    entry->handle = handle;
    entry->type = type;
    entry->name = name ? name : "";
    int id;
    {
        std::scoped_lock lock{table_mutex};
        id = next_id++;
        sockets[id] = entry;
    }
    log_net("sceNetSocket", id, extra);
    return id;
}
CTR_HLE_REGISTER_ONLINE("Q4qBuN-c0ZM", "libSceNet", sceNetSocket);

CTR_SYSV int sceNetSocketClose(int id) {
    std::shared_ptr<Socket> entry;
    {
        std::scoped_lock lock{table_mutex};
        const auto it = sockets.find(id);
        if (it != sockets.end()) {
            entry = it->second;
            sockets.erase(it);
        }
    }
    const int result = entry ? (closesocket(entry->handle) ? fail_wsa() : 0) : fail(E_BADF);
    log_net("sceNetSocketClose", result, ",\"socket\":" + std::to_string(id));
    return result;
}
CTR_HLE_REGISTER_ONLINE("45ggEzakPJQ", "libSceNet", sceNetSocketClose);

CTR_SYSV int sceNetBind(int id, const void* address, std::uint32_t length) {
    const auto entry = find_socket(id);
    sockaddr_in host{};
    int result;
    if (!entry) {
        result = fail(E_BADF);
    } else if (!to_host(address, length, &host)) {
        result = fail(E_INVAL);
    } else {
        // Remap only the well-known local UDP endpoint. Return its actual port from
        // getsockname so the original client advertises the reachable peer endpoint.
        // sendto/recvfrom preserve peer ports; applying our local offset there would
        // also redirect traffic meant for the server or another instance.
        if ((entry->type == SOCK_DGRAM_ || entry->type == SOCK_DGRAM_P2P) && ntohs(host.sin_port) == 3074) {
            const char* value = std::getenv("CTR_ONLINE_PORT_OFFSET");
            if (value && *value) {
                char* end = nullptr;
                const long offset = std::strtol(value, &end, 10);
                if (*end || offset < 0 || offset > 65535 - 3074) return fail(E_INVAL);
                host.sin_port = htons(static_cast<unsigned short>(3074 + offset));
            }
        }
        result = bind(entry->handle, reinterpret_cast<sockaddr*>(&host), sizeof(host)) ? fail_wsa()
                                                                                         : 0;
    }
    log_net("sceNetBind", result,
            ",\"socket\":" + std::to_string(id) + ",\"address\":\"" +
                endpoint(host.sin_addr.s_addr, host.sin_port) + "\"");
    return result;
}
CTR_HLE_REGISTER_ONLINE("bErx49PgxyY", "libSceNet", sceNetBind);

CTR_SYSV int sceNetListen(int id, int backlog) {
    const auto entry = find_socket(id);
    const int result = !entry ? fail(E_BADF) : listen(entry->handle, backlog) ? fail_wsa() : 0;
    log_net("sceNetListen", result, ",\"socket\":" + std::to_string(id));
    return result;
}
CTR_HLE_REGISTER_ONLINE("kOj1HiAGE54", "libSceNet", sceNetListen);

CTR_SYSV int sceNetAccept(int id, void* address, std::uint32_t* length) {
    const auto entry = find_socket(id);
    if (!entry) {
        return fail(E_BADF);
    }
    sockaddr_in host{};
    int host_length = sizeof(host);
    const SOCKET accepted = accept(entry->handle, reinterpret_cast<sockaddr*>(&host), &host_length);
    if (accepted == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        const int result = fail(map_wsa(error));
        if (error != WSAEWOULDBLOCK) {
            log_net("sceNetAccept", result, ",\"socket\":" + std::to_string(id));
        }
        return result;
    }
    to_guest(host, address, length);
    auto child = std::make_shared<Socket>();
    child->handle = accepted;
    child->type = entry->type;
    child->peer = host.sin_addr.s_addr;
    child->peer_port = host.sin_port;
    int child_id;
    {
        std::scoped_lock lock{table_mutex};
        child_id = next_id++;
        sockets[child_id] = child;
    }
    log_net("sceNetAccept", child_id,
            ",\"socket\":" + std::to_string(id) + ",\"peer\":\"" +
                endpoint(host.sin_addr.s_addr, host.sin_port) + "\"");
    return child_id;
}
CTR_HLE_REGISTER_ONLINE("PIWqhn9oSxc", "libSceNet", sceNetAccept);

CTR_SYSV int sceNetConnect(int id, const void* address, std::uint32_t length) {
    const auto entry = find_socket(id);
    sockaddr_in host{};
    int result;
    bool blocked = false;
    if (!entry) {
        result = fail(E_BADF);
    } else if (!to_host(address, length, &host)) {
        result = fail(E_INVAL);
    } else if (!ctr_hle::online::destination_allowed(host.sin_addr.s_addr)) {
        blocked = true;
        result = fail(E_HOSTUNREACH);
    } else {
        entry->peer = host.sin_addr.s_addr;
        entry->peer_port = host.sin_port;
        if (connect(entry->handle, reinterpret_cast<sockaddr*>(&host), sizeof(host)) == 0) {
            result = 0;
        } else {
            // Winsock reports EWOULDBLOCK where the PS4 reports EINPROGRESS for a
            // non-blocking connect (shadPS4 posix_sockets.cpp Connect).
            const int error = WSAGetLastError();
            result = fail(error == WSAEWOULDBLOCK ? E_INPROGRESS : map_wsa(error));
        }
    }
    log_net("sceNetConnect", result,
            ",\"socket\":" + std::to_string(id) + ",\"address\":\"" +
                endpoint(host.sin_addr.s_addr, host.sin_port) + "\"" +
                (blocked ? ",\"blocked_by_policy\":true" : ""));
    return result;
}
CTR_HLE_REGISTER_ONLINE("OXXX4mUk3uk", "libSceNet", sceNetConnect);

CTR_SYSV int sceNetGetsockname(int id, void* address, std::uint32_t* length) {
    const auto entry = find_socket(id);
    if (!entry) {
        return fail(E_BADF);
    }
    sockaddr_in host{};
    int host_length = sizeof(host);
    if (getsockname(entry->handle, reinterpret_cast<sockaddr*>(&host), &host_length)) {
        return fail_wsa();
    }
    to_guest(host, address, length);
    log_net("sceNetGetsockname", 0,
            ",\"socket\":" + std::to_string(id) + ",\"address\":\"" +
                endpoint(host.sin_addr.s_addr, host.sin_port) + "\"");
    return 0;
}
CTR_HLE_REGISTER_ONLINE("hoOAofhhRvE", "libSceNet", sceNetGetsockname);

namespace {
int send_common(const char* function, int id, const void* data, std::uint64_t length, int flags,
                const void* address, std::uint32_t address_length) {
    const auto entry = find_socket(id);
    if (!entry) {
        return fail(E_BADF);
    }
    sockaddr_in host{};
    const bool has_address = address != nullptr;
    if (has_address) {
        if (!to_host(address, address_length, &host)) {
            return fail(E_INVAL);
        }
        if (!ctr_hle::online::destination_allowed(host.sin_addr.s_addr)) {
            const int result = fail(E_HOSTUNREACH);
            log_net(function, result,
                    ",\"socket\":" + std::to_string(id) + ",\"address\":\"" +
                        endpoint(host.sin_addr.s_addr, host.sin_port) +
                        "\",\"blocked_by_policy\":true");
            return result;
        }
    }
    if ((flags & MSG_DONTWAIT_) && !ready(entry->handle, false)) {
        return fail(E_WOULDBLOCK);
    }
    const int size = length > 0x7fffffff ? 0x7fffffff : static_cast<int>(length);
    const int sent =
        has_address ? sendto(entry->handle, static_cast<const char*>(data), size, 0,
                             reinterpret_cast<sockaddr*>(&host), sizeof(host))
                    : send(entry->handle, static_cast<const char*>(data), size, 0);
    if (sent == SOCKET_ERROR) {
        const int result = fail_wsa();
        if (net_errno != E_WOULDBLOCK) {
            log_net(function, result, ",\"socket\":" + std::to_string(id));
        }
        return result;
    }
    const std::uint32_t to = has_address ? host.sin_addr.s_addr : entry->peer;
    const std::uint16_t to_port = has_address ? host.sin_port : entry->peer_port;
    log_net(function, sent,
            ",\"socket\":" + std::to_string(id) + ",\"to\":\"" + endpoint(to, to_port) +
                "\",\"bytes\":\"" + hex_prefix(data, sent) + "\"");
    return sent;
}

int receive_common(const char* function, int id, void* buffer, std::uint64_t length, int flags,
                   void* address, std::uint32_t* address_length) {
    const auto entry = find_socket(id);
    if (!entry) {
        return fail(E_BADF);
    }
    if ((flags & MSG_DONTWAIT_) && !ready(entry->handle, true)) {
        return fail(E_WOULDBLOCK);
    }
    int host_flags = 0;
    if (flags & MSG_PEEK_) {
        host_flags |= MSG_PEEK;
    }
    if ((flags & MSG_WAITALL_) && (entry->type == SOCK_STREAM_ || entry->type == SOCK_STREAM_P2P)) {
        host_flags |= MSG_WAITALL;
    }
    const int size = length > 0x7fffffff ? 0x7fffffff : static_cast<int>(length);
    sockaddr_in host{};
    int host_length = sizeof(host);
    const int received = recvfrom(entry->handle, static_cast<char*>(buffer), size, host_flags,
                                  reinterpret_cast<sockaddr*>(&host), &host_length);
    if (received == SOCKET_ERROR) {
        const int error = WSAGetLastError();
        // A datagram larger than the buffer: the BSD stack truncates and returns the length.
        if (error == WSAEMSGSIZE) {
            log_net(function, size, ",\"socket\":" + std::to_string(id) + ",\"truncated\":true");
            return size;
        }
        const int result = fail(map_wsa(error));
        if (net_errno != E_WOULDBLOCK) {
            log_net(function, result, ",\"socket\":" + std::to_string(id));
        }
        return result;
    }
    if (host.sin_family != AF_INET) {
        host.sin_addr.s_addr = entry->peer;
        host.sin_port = entry->peer_port;
    }
    if (address) {
        to_guest(host, address, address_length);
    }
    log_net(function, received,
            ",\"socket\":" + std::to_string(id) + ",\"from\":\"" +
                endpoint(host.sin_addr.s_addr, host.sin_port) + "\",\"bytes\":\"" +
                hex_prefix(buffer, received) + "\"");
    return received;
}
} // namespace

CTR_SYSV int sceNetSend(int id, const void* data, std::uint64_t length, int flags) {
    return send_common("sceNetSend", id, data, length, flags, nullptr, 0);
}
CTR_HLE_REGISTER_ONLINE("beRjXBn-z+o", "libSceNet", sceNetSend);

CTR_SYSV int sceNetSendto(int id, const void* data, std::uint64_t length, int flags,
                          const void* address, std::uint32_t address_length) {
    return send_common("sceNetSendto", id, data, length, flags, address, address_length);
}
CTR_HLE_REGISTER_ONLINE("gvD1greCu0A", "libSceNet", sceNetSendto);

CTR_SYSV int sceNetRecv(int id, void* buffer, std::uint64_t length, int flags) {
    return receive_common("sceNetRecv", id, buffer, length, flags, nullptr, nullptr);
}
CTR_HLE_REGISTER_ONLINE("9wO9XrMsNhc", "libSceNet", sceNetRecv);

CTR_SYSV int sceNetRecvfrom(int id, void* buffer, std::uint64_t length, int flags, void* address,
                            std::uint32_t* address_length) {
    return receive_common("sceNetRecvfrom", id, buffer, length, flags, address, address_length);
}
CTR_HLE_REGISTER_ONLINE("304ooNZxWDY", "libSceNet", sceNetRecvfrom);

namespace {
// Options stored per socket and never passed to Winsock (net.h, posix_sockets.cpp).
int* stored_option(Socket& entry, int level, int name) {
    if (level == LEVEL_SOCKET) {
        switch (name) {
        case SO_NBIO_: return &entry.nbio;
        case SO_CONNECTTIMEO_: return &entry.connect_timeout;
        case SO_REUSEPORT_: return &entry.reuseport;
        case SO_ONESBCAST_: return &entry.onesbcast;
        case SO_USECRYPTO_: return &entry.usecrypto;
        case SO_USESIGNATURE_: return &entry.usesignature;
        default: return nullptr;
        }
    }
    if (level == LEVEL_IP && name == IP_TTLCHK_) return &entry.ttlchk;
    if (level == LEVEL_IP && name == IP_MAXTTL_) return &entry.maxttl;
    if (level == LEVEL_TCP && name == TCP_MSS_TO_ADVERTISE_) return &entry.mss;
    return nullptr;
}
// Options whose PS4 number equals the Winsock number at the same level.
bool passthrough_option(int level, int name) {
    if (level == LEVEL_SOCKET) {
        return name == SO_REUSEADDR || name == SO_KEEPALIVE || name == SO_BROADCAST ||
               name == SO_LINGER || name == SO_SNDBUF || name == SO_RCVBUF;
    }
    if (level == LEVEL_IP) {
        return name == IP_TOS || name == IP_TTL || name == IP_MULTICAST_IF ||
               name == IP_MULTICAST_TTL || name == IP_MULTICAST_LOOP ||
               name == IP_ADD_MEMBERSHIP || name == IP_DROP_MEMBERSHIP;
    }
    if (level == LEVEL_TCP) {
        return name == TCP_NODELAY || name == TCP_MAXSEG;
    }
    return false;
}
int host_level(int level) {
    return level == LEVEL_SOCKET ? SOL_SOCKET : level == LEVEL_TCP ? IPPROTO_TCP : IPPROTO_IP;
}
std::string option_text(int id, int level, int name) {
    return ",\"socket\":" + std::to_string(id) + ",\"level\":" + std::to_string(level) +
           ",\"option\":" + std::to_string(name);
}
} // namespace

CTR_SYSV int sceNetSetsockopt(int id, int level, int name, const void* value,
                              std::uint32_t length) {
    const auto entry = find_socket(id);
    int result;
    if (!entry) {
        result = fail(E_BADF);
    } else if (!value) {
        result = fail(E_FAULT);
    } else if (int* stored = stored_option(*entry, level, name)) {
        if (length < sizeof(int)) {
            result = fail(E_INVAL);
        } else {
            std::memcpy(stored, value, sizeof(int));
            result = 0;
            if (level == LEVEL_SOCKET && name == SO_NBIO_) {
                u_long mode = *stored ? 1 : 0;
                result = ioctlsocket(entry->handle, FIONBIO, &mode) ? fail_wsa() : 0;
            } else if (level == LEVEL_SOCKET && name == SO_ONESBCAST_) {
                const BOOL on = *stored ? TRUE : FALSE;
                result = setsockopt(entry->handle, SOL_SOCKET, SO_BROADCAST,
                                    reinterpret_cast<const char*>(&on), sizeof(on))
                             ? fail_wsa()
                             : 0;
            }
        }
    } else if (level == LEVEL_SOCKET && (name == SO_SNDTIMEO_ || name == SO_RCVTIMEO_)) {
        if (length != sizeof(int)) {
            result = fail(E_FAULT);
        } else {
            // PS4 timeouts are microseconds; Winsock takes milliseconds.
            const DWORD milliseconds = static_cast<DWORD>(*static_cast<const int*>(value) / 1000);
            result = setsockopt(entry->handle, SOL_SOCKET,
                                name == SO_SNDTIMEO_ ? SO_SNDTIMEO : SO_RCVTIMEO,
                                reinterpret_cast<const char*>(&milliseconds), sizeof(milliseconds))
                         ? fail_wsa()
                         : 0;
        }
    } else if (passthrough_option(level, name)) {
        result = setsockopt(entry->handle, host_level(level), name,
                            static_cast<const char*>(value), static_cast<int>(length))
                     ? fail_wsa()
                     : 0;
    } else {
        result = fail(E_NOPROTOOPT);
        log_net("sceNetSetsockopt", result, option_text(id, level, name) + ",\"unsupported\":true");
        return result;
    }
    log_net("sceNetSetsockopt", result, option_text(id, level, name));
    return result;
}
CTR_HLE_REGISTER_ONLINE("2mKX2Spso7I", "libSceNet", sceNetSetsockopt);

CTR_SYSV int sceNetGetsockopt(int id, int level, int name, void* value, std::uint32_t* length) {
    const auto entry = find_socket(id);
    int result;
    if (!entry) {
        result = fail(E_BADF);
    } else if (!value || !length) {
        result = fail(E_FAULT);
    } else if (int* stored = stored_option(*entry, level, name)) {
        if (*length < sizeof(int)) {
            *length = sizeof(int);
            result = fail(E_FAULT);
        } else {
            std::memcpy(value, stored, sizeof(int));
            *length = sizeof(int);
            result = 0;
        }
    } else if (level == LEVEL_SOCKET && (name == SO_ERROR || name == SO_ERROR_EX_)) {
        int error = 0;
        int error_length = sizeof(error);
        result = getsockopt(entry->handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error),
                            &error_length)
                     ? fail_wsa()
                     : 0;
        const int mapped = error ? map_wsa(error) : 0;
        if (!result && *length >= sizeof(int)) {
            std::memcpy(value, &mapped, sizeof(int));
            *length = sizeof(int);
        }
        // SO_ERROR_EX returns the pending error as the call result (shadPS4
        // GetSocketOptions).
        if (!result && name == SO_ERROR_EX_) {
            result = mapped;
        }
    } else if (level == LEVEL_SOCKET && name == SO_TYPE) {
        if (*length < sizeof(int)) {
            result = fail(E_FAULT);
        } else {
            std::memcpy(value, &entry->type, sizeof(int));
            *length = sizeof(int);
            result = 0;
        }
    } else if (passthrough_option(level, name)) {
        int host_length = static_cast<int>(*length);
        result = getsockopt(entry->handle, host_level(level), name, static_cast<char*>(value),
                            &host_length)
                     ? fail_wsa()
                     : 0;
        *length = static_cast<std::uint32_t>(host_length);
    } else {
        result = fail(E_NOPROTOOPT);
        log_net("sceNetGetsockopt", result, option_text(id, level, name) + ",\"unsupported\":true");
        return result;
    }
    log_net("sceNetGetsockopt", result, option_text(id, level, name));
    return result;
}
CTR_HLE_REGISTER_ONLINE("xphrZusl78E", "libSceNet", sceNetGetsockopt);

// Resolver. Async resolutions complete when the owning epoll is waited on (shadPS4
// net_resolver.cpp); names follow the R8 policy in online_config.h.
namespace {
int resolve_now(const std::string& hostname, std::uint32_t* output) {
    const char* reason = "";
    std::uint32_t address = 0;
    const bool allowed = ctr_hle::online::resolve(hostname.c_str(), &address, &reason);
    if (allowed && output) {
        *output = address;
    }
    const int result = allowed ? 0 : kErrorBase | E_RESOLVER_ENOHOST;
    log_net("sceNetResolverStartNtoa.resolution", result,
            ",\"hostname\":\"" + hostname + "\",\"policy\":\"" + reason + "\"" +
                (allowed ? ",\"address\":\"" + ctr_hle::online::format_address(address) + "\""
                         : std::string()));
    return result;
}
} // namespace

CTR_SYSV int sceNetResolverCreate(const char* name, int pool, int flags) {
    if (flags) {
        return fail(E_INVAL);
    }
    auto entry = std::make_shared<Resolver>();
    entry->name = name ? name : "";
    int id;
    {
        std::scoped_lock lock{table_mutex};
        id = next_id++;
        resolvers[id] = entry;
    }
    log_net("sceNetResolverCreate", id, ",\"name\":\"" + entry->name + "\"");
    return id;
}
CTR_HLE_REGISTER_ONLINE("C4UgDHHPvdw", "libSceNet", sceNetResolverCreate);

CTR_SYSV int sceNetResolverDestroy(int id) {
    std::scoped_lock lock{table_mutex};
    if (!resolvers.erase(id)) {
        return fail(E_BADF);
    }
    return 0;
}
CTR_HLE_REGISTER_ONLINE("kJlYH5uMAWI", "libSceNet", sceNetResolverDestroy);

CTR_SYSV int sceNetResolverAbort(int id, int flags) {
    std::scoped_lock lock{table_mutex};
    const auto it = resolvers.find(id);
    if (it == resolvers.end()) {
        return fail(E_BADF);
    }
    it->second->pending = false;
    return 0;
}
CTR_HLE_REGISTER_ONLINE("AzqoBha7js4", "libSceNet", sceNetResolverAbort);

CTR_SYSV int sceNetResolverStartNtoa(int id, const char* hostname, std::uint32_t* address,
                                     int timeout, int retry, int flags) {
    std::shared_ptr<Resolver> entry;
    {
        std::scoped_lock lock{table_mutex};
        const auto it = resolvers.find(id);
        if (it != resolvers.end()) {
            entry = it->second;
        }
    }
    if (!entry) {
        return fail(E_BADF);
    }
    const std::string name = hostname ? hostname : "";
    if (flags & RESOLVER_ASYNC_) {
        std::scoped_lock lock{table_mutex};
        if (entry->pending) {
            return fail(E_RESOLVER_EBUSY);
        }
        entry->pending = true;
        entry->hostname = name;
        entry->output = address;
        log_net("sceNetResolverStartNtoa", 0, ",\"hostname\":\"" + name + "\",\"async\":true");
        return 0;
    }
    const int result = resolve_now(name, address);
    entry->error = result;
    if (result) {
        net_errno = E_RESOLVER_ENOHOST;
    }
    return result;
}
CTR_HLE_REGISTER_ONLINE("Nd91WaWmG2w", "libSceNet", sceNetResolverStartNtoa);

// Epoll over WSAPoll.
CTR_SYSV int sceNetEpollCreate(const char* name, int flags) {
    if (flags) {
        return fail(E_INVAL);
    }
    auto entry = std::make_shared<Epoll>();
    entry->name = name ? name : "";
    int id;
    {
        std::scoped_lock lock{table_mutex};
        id = next_id++;
        epolls[id] = entry;
    }
    log_net("sceNetEpollCreate", id, ",\"name\":\"" + entry->name + "\"");
    return id;
}
CTR_HLE_REGISTER_ONLINE("SF47kB2MNTo", "libSceNet", sceNetEpollCreate);

CTR_SYSV int sceNetEpollDestroy(int id) {
    std::scoped_lock lock{table_mutex};
    if (!epolls.erase(id)) {
        return fail(E_BADF);
    }
    return 0;
}
CTR_HLE_REGISTER_ONLINE("Inp1lfL+Jdw", "libSceNet", sceNetEpollDestroy);

CTR_SYSV int sceNetEpollControl(int id, int operation, int target, const OrbisEpollEvent* event) {
    std::scoped_lock lock{table_mutex};
    const auto it = epolls.find(id);
    if (it == epolls.end()) {
        return fail(E_BADF);
    }
    auto& entries = it->second->entries;
    auto found = entries.begin();
    while (found != entries.end() && found->first != target) {
        ++found;
    }
    const bool known = sockets.count(target) || resolvers.count(target);
    switch (operation) {
    case EPOLL_CTL_ADD_:
        if (!event) return fail(E_INVAL);
        if (found != entries.end()) return fail(E_EXIST);
        if (!known) return fail(E_BADF);
        entries.emplace_back(target, *event);
        return 0;
    case EPOLL_CTL_MOD_:
        if (!event) return fail(E_INVAL);
        if (found == entries.end()) return fail(E_BADF);
        found->second = *event;
        return 0;
    case EPOLL_CTL_DEL_:
        if (found == entries.end()) return fail(E_BADF);
        entries.erase(found);
        return 0;
    default:
        return fail(E_INVAL);
    }
}
CTR_HLE_REGISTER_ONLINE("ZVw46bsasAk", "libSceNet", sceNetEpollControl);

// timeout in microseconds, negative = infinite (shadPS4 sceNetEpollWait).
CTR_SYSV int sceNetEpollWait(int id, OrbisEpollEvent* events, int max_events, int timeout) {
    if (!events || max_events <= 0) {
        return fail(E_INVAL);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(timeout);
    for (;;) {
        std::vector<WSAPOLLFD> polled;
        std::vector<std::pair<int, OrbisEpollEvent>> polled_entries;
        std::vector<std::pair<std::shared_ptr<Resolver>, OrbisEpollEvent>> pending;
        std::vector<int> pending_ids;
        {
            std::scoped_lock lock{table_mutex};
            const auto it = epolls.find(id);
            if (it == epolls.end()) {
                return fail(E_BADF);
            }
            for (const auto& [target, event] : it->second->entries) {
                if (const auto socket_it = sockets.find(target); socket_it != sockets.end()) {
                    SHORT requested = 0;
                    if (event.events & EPOLLIN_) requested |= POLLRDNORM;
                    if (event.events & EPOLLOUT_) requested |= POLLWRNORM;
                    polled.push_back({socket_it->second->handle, requested, 0});
                    polled_entries.emplace_back(target, event);
                } else if (const auto resolver_it = resolvers.find(target);
                           resolver_it != resolvers.end() && resolver_it->second->pending) {
                    resolver_it->second->pending = false;
                    pending.emplace_back(resolver_it->second, event);
                    pending_ids.push_back(target);
                }
            }
        }
        int count = 0;
        for (std::size_t i = 0; i < pending.size(); ++i) {
            auto& resolver = *pending[i].first;
            resolver.error = resolve_now(resolver.hostname, resolver.output);
            if (resolver.error || count == max_events) {
                continue; // failed resolutions are not reported (shadPS4 sceNetEpollWait)
            }
            events[count].events = EPOLLDESCID_;
            events[count].pad = 0;
            events[count].ident = static_cast<std::uint64_t>(pending_ids[i]);
            events[count].data = pending[i].second.data;
            ++count;
        }
        if (!polled.empty() && count < max_events) {
            if (WSAPoll(polled.data(), static_cast<ULONG>(polled.size()), 0) == SOCKET_ERROR) {
                return fail_wsa();
            }
            for (std::size_t i = 0; i < polled.size() && count < max_events; ++i) {
                const SHORT revents = polled[i].revents;
                std::uint32_t out = 0;
                if (revents & (POLLRDNORM | POLLHUP)) out |= EPOLLIN_;
                if (revents & POLLWRNORM) out |= EPOLLOUT_;
                if (revents & POLLERR) out |= EPOLLERR_;
                if (revents & POLLHUP) out |= EPOLLHUP_;
                if (!out) {
                    continue;
                }
                events[count].events = out;
                events[count].pad = 0;
                events[count].ident = static_cast<std::uint64_t>(polled_entries[i].first);
                events[count].data = polled_entries[i].second.data;
                ++count;
            }
        }
        if (count || timeout == 0 ||
            (timeout > 0 && std::chrono::steady_clock::now() >= deadline)) {
            return count;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
CTR_HLE_REGISTER_ONLINE("drjIbDbA7UQ", "libSceNet", sceNetEpollWait);
