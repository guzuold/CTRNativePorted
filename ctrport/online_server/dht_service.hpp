// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ctr_online {

using Bytes = std::vector<unsigned char>;

struct DhtStats {
    int good_nodes = 0;
    int dubious_nodes = 0;
    int cached_nodes = 0;
    int incoming_nodes = 0;
    std::uint16_t bound_port = 0;
    bool announcing = false;
};

class DhtService {
public:
    DhtService();
    ~DhtService();
    DhtService(const DhtService&) = delete;
    DhtService& operator=(const DhtService&) = delete;

    // Starts the DHT background worker on the specified UDP port (0 = automatic/ephemeral).
    // cache_path: path to dht_nodes.dat cache file (e.g. runtime/user/online/dht_nodes.dat).
    // extra_bootstrap_nodes: optional additional "ip:port" or "host:port" bootstrap nodes (useful for tests).
    bool start(std::uint16_t port, const std::filesystem::path& cache_path,
               const std::vector<std::string>& extra_bootstrap_nodes = {});

    // Stops the worker cleanly, saves good nodes to cache, uninitializes dht.
    void stop();

    bool is_running() const;

    // Adds a node to ping (e.g. for bootstrapping or local tests).
    bool ping_node(const std::string& ip, std::uint16_t port);

    // Announces that this host serves contact service on contact_tcp_port for info_hash_20.
    // Call only while the host server is active.
    void set_announce(const Bytes& info_hash_20, std::uint16_t contact_tcp_port);

    // Clears active announce.
    void clear_announce();

    // Further keys announced alongside the main one (e.g. the public matchmaking key), each
    // renewed every 15 minutes; remove_announce stops renewing one.
    void add_announce(const Bytes& info_hash_20, std::uint16_t contact_tcp_port);
    void remove_announce(const Bytes& info_hash_20);

    // Searches the DHT for info_hash_20, collecting candidate "ip:port" values.
    // Blocks the caller until search completes or timeout_ms expires. Call off UI thread.
    std::vector<std::string> search(const Bytes& info_hash_20, int timeout_ms = 8000);

    DhtStats stats() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ctr_online
