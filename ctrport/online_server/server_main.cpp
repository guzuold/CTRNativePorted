// SPDX-License-Identifier: GPL-2.0-or-later
// Standalone project server: ctr_online_server.exe (same options as the Python reference
// tools/online/ctr_online_server.py, plus --upnp and --auto-public).
//   ctr_online_server.exe --public-host <public IPv4> [--upnp]   internet, one PC hosting
//   ctr_online_server.exe --auto-public --upnp                     public address from the router
//   ctr_online_server.exe                                          loopback development layout
//   ctr_online_server.exe --set-balance <account|online ID> <currency> <amount>
//       testing: sets an account's balance (0 removes it) in runtime/user/online/marketplace.txt
//       and exits; a running server reads the change at the game's next balance request.
//       Retail v01.21 currencies: 10 Wumpa Coins, 11 Nitro Boost, 12 Free Nitro Boost.
// Stop with Ctrl+C (the router mappings opened with --upnp are removed).
#include "server.hpp"
#include "friend_store.hpp"
#include "stores.hpp"
#include "launcher_online.hpp"
#include "upnp.hpp"
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_stop{false};
BOOL WINAPI on_console(DWORD) {
    g_stop = true;
    return TRUE;
}
std::vector<std::uint16_t> port_list(const std::string& text) {
    std::vector<std::uint16_t> ports;
    std::stringstream in(text);
    std::string item;
    while (std::getline(in, item, ','))
        if (!item.empty()) ports.push_back(static_cast<std::uint16_t>(std::stoi(item)));
    return ports;
}
void usage() {
    std::puts(
        "usage: ctr_online_server [--host 0.0.0.0] [--http 8780] [--tcp 3075] [--udp 3075,3478,3479]\n"
        "                         [--peer-host 127.0.0.2] [--stun-secondary-host 127.0.0.3] [--stun-alternate-port 3076]\n"
        "                         [--public-host IPv4 | --auto-public] [--discovery-port 3078] [--relay-port 3082 (0: off)]\n"
        "                         [--upnp] [--upnp-probe] [--upnp-check]\n"
        "                         [--upnp-location URL] [--root DIR] [--match-hub CODE@IPv4[:PORT]]\n"
        "                         [--friend-network [--local-name ONLINE_ID]]\n"
        "       ctr_online_server --set-balance <account|online ID> <currency> <amount> [--root DIR]\n"
        "       ctr_online_server --friend-code [--root DIR]\n"
        "       ctr_online_server --add-friend <code> <IPv4:port> [--root DIR]");
}
} // namespace

int main(int argc, char** argv) {
    using namespace ctr_online;
    ServerConfig config;
    bool upnp = false, auto_public = false, probe = false, check = false;
    std::string location;
    std::vector<std::string> balance;    // --set-balance account currency amount
    std::vector<std::string> add_friend; // --add-friend code endpoint
    bool show_code = false;              // --friend-code
    std::filesystem::path root;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            auto next = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument("missing value for " + arg);
                return argv[++i];
            };
            if (arg == "--host") config.host = next();
            // Friends of runtime/user/online/friends.json, reached at their saved endpoints.
            else if (arg == "--friend-network")
                config.friend_network = true;
            else if (arg == "--local-name")
                config.local_name = next();
            else if (arg == "--match-hub") {
                // Public matchmaking on another service (CODE@IPv4[:PORT]), as the launcher's
                // hub election would choose it.
                const std::string value = next();
                const auto at = value.find('@'), colon = value.find(':', value.find('@'));
                if (at == std::string::npos) throw std::invalid_argument("--match-hub expects CODE@IPv4[:PORT]");
                MatchHub hub{value.substr(0, at),
                             value.substr(at + 1, colon == std::string::npos ? std::string::npos : colon - at - 1)};
                if (colon != std::string::npos)
                    hub.port = static_cast<std::uint16_t>(std::stoi(value.substr(colon + 1)));
                config.match_hub = [hub] { return std::optional<MatchHub>(hub); };
            } else if (arg == "--http")
                config.http_port = static_cast<std::uint16_t>(std::stoi(next()));
            else if (arg == "--tcp")
                config.tcp_record_ports = port_list(next());
            else if (arg == "--udp")
                config.udp_record_ports = port_list(next());
            else if (arg == "--peer-host")
                config.peer_host = next();
            else if (arg == "--stun-secondary-host")
                config.stun_secondary_host = next();
            else if (arg == "--stun-alternate-port")
                config.stun_alternate_port = static_cast<std::uint16_t>(std::stoi(next()));
            else if (arg == "--public-host")
                config.public_host = next();
            else if (arg == "--discovery-port")
                config.discovery_port = static_cast<std::uint16_t>(std::stoi(next()));
            else if (arg == "--relay-port")
                config.relay_port = static_cast<std::uint16_t>(std::stoi(next()));
            else if (arg == "--upnp")
                upnp = true;
            else if (arg == "--auto-public")
                auto_public = true;
            else if (arg == "--upnp-probe")
                probe = true;
            else if (arg == "--upnp-check")
                check = true;
            else if (arg == "--upnp-location")
                location = next();
            else if (arg == "--root")
                root = next();
            else if (arg == "--set-balance") {
                balance.push_back(next());
                balance.push_back(next());
                balance.push_back(next());
            } else if (arg == "--friend-code")
                show_code = true;
            else if (arg == "--add-friend") {
                add_friend.push_back(next());
                add_friend.push_back(next());
            } else if (arg == "--help" || arg == "-h") {
                usage();
                return 0;
            } else
                throw std::invalid_argument("unknown option " + arg);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        usage();
        return 2;
    }
    if (root.empty()) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        root = find_project_root(std::filesystem::path(path).parent_path());
    }
    config.state_dir = root / "runtime" / "user" / "online";
    if (!balance.empty()) {
        // An all-digit value is an account ID (online IDs start with a letter); otherwise the
        // account the server derives from the online ID at login.
        try {
            const bool digits = balance[0].find_first_not_of("0123456789") == std::string::npos;
            const std::uint64_t account = digits ? std::stoull(balance[0]) : user_id(balance[0]);
            const std::uint64_t currency = std::stoull(balance[1]);
            const std::int64_t amount = std::stoll(balance[2]);
            MarketplaceStore(config.state_dir / "marketplace.txt").set_balance(account, currency, amount);
            std::printf("{\"event\": \"balance_set\", \"account\": %llu, \"currency\": %llu, \"amount\": %lld}\n",
                        static_cast<unsigned long long>(account), static_cast<unsigned long long>(currency),
                        static_cast<long long>(amount));
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "set-balance: %s\n", error.what());
            return 2;
        }
    }
    if (show_code) {
        const auto code = local_friend_code(root);
        std::printf("%s\n", code.c_str());
        return code.empty() ? 2 : 0;
    }
    if (!add_friend.empty()) {
        // Testing and manual setup: what the launcher's AMICI panel does, plus the endpoint
        // the friend's service is reached at (its friend network finds it there).
        std::string error;
        FriendStore store(config.state_dir / "friends.json");
        FriendCodeInfo info;
        if (parse_friend_code(add_friend[0], info) != CodeParseError::Ok ||
            !store.add_or_update(info.code, "", &error) ||
            !store.update_endpoint(info.code, add_friend[1], "Added from the command line")) {
            std::fprintf(stderr, "add-friend: %s\n", error.empty() ? "invalid friend code" : error.c_str());
            return 2;
        }
        std::printf("{\"event\": \"friend_added\", \"endpoint\": \"%s\"}\n", add_friend[1].c_str());
        return 0;
    }
    config.log_path = default_log_path(root);
    std::mutex out;
    config.on_event = [&](const std::string& line) {
        std::lock_guard<std::mutex> lock(out);
        std::fwrite(line.data(), 1, line.size(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    };
    auto say = [&](const std::string& line) { config.on_event(line); };

    PortMapper mapper(say);
    if (check) {
        // Opens the host ports on the router, reports each one, then closes them again.
        const std::uint16_t discovery = config.discovery_port ? config.discovery_port : 3078;
        const bool found = mapper.open(host_ports(config.http_port, discovery), "CTR Native Port", location);
        int open_ports = 0;
        for (const PortResult& r : mapper.results()) open_ports += r.ok;
        say(found ? "upnp-check: " + std::to_string(open_ports) + "/" + std::to_string(mapper.results().size()) +
                        " porte aperte, indirizzo esterno " + mapper.external_ip()
                  : "upnp-check: " + mapper.error());
        mapper.close();
        return found && open_ports == static_cast<int>(mapper.results().size()) ? 0 : 1;
    }
    if (probe) {
        // Router discovery and external address only: no port is mapped.
        const bool found = mapper.open({}, "CTR Native Port", location);
        say(found ? "upnp-probe: router " + mapper.router() + ", external address " + mapper.external_ip() + " (" +
                        ipv4_scope(mapper.external_ip()) + "), this PC " + mapper.local_ip()
                  : "upnp-probe: " + mapper.error());
        return found ? 0 : 1;
    }
    if (upnp || auto_public) {
        const std::uint16_t discovery = config.discovery_port ? config.discovery_port : 3078;
        std::vector<PortRequest> ports;
        if (upnp)
            ports = {{"TCP", config.http_port, "server HTTP"},
                     {"TCP", 3074, "lobby"},
                     {"UDP", 3074, "game"},
                     {"UDP", discovery, "discovery"},
                     {"UDP", static_cast<std::uint16_t>(discovery + 1), "discovery"},
                     {"UDP", static_cast<std::uint16_t>(discovery + 2), "discovery"},
                     {"UDP", static_cast<std::uint16_t>(discovery + 3), "discovery"}};
        if (mapper.open(ports, "CTR Native Port", location) && auto_public && config.public_host.empty()) {
            const std::string scope = ipv4_scope(mapper.external_ip());
            if (scope != "public")
                say("upnp: the router reports the address " + mapper.external_ip() + " (" + scope +
                    "): the connection is behind another NAT (for example CGNAT); other players will not be able to reach this PC");
            config.public_host = mapper.external_ip();
        }
        if (auto_public && config.public_host.empty()) {
            std::fprintf(stderr, "public address unknown: pass --public-host\n");
            return 1;
        }
    }
    SetConsoleCtrlHandler(on_console, TRUE);
    try {
        Server server(config);
        server.start();
        while (!g_stop) std::this_thread::sleep_for(std::chrono::milliseconds(200));
        server.stop();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "server error: %s\n", error.what());
        mapper.close();
        return 1;
    }
    mapper.close();
    return 0;
}
