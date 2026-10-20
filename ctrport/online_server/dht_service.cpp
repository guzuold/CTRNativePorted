// SPDX-License-Identifier: GPL-2.0-or-later
#include "dht_service.hpp"
#include "../dht/dht.h"
#include "crypto.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <windows.h>
#include <bcrypt.h>

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

extern "C" {

int dht_gettimeofday(struct timeval *tv, struct timezone *tz) {
    (void)tz;
    if (tv) {
        FILETIME ft;
        GetSystemTimePreciseAsFileTime(&ft);
        ULARGE_INTEGER li;
        li.LowPart = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        const unsigned __int64 kEpochDiff = 116444736000000000ULL;
        if (li.QuadPart >= kEpochDiff) {
            unsigned __int64 diff = li.QuadPart - kEpochDiff;
            tv->tv_sec = static_cast<long>(diff / 10000000ULL);
            tv->tv_usec = static_cast<long>((diff % 10000000ULL) / 10ULL);
        } else {
            tv->tv_sec = 0;
            tv->tv_usec = 0;
        }
    }
    return 0;
}

int dht_sendto(int sockfd, const void *buf, int len, int flags,
               const struct sockaddr *to, int tolen) {
    return sendto(static_cast<SOCKET>(sockfd), reinterpret_cast<const char*>(buf), len, flags, to, tolen);
}

int dht_blacklisted(const struct sockaddr *sa, int salen) {
    (void)sa; (void)salen;
    return 0;
}

void dht_hash(void *hash_return, int hash_size,
              const void *v1, int len1,
              const void *v2, int len2,
              const void *v3, int len3) {
    static BCRYPT_ALG_HANDLE alg = nullptr;
    if (!alg) BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0);
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        if (v1 && len1 > 0) BCryptHashData(hash, const_cast<PUCHAR>(reinterpret_cast<const unsigned char*>(v1)), static_cast<ULONG>(len1), 0);
        if (v2 && len2 > 0) BCryptHashData(hash, const_cast<PUCHAR>(reinterpret_cast<const unsigned char*>(v2)), static_cast<ULONG>(len2), 0);
        if (v3 && len3 > 0) BCryptHashData(hash, const_cast<PUCHAR>(reinterpret_cast<const unsigned char*>(v3)), static_cast<ULONG>(len3), 0);
        unsigned char digest[20] = {};
        BCryptFinishHash(hash, digest, 20, 0);
        BCryptDestroyHash(hash);
        const int copy_len = hash_size < 20 ? hash_size : 20;
        std::memcpy(hash_return, digest, static_cast<std::size_t>(copy_len));
        if (hash_size > 20) std::memset(reinterpret_cast<char*>(hash_return) + 20, 0, static_cast<std::size_t>(hash_size - 20));
    }
}

int dht_random_bytes(void *buf, size_t size) {
    return BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(buf), static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0
        ? static_cast<int>(size) : -1;
}

} // extern "C"

namespace ctr_online {

namespace {
std::atomic<bool> dht_owned{false};

struct WsaInit {
    WsaInit() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); }
    ~WsaInit() { WSACleanup(); }
};

const char* const kDefaultBootstrapNodes[] = {
    "router.bittorrent.com:6881",
    "dht.transmissionbt.com:6881",
    "router.utorrent.com:6881",
    "dht.libtorrent.org:25401"
};

std::string format_endpoint(const sockaddr_in& sin) {
    char ip[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, const_cast<in_addr*>(&sin.sin_addr), ip, sizeof(ip));
    return std::string(ip) + ":" + std::to_string(ntohs(sin.sin_port));
}

bool resolve_node(const std::string& endpoint, sockaddr_in& out) {
    const std::size_t colon = endpoint.rfind(':');
    if (colon == std::string::npos) return false;
    const std::string host = endpoint.substr(0, colon);
    const int port = std::atoi(endpoint.substr(colon + 1).c_str());
    if (port <= 0 || port > 65535) return false;

    std::memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port = htons(static_cast<std::uint16_t>(port));

    if (inet_pton(AF_INET, host.c_str(), &out.sin_addr) == 1) return true;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0 && res) {
        out.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
        freeaddrinfo(res);
        return true;
    }
    return false;
}

} // namespace

struct ActiveSearch {
    Bytes info_hash;
    std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
    bool search_started = false;
    std::set<std::string> results;
    bool done = false;
    std::mutex mutex;
    std::condition_variable cv;
};

struct BootstrapContext {
    std::atomic<bool> stop_requested{false};
    std::mutex mutex;
    std::function<void(const sockaddr_in&)> on_node_resolved;
};

struct DhtService::Impl {
    WsaInit wsa;
    bool owns_global_ = false;
    std::mutex search_mutex_;
    DhtStats cached_stats_;
    SOCKET socket_ = INVALID_SOCKET;
    std::uint16_t bound_port_ = 0;
    std::filesystem::path cache_path_;
    std::vector<std::string> extra_nodes_;

    std::atomic<bool> running_{false};
    std::thread worker_;

    // Commands to worker
    std::mutex cmd_mutex_;
    std::vector<sockaddr_in> pending_pings_;
    bool announce_changed_ = false;
    bool have_announce_ = false;
    Bytes announce_hash_;
    std::uint16_t announce_port_ = 0;
    std::chrono::steady_clock::time_point last_announce_{};
    struct ExtraAnnounce { std::uint16_t port = 0; std::chrono::steady_clock::time_point last{}; bool due = true; };
    std::map<Bytes, ExtraAnnounce> extra_announces_;


    std::map<Bytes, std::shared_ptr<ActiveSearch>> searches_;
    std::shared_ptr<BootstrapContext> bootstrap_ctx_;

    void wake() {
        if (socket_ != INVALID_SOCKET && bound_port_ != 0) {
            sockaddr_in loopback{};
            loopback.sin_family = AF_INET;
            loopback.sin_port = htons(bound_port_);
            inet_pton(AF_INET, "127.0.0.1", &loopback.sin_addr);
            char b = 0;
            sendto(socket_, &b, 1, 0, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback));
        }
    }

    static void dht_callback(void* closure, int event, const unsigned char* info_hash,
                             const void* data, size_t data_len) {
        auto* self = static_cast<Impl*>(closure);
        if (!self || !info_hash) return;
        std::shared_ptr<ActiveSearch> search;
        {
            std::lock_guard<std::mutex> lock(self->cmd_mutex_);
            const auto it = self->searches_.find(Bytes(info_hash, info_hash + 20));
            if (it == self->searches_.end()) return;
            search = it->second;
        }
        std::lock_guard<std::mutex> lock(search->mutex);
        if (event == DHT_EVENT_VALUES) {
            const auto* bytes = static_cast<const unsigned char*>(data);
            const std::size_t count = data_len / 6;
            for (std::size_t i = 0; i < count; ++i) {
                sockaddr_in sin{};
                sin.sin_family = AF_INET;
                std::memcpy(&sin.sin_addr, bytes + i * 6, 4);
                std::memcpy(&sin.sin_port, bytes + i * 6 + 4, 2);
                search->results.insert(format_endpoint(sin));
            }
            search->cv.notify_all();
        } else if (event == DHT_EVENT_SEARCH_DONE) {
            search->done = true;
            search->cv.notify_all();
        }
    }

    void load_cache() {
        if (cache_path_.empty() || !std::filesystem::exists(cache_path_)) return;
        std::ifstream file(cache_path_, std::ios::binary);
        if (!file.is_open()) return;
        std::uint32_t count = 0;
        file.read(reinterpret_cast<char*>(&count), sizeof(count));
        for (std::uint32_t i = 0; i < count && i < 500; ++i) {
            sockaddr_in sin{};
            file.read(reinterpret_cast<char*>(&sin), sizeof(sin));
            if (!file) break;
            if (sin.sin_family == AF_INET && sin.sin_port != 0) {
                dht_ping_node(reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
            }
        }
    }

    void save_cache() {
        if (cache_path_.empty()) return;
        std::error_code ec;
        std::filesystem::create_directories(cache_path_.parent_path(), ec);
        sockaddr_in nodes[500];
        int num = 500, num6 = 0;
        if (dht_get_nodes(nodes, &num, nullptr, &num6) >= 0 && num > 0) {
            std::ofstream file(cache_path_, std::ios::binary | std::ios::trunc);
            const std::uint32_t count = static_cast<std::uint32_t>(num);
            file.write(reinterpret_cast<const char*>(&count), sizeof(count));
            for (int i = 0; i < num; ++i) {
                file.write(reinterpret_cast<const char*>(&nodes[i]), sizeof(nodes[i]));
            }
        }
    }

    void queue_ping(const sockaddr_in& sin) {
        {
            std::lock_guard<std::mutex> lock(cmd_mutex_);
            pending_pings_.push_back(sin);
        }
        wake();
    }

    void worker_loop() {
        unsigned char my_id[20];
        dht_random_bytes(my_id, 20);

        if (dht_init(static_cast<int>(socket_), -1, my_id, reinterpret_cast<const unsigned char*>("CTR1")) < 0) {
            return;
        }

        load_cache();

        // Resolve and ping bootstrap nodes asynchronously to prevent blocking worker startup or shutdown
        std::thread([ctx = bootstrap_ctx_, extra_nodes = extra_nodes_] {
            // Its own Winsock reference: the service may be destroyed (WSACleanup) while this
            // detached thread is still inside getaddrinfo, which crashed in the RPC runtime.
            WsaInit wsa;
            if (!ctx) return;
            auto deliver = [&](const sockaddr_in& sin) {
                if (ctx->stop_requested) return;
                std::lock_guard<std::mutex> lock(ctx->mutex);
                if (ctx->on_node_resolved) {
                    ctx->on_node_resolved(sin);
                }
            };
            if (!extra_nodes.empty()) {
                for (const std::string& extra : extra_nodes) {
                    if (ctx->stop_requested) return;
                    sockaddr_in sin{};
                    if (resolve_node(extra, sin)) {
                        deliver(sin);
                    }
                }
            } else {
                for (const char* node_str : kDefaultBootstrapNodes) {
                    if (ctx->stop_requested) return;
                    sockaddr_in sin{};
                    if (resolve_node(node_str, sin)) {
                        deliver(sin);
                    }
                }
            }
        }).detach();

        time_t tosleep = 0;
        unsigned char buf[4096];

        while (running_) {
            // Process pending pings
            std::vector<sockaddr_in> pings;
            std::vector<std::shared_ptr<ActiveSearch>> search_commands;
            bool announce_now = false;
            std::vector<std::pair<Bytes, std::uint16_t>> extra_due;
            Bytes announce_h;
            std::uint16_t announce_p = 0;

            {
                std::lock_guard<std::mutex> lock(cmd_mutex_);
                pings = std::move(pending_pings_);
                pending_pings_.clear();
                for (const auto& item : searches_) search_commands.push_back(item.second);

                const auto now = std::chrono::steady_clock::now();
                if (have_announce_ && (announce_changed_ || now - last_announce_ > std::chrono::minutes(2))) {
                    announce_now = true;
                    announce_changed_ = false;
                    announce_h = announce_hash_;
                    announce_p = announce_port_;
                    last_announce_ = now;
                }
                for (auto& [hash, extra] : extra_announces_) {
                    if (!extra.due && now - extra.last < std::chrono::minutes(2)) continue;
                    extra.due = false;
                    extra.last = now;
                    extra_due.emplace_back(hash, extra.port);
                }
            }

            for (const auto& sin : pings) {
                dht_ping_node(reinterpret_cast<const sockaddr*>(&sin), sizeof(sin));
            }

            if (announce_now && announce_h.size() == 20) {
                dht_search(announce_h.data(), static_cast<int>(announce_p), AF_INET, nullptr, nullptr);
            }
            for (const auto& [hash, port] : extra_due) dht_search(hash.data(), static_cast<int>(port), AF_INET, nullptr, nullptr);

            for (const auto& search_copy : search_commands) {
                bool trigger = false;
                {
                    std::lock_guard<std::mutex> slock(search_copy->mutex);
                    if (!search_copy->search_started && search_copy->info_hash.size() == 20) {
                        search_copy->search_started = true;
                        trigger = true;
                    }
                }
                if (trigger) {
                                        // Looking up an announced key must not cancel its publication.
                    int port = 0;
                    {
                        std::lock_guard<std::mutex> lock(cmd_mutex_);
                        if (have_announce_ && announce_hash_ == search_copy->info_hash) port = announce_port_;
                        else if (const auto it = extra_announces_.find(search_copy->info_hash); it != extra_announces_.end()) port = it->second.port;
                    }
                    dht_search(search_copy->info_hash.data(), port, AF_INET, dht_callback, this);
                }
            }

            {
                std::lock_guard<std::mutex> lock(cmd_mutex_);
                dht_nodes(AF_INET, &cached_stats_.good_nodes, &cached_stats_.dubious_nodes,
                          &cached_stats_.cached_nodes, &cached_stats_.incoming_nodes);
            }
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(socket_, &readfds);

            timeval tv{};
            tv.tv_sec = static_cast<long>(std::max<time_t>(0, std::min<time_t>(tosleep, 1)));
            tv.tv_usec = 100000; // 100ms

            const int rc = select(static_cast<int>(socket_) + 1, &readfds, nullptr, nullptr, &tv);

            if (!running_) break;

            if (rc > 0 && FD_ISSET(socket_, &readfds)) {
                sockaddr_storage from{};
                int fromlen = sizeof(from);
                const int n = recvfrom(socket_, reinterpret_cast<char*>(buf), sizeof(buf) - 1, 0,
                                       reinterpret_cast<sockaddr*>(&from), &fromlen);
                if (n > 1) { // >1 byte to filter out internal 1-byte wakeups
                    buf[n] = '\0';
                    dht_periodic(buf, static_cast<size_t>(n), reinterpret_cast<sockaddr*>(&from), fromlen,
                                 &tosleep, dht_callback, this);
                } else {
                    dht_periodic(nullptr, 0, nullptr, 0, &tosleep,
                                 dht_callback, this);
                }
            } else {
                dht_periodic(nullptr, 0, nullptr, 0, &tosleep,
                             dht_callback, this);
            }
        }

        save_cache();
        dht_uninit();
    }
};

DhtService::DhtService() : impl_(std::make_unique<Impl>()) {}
DhtService::~DhtService() { stop(); }

bool DhtService::start(std::uint16_t port, const std::filesystem::path& cache_path,
                       const std::vector<std::string>& extra_bootstrap_nodes) {
    stop();

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return false;

    u_long nonblock = 1;
    ioctlsocket(s, FIONBIO, &nonblock);

    // Disable UDP connection resets on Windows
    BOOL report = FALSE;
    DWORD ignored = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &report, sizeof(report), nullptr, 0, &ignored, nullptr, nullptr);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        if (port != 0) {
            addr.sin_port = 0;
            if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
                closesocket(s);
                return false;
            }
        } else {
            closesocket(s);
            return false;
        }
    }

    int addrlen = sizeof(addr);
    getsockname(s, reinterpret_cast<sockaddr*>(&addr), &addrlen);
    bool expected = false;
    if (!dht_owned.compare_exchange_strong(expected, true)) { closesocket(s); return false; }
    impl_->owns_global_ = true;
    impl_->socket_ = s;
    impl_->bound_port_ = ntohs(addr.sin_port);
    impl_->cache_path_ = cache_path;
    impl_->extra_nodes_ = extra_bootstrap_nodes;
    impl_->running_ = true;

    impl_->bootstrap_ctx_ = std::make_shared<BootstrapContext>();
    impl_->bootstrap_ctx_->on_node_resolved = [this](const sockaddr_in& sin) {
        impl_->queue_ping(sin);
    };

    impl_->worker_ = std::thread([this] { impl_->worker_loop(); });
    return true;
}

void DhtService::stop() {
    if (impl_->bootstrap_ctx_) {
        impl_->bootstrap_ctx_->stop_requested = true;
        std::lock_guard<std::mutex> lock(impl_->bootstrap_ctx_->mutex);
        impl_->bootstrap_ctx_->on_node_resolved = nullptr;
    }
    if (impl_->running_) {
        impl_->running_ = false;
        impl_->wake();
        if (impl_->worker_.joinable()) {
            impl_->worker_.join();
        }
    }
    if (impl_->socket_ != INVALID_SOCKET) {
        closesocket(impl_->socket_);
        impl_->socket_ = INVALID_SOCKET;
    }
    impl_->bound_port_ = 0;
    if (impl_->owns_global_) { impl_->owns_global_ = false; dht_owned = false; }
}

bool DhtService::is_running() const {
    return impl_->running_;
}

bool DhtService::ping_node(const std::string& ip, std::uint16_t port) {
    if (!impl_->running_) return false;
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &sin.sin_addr) != 1) return false;

    {
        std::lock_guard<std::mutex> lock(impl_->cmd_mutex_);
        impl_->pending_pings_.push_back(sin);
    }
    impl_->wake();
    return true;
}

void DhtService::set_announce(const Bytes& info_hash_20, std::uint16_t contact_tcp_port) {
    if (info_hash_20.size() != 20) return;
    {
        std::lock_guard<std::mutex> lock(impl_->cmd_mutex_);
        impl_->have_announce_ = true;
        impl_->announce_hash_ = info_hash_20;
        impl_->announce_port_ = contact_tcp_port;
        impl_->announce_changed_ = true;
    }
    impl_->wake();
}

void DhtService::add_announce(const Bytes& info_hash_20, std::uint16_t contact_tcp_port) {
    if (info_hash_20.size() != 20) return;
    {
        std::lock_guard<std::mutex> lock(impl_->cmd_mutex_);
        auto& extra = impl_->extra_announces_[info_hash_20];
        extra.port = contact_tcp_port;
        extra.due = true;
    }
    impl_->wake();
}

void DhtService::remove_announce(const Bytes& info_hash_20) {
    std::lock_guard<std::mutex> lock(impl_->cmd_mutex_);
    impl_->extra_announces_.erase(info_hash_20);
}

void DhtService::clear_announce() {
    {
        std::lock_guard<std::mutex> lock(impl_->cmd_mutex_);
        impl_->have_announce_ = false;
        impl_->announce_hash_.clear();
        impl_->announce_port_ = 0;
        impl_->announce_changed_ = false;
    }
    impl_->wake();
}

std::vector<std::string> DhtService::search(const Bytes& info_hash_20, int timeout_ms) {
    std::lock_guard<std::mutex> serial(impl_->search_mutex_);
    if (!impl_->running_ || info_hash_20.size() != 20) return {};

    std::shared_ptr<ActiveSearch> active;
    {
        std::lock_guard<std::mutex> lock(impl_->cmd_mutex_);
        const auto now = std::chrono::steady_clock::now();
        for (auto it = impl_->searches_.begin(); it != impl_->searches_.end();)
            it = now - it->second->created > std::chrono::minutes(2) ? impl_->searches_.erase(it) : std::next(it);
        auto it = impl_->searches_.find(info_hash_20);
        if (it != impl_->searches_.end()) {
            const auto previous = it->second;
            std::lock_guard<std::mutex> search_lock(previous->mutex);
            if (previous->done && previous->results.empty() && now - previous->created > std::chrono::seconds(10))
                impl_->searches_.erase(it);
        }
        if (!impl_->searches_.count(info_hash_20) && impl_->searches_.size() >= 128)
            impl_->searches_.erase(impl_->searches_.begin());
        auto& slot = impl_->searches_[info_hash_20];
        if (!slot) { slot = std::make_shared<ActiveSearch>(); slot->info_hash = info_hash_20; }
        active = slot;

    }
    // Timeout ends only the wait, not the search. Late results remain keyed by hash.
    impl_->wake();

    std::unique_lock<std::mutex> ulock(active->mutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    // The worker owns all dht_search calls; callers only wait on their own hash.
    while (!active->done && active->results.empty()) {
        if (active->cv.wait_until(ulock, deadline) == std::cv_status::timeout) {
            break;
        }
        if (!active->results.empty()) {
            // Once we have candidates, we can either return or wait for more
            break;
        }
    }

    std::vector<std::string> out(active->results.begin(), active->results.end());
    return out;
}

DhtStats DhtService::stats() const {
    DhtStats s;
    s.bound_port = impl_->bound_port_;
    {
        std::lock_guard<std::mutex> lock(impl_->cmd_mutex_);
        s = impl_->cached_stats_;
        s.bound_port = impl_->bound_port_;
        s.announcing = impl_->have_announce_;
    }
    return s;
}

} // namespace ctr_online
