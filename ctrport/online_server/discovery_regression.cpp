// SPDX-License-Identifier: GPL-2.0-or-later
// Real loopback services and separate DHT processes; no game, WAN or user data.
#include "server.hpp"
#include "launcher_online.hpp"
#include "friend_store.hpp"
#include "friend_identity.hpp"
#include "crypto.hpp"
#include "dht_service.hpp"
#include "../dht/dht.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <thread>
using namespace ctr_online;
int failures = 0;
void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    std::fflush(stdout);
    if (!ok) ++failures;
}
struct Child {
    PROCESS_INFORMATION process{};
    Child(const std::wstring& args) {
        wchar_t path[32768];
        GetModuleFileNameW(nullptr, path, 32768);
        auto command = std::wstring(L"\"") + path + L"\" " + args;
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                            &process))
            throw std::runtime_error("child start");
    }
    void stop() {
        if (process.hProcess) {
            TerminateProcess(process.hProcess, 0);
            WaitForSingleObject(process.hProcess, 2000);
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            process = {};
        }
    }
    ~Child() { stop(); }
};
int main(int argc, char** argv) {
    try {
        dht_allow_loopback = 1;
        const Bytes key = sha1(bytes_of("ctr-discovery-regression-042"));
        const bool presence_only = argc > 1 && std::string(argv[1]) == "--presence-only";
        const bool late_only = argc > 1 && std::string(argv[1]) == "--late-only";
        if (argc > 1 && !late_only && !presence_only) {
            DhtService d;
            const bool publisher = std::string(argv[1]) == "--publisher";
            const bool host = std::string(argv[1]) == "--host";
            if (!d.start(publisher ? 0 : 28990, {},
                         publisher ? std::vector<std::string>{"127.0.0.1:28990"}
                                   : std::vector<std::string>{"127.0.0.1:1"}))
                return 2;
            if (host || publisher) {
                d.set_announce(key, 28783);
                if (publisher) d.search(key, 100);
            }
            std::this_thread::sleep_for(std::chrono::seconds(60));
            return 0;
        }
        if (!presence_only) {
            Child host(L"--host");
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            DhtService guest;
            check(guest.start(0, {}, {"127.0.0.1:1"}), "DHT guest starts");
            check(guest.search(key, 50).empty(), "cold lookup initially times out");
            guest.search(sha1(bytes_of("unrelated-friend")), 50);
            guest.ping_node("127.0.0.1", 28990);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            host.stop(); // The late result must be cached: no node remains to answer a retry.
            const auto found = guest.search(key, 1);
            check(std::find(found.begin(), found.end(), "127.0.0.1:28783") != found.end(),
                  "late DHT results survive timeout and a different lookup");
        }
        if (!presence_only) {
            if (late_only) return failures ? 1 : 0;
            Child storage(L"--storage");
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
            Child publisher(L"--publisher");
            DhtService guest;
            check(guest.start(0, {}, {"127.0.0.1:28990"}), "independent DHT reader starts");
            bool found = false;
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(35);
            while (!found && std::chrono::steady_clock::now() < end) {
                const auto rows = guest.search(key, 1000);
                found = std::find(rows.begin(), rows.end(), "127.0.0.1:28783") != rows.end();
                if (!found) std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            check(found, "lookup of own key preserves announcement on a third node");
        }
        const auto root = std::filesystem::temp_directory_path() / ("ctr_recovery_" + hex(random_bytes(8)));
        const auto ar = root / "a", br = root / "b";
        const auto a = local_friend_code(ar), b = local_friend_code(br);
        const auto ap = ar / "runtime/user/online/friends.json", bp = br / "runtime/user/online/friends.json";
        FriendStore(ap).add_or_update(b, "B");
        FriendStore(bp).add_or_update(a, "A");
        auto config = [](const std::filesystem::path& r, const char* ip, const char* name) {
            ServerConfig x;
            x.host = ip;
            x.public_host = ip;
            x.http_port = 28784;
            x.discovery_port = 28790;
            x.relay_port = 0;
            x.tcp_record_ports = {};
            x.udp_record_ports = {};
            x.state_dir = r / "runtime/user/online";
            x.log_path = r / "runtime/user/logs/test.jsonl";
            x.friend_network = true;
            x.local_name = name;
            x.friend_poll_period = std::chrono::hours(1);
            return x;
        };
        auto ac = config(ar, "127.0.0.1", "Anna"), bc = config(br, "127.0.0.3", "Bruno");
        // Simulate discovery succeeding on A first; neither address book is prepopulated.
        ac.friend_resolver = [](const std::string&) -> std::optional<std::pair<std::string, std::uint16_t>> {
            return std::make_pair("127.0.0.3", std::uint16_t(28784));
        };
        {
            Server sa(ac), sb(bc);
            sa.start();
            sb.start();
            sa.poll_friends();
            sb.poll_friends();
            check(sa.friends().size() == 1 && sa.friends()[0].online,
                  "first direction discovers and authenticates friend");
            check(sb.friends().size() == 1 && sb.friends()[0].online && sb.friends()[0].name == "Anna" &&
                      !sb.friends()[0].in_game,
                  "signed incoming presence recovers reverse direction without game or saved IP");
            check(FriendStore(bp).entries()[0].last_endpoint == "127.0.0.1:28784",
                  "reverse endpoint saved only after authenticated response");
            FriendStore stale(bp);
            FriendStore(bp).remove(a);
            stale.update_status(a, "late lookup");
            check(FriendStore(bp).entries().empty(), "stale lookup cannot resurrect a removed friend");
            sb.poll_friends();
            FriendStore(bp).add_or_update(a, "A");
            sa.poll_friends();
            sb.poll_friends();
            check(sb.friends().size() == 1 && sb.friends()[0].online,
                  "remove and re-add recovers automatically from signed contact");
            sa.stop();
            sb.stop();
        }
        const auto c = local_friend_code(root / "c");
        FriendStore(ap).add_or_update(c, "C");
        const auto identity = HostIdentity::load_or_create(ar / "runtime/user/identity/host_identity.json");
        int visited = 0;
        bool threw = false;
        FriendWatcher watcher(identity, ap,
                              [&](const std::string& code) -> std::optional<std::pair<std::string, std::uint16_t>> {
                                  ++visited;
                                  if (code == b) throw std::runtime_error("fixture resolver failure");
                                  return std::nullopt;
                              });
        try {
            watcher.poll_now();
        } catch (...) {
            threw = true;
        }
        check(!threw && visited == 2, "one resolver exception does not freeze the entire presence round");
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::printf("discovery_regression failures=%d\n", failures);
        return failures ? 1 : 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ERROR %s\n", e.what());
        return 2;
    }
}