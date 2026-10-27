// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Experimental EU1.21 live-session migration. Disabled unless explicitly enabled.
// Descriptive bindings verified against retail instructions; this is a runtime
// adaptation, not a reconstructed replacement of any original function.
#include "original_call.hpp"
#include <map>
#include <array>
#include <set>
#include <vector>
#include <cstdio>
#include <mutex>

namespace ctrport::race_migration {
inline unsigned char* image = nullptr;
inline std::recursive_mutex state_mutex;
inline constexpr char retail[] = "39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5";
template <class T> T read(std::uint64_t address) {
    T result{};
    SIZE_T done = 0;
    if (!address ||
        !ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &result, sizeof(result), &done) ||
        done != sizeof(result))
        throw std::runtime_error("Migration field unreadable");
    return result;
}
template <class T> void write(std::uint64_t address, T value) {
    SIZE_T done = 0;
    if (!WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &value, sizeof(value), &done) ||
        done != sizeof(value))
        throw std::runtime_error("Migration field not writable");
}
inline void original(std::uint64_t offset, std::uint64_t a, std::uint64_t b = 0, std::uint64_t c = 0) {
    ctr_call_original_integer3(image + offset, a, b, c);
}
inline void replace_connection(std::uint64_t field, std::uint64_t next) {
    const auto previous = read<std::uint64_t>(field);
    if (previous == next) return;
    if (next) InterlockedIncrement(reinterpret_cast<volatile LONG*>(next + 8));
    write(field, next);
    if (previous && !InterlockedDecrement(reinterpret_cast<volatile LONG*>(previous + 8))) {
        const auto table = read<std::uint64_t>(previous);
        ctr_call_original_integer2(reinterpret_cast<void*>(read<std::uint64_t>(table + 8)), previous, 0);
    }
}

inline std::uint64_t host_document = 0, connected_owner = 0;
struct Route {
    std::uint64_t account;
    std::array<unsigned char, 67> address;
};
inline std::vector<Route> routes;
inline std::set<std::uint64_t> failed_accounts;
// This field comes from the authenticated backend document. Only numeric IDs and
// fixed-size addresses are accepted; it never supplies code or memory addresses.
inline void cache_routes(std::uint64_t json) {
    routes.clear();
    try {
        std::string text;
        while (text.size() < 16384) {
            const auto at = json + text.size();
            const auto length = static_cast<std::size_t>((std::min)(512ULL, 4096ULL - (at & 4095)));
            std::array<char, 512> chunk{};
            SIZE_T done = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(at), chunk.data(), length, &done) ||
                done != length)
                throw std::runtime_error("Route document unreadable");
            const auto end =
                static_cast<std::size_t>(std::find(chunk.begin(), chunk.begin() + length, '\0') - chunk.begin());
            text.append(chunk.data(), end);
            if (end < length) break;
        }
        const std::string key = "\"ctr_migration\"";
        auto pos = text.find(key);
        if (pos == std::string::npos) return;
        if (text.find(key, pos + key.size()) != std::string::npos) throw std::runtime_error("Duplicate route field");
        pos += key.size();
        auto space = [&] {
            while (pos < text.size() &&
                   (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n'))
                ++pos;
        };
        space();
        if (pos >= text.size() || text[pos++] != ':') throw std::runtime_error("Route field separator");
        space();
        if (text.compare(pos, 4, "\"v1|") != 0) throw std::runtime_error("Route format");
        pos += 4;
        std::vector<Route> parsed;
        std::set<std::uint64_t> seen;
        auto digit = [](char c) -> unsigned {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            throw std::runtime_error("Route hex encoding");
        };
        while (pos < text.size() && text[pos] != '\"') {
            const auto first = pos;
            while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') ++pos;
            if (pos == first || pos - first > 20 || pos >= text.size() || text[pos++] != '=')
                throw std::runtime_error("Route account");
            Route route{};
            route.account = std::stoull(text.substr(first, pos - first - 1));
            if (!route.account || !seen.insert(route.account).second || parsed.size() >= 8 || text.size() - pos < 135)
                throw std::runtime_error("Route bounds");
            for (auto& byte : route.address) {
                byte = static_cast<unsigned char>((digit(text[pos]) << 4) | digit(text[pos + 1]));
                pos += 2;
            }
            if (text[pos++] != ';') throw std::runtime_error("Route terminator");
            parsed.push_back(route);
        }
        if (pos >= text.size() || text[pos] != '\"') throw std::runtime_error("Route string truncated");
        routes = std::move(parsed);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Migration route cache rejected: %s\n", e.what());
        std::fflush(stderr);
    }
}
struct Transition {
    std::uint64_t session = 0, local = 0, manager = 0, lost = 0, retained_connection = 0;
    std::uint64_t self = 0, successor = 0;
    unsigned send_mask = 0, receive_mask = 0;
    bool active = false, winner = false;
};
inline Transition transition;
inline ULONGLONG deadline = 0;
inline std::set<std::uint64_t> pending;
inline std::map<std::uint64_t, std::uint64_t> object_accounts;
inline std::uint64_t departed_player = 0;
inline void remember_owners(std::uint64_t local, const std::set<std::uint64_t>& members);
inline bool within_window() {
    if (!transition.active) return false;
    if (GetTickCount64() < deadline) return true;
    transition.active = false;
    if (transition.lost) {
        original(0x432d70, transition.local, read<std::uint64_t>(transition.lost + 0x10));
        transition.lost = 0;
        replace_connection(reinterpret_cast<std::uint64_t>(&transition.retained_connection), 0);
    }
    std::puts("{\"scope\":\"race_migration_timeout\"}");
    std::fflush(stdout);
    return false;
}
inline void return_from(CONTEXT& c, std::uint64_t value) {
    c.Rax = value;
    c.Rip = read<std::uint64_t>(c.Rsp);
    c.Rsp += 8;
}
inline std::vector<std::uint64_t> replicas(std::uint64_t local) {
    const auto count = read<unsigned>(local + 0x28);
    const auto data = read<std::uint64_t>(local + 0x38);
    if (count > 65536 || (count && !data)) throw std::runtime_error("Invalid replica vector");
    std::vector<std::uint64_t> result;
    for (unsigned n = 0; n < count; ++n) result.push_back(read<std::uint64_t>(data + n * 8ULL));
    return result;
}
// Retail live observations: the per-client player root (type 1, sequence 3)
// carries its account at +0xa8. Its 32-bit ID namespace also identifies the
// client's dependent replicas. Require all roots to share the local root type.
inline void remember_owners(std::uint64_t local, const std::set<std::uint64_t>& members) {
    std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> roots;
    std::uint64_t local_type = 0;
    departed_player = 0;
    for (const auto replica : replicas(local)) {
        const auto id = read<std::uint64_t>(replica + 0x30);
        if (read<unsigned char>(replica + 0x28) != 1 || (id & 0xffffffffULL) != 3) continue;
        const auto root = read<std::uint64_t>(replica + 0x10);
        const auto handle = read<std::uint64_t>(root + 0x18);
        if (read<unsigned>(handle) & 0x10000000)
            throw std::runtime_error("Redirected player handle unsupported during migration");
        const auto object = read<std::uint64_t>(handle + 0x28);
        const auto account = read<std::uint64_t>(object + 0xa8), type = read<std::uint64_t>(object);
        if (!members.count(account)) continue;
        roots[id >> 32] = {account, type};
        if (account == transition.self) local_type = type;
        if (account == connected_owner) departed_player = object;
    }
    if (!local_type) throw std::runtime_error("Local player identity unavailable for migration");
    std::map<std::uint64_t, std::uint64_t> namespaces;
    for (const auto& [prefix, root] : roots) {
        if (root.second != local_type) throw std::runtime_error("Player root type differs during migration");
        namespaces[prefix] = root.first;
    }
    object_accounts.clear();
    for (const auto replica : replicas(local)) {
        const auto id = read<std::uint64_t>(replica + 0x30), source = read<std::uint64_t>(replica + 0x38);
        const auto found = namespaces.find((source ? source : id) >> 32);
        if (found != namespaces.end()) object_accounts[id] = found->second;
    }
}
inline std::uint64_t find_peer(std::uint64_t local, std::uint64_t connection) {
    const auto table = read<std::uint64_t>(local + 0x20);
    const auto bytes = read<std::uint64_t>(table + 0x20) & 0xfffffffffffffULL;
    const auto keys = read<std::uint64_t>(table + 0x28), values = read<std::uint64_t>(table + 0x18);
    if (bytes > 65536 || bytes % 8) throw std::runtime_error("Invalid peer table");
    for (std::uint64_t n = 0; n < bytes; n += 8)
        if (read<std::uint64_t>(keys + n) == connection) return read<std::uint64_t>(values + n);
    throw std::runtime_error("New peer missing from table");
}
// Return 0 for the original fallback, 1 to remove only the lost peer, 2 to defer
// its removal until the newly elected server has completed its authenticated join.
inline unsigned migrate(CONTEXT& context) {
    const auto local = context.R12, lost = context.Rbx, manager = context.R14;
    const auto socket = read<std::uint64_t>(manager + 0x20);
    const auto session = read<std::uint64_t>(socket + 0x10);
    if (!session || !host_document || read<unsigned>(session + 0x118) != 2 || read<unsigned char>(session + 0xf0) != 1)
        return 0;
    const auto self = read<std::uint64_t>(session + 0xd8);
    auto successor = read<std::uint64_t>(host_document + 0x90);
    if (!self) return 0;
    if (within_window() && transition.session == session) return 2;
    if (connected_owner) failed_accounts.insert(connected_owner);
    auto successor_address = read<std::array<unsigned char, 67>>(host_document + 0x48);
    const auto roster = read<std::uint64_t>(host_document + 0x38);
    const auto members = read<unsigned>(host_document + 0x44);
    if (members > 8 || !members || !roster) return 0;
    pending.clear();
    for (unsigned n = 0; n < members; ++n) {
        const auto account = read<std::uint64_t>(roster + 24ULL * n);
        if (account && account != self && !failed_accounts.count(account)) pending.insert(account);
    }
    if (!successor || failed_accounts.count(successor)) {
        successor = 0;
        for (const auto& route : routes) {
            if (failed_accounts.count(route.account) || (route.account != self && !pending.count(route.account)))
                continue;
            successor = route.account;
            successor_address = route.address;
            break;
        }
        if (!successor) return 0;
        std::puts("{\"scope\":\"race_migration_using_cached_route\"}");
        std::fflush(stdout);
    }
    const bool winner = self == successor;
    if (!winner) pending = {successor};
    deadline = GetTickCount64() + 30000;
    const auto old_connection = read<std::uint64_t>(read<std::uint64_t>(lost + 0x10) + 0x10);
    transition = {
        session, local, manager, lost, 0, self, successor, read<unsigned>(lost + 0x50), read<unsigned>(lost + 0x54),
        true,    winner};
    std::printf("{\"scope\":\"race_migration_begin\",\"self\":%llu,\"successor\":%llu,\"winner\":%s}\n",
                static_cast<unsigned long long>(self), static_cast<unsigned long long>(successor),
                winner ? "true" : "false");
    std::fflush(stdout);
    if (winner) {
        auto known_members = pending;
        known_members.insert(self);
        known_members.insert(connected_owner);
        remember_owners(local, known_members);
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(old_connection + 8));
        transition.retained_connection = old_connection;
        write<unsigned>(session + 0x118, 1);
        replace_connection(session + 0x110, 0);
        original(0xa44550, 1);
        unsigned transferred = 0;
        for (const auto replica : replicas(local))
            if (read<std::uint64_t>(replica + 0x48) == lost) {
                const auto owner = object_accounts.find(read<std::uint64_t>(replica + 0x30));
                if (owner != object_accounts.end() && pending.count(owner->second)) continue;
                original(0x43bb10, local, replica);
                ++transferred;
            }
        // Keep the forwarding peer until its surviving objects have new owners.
        write<unsigned char>(read<std::uint64_t>(manager + 0x18) + 0x20, 0);
        std::printf("{\"scope\":\"race_migration_promoted\",\"replicas\":%u,\"session_preserved\":true}\n",
                    transferred);
        std::fflush(stdout);
        return 2;
    }
    // The old replication peer remains alive until its replacement exists. Its
    // transport object must also remain alive because the peer's address is a raw ID.
    InterlockedIncrement(reinterpret_cast<volatile LONG*>(old_connection + 8));
    transition.retained_connection = old_connection;
    std::uint64_t address = 0;
    original(0x400f00, reinterpret_cast<std::uint64_t>(&address),
             reinterpret_cast<std::uint64_t>(successor_address.data()));
    const auto connected =
        ctr_call_original_integer2(image + 0x40a9b0, session, reinterpret_cast<std::uint64_t>(&address));
    replace_connection(reinterpret_cast<std::uint64_t>(&address), 0);
    if (!connected) throw std::runtime_error("Successor connection could not be started");
    write<unsigned char>(read<std::uint64_t>(manager + 0x18) + 0x20, 0);
    std::puts("{\"scope\":\"race_migration_reconnecting\",\"session_preserved\":true}");
    std::fflush(stdout);
    return 2;
}
inline void connected(CONTEXT& c) {
    const auto peer = find_peer(c.R12, c.R13);
    write<unsigned>(peer + 0x50, transition.send_mask);
    write<unsigned>(peer + 0x54, transition.receive_mask);
    unsigned reassigned = 0;
    if (transition.winner && transition.lost) {
        const auto account = read<std::uint64_t>(c.R13 + 0x20);
        for (const auto replica : replicas(c.R12)) {
            const auto owner = object_accounts.find(read<std::uint64_t>(replica + 0x30));
            if (owner != object_accounts.end() && owner->second == account &&
                read<std::uint64_t>(replica + 0x48) == transition.lost) {
                write<std::uint64_t>(replica + 0x48, peer);
                ++reassigned;
            }
        }
    }
    if (!transition.winner && transition.lost) {
        for (const auto replica : replicas(c.R12))
            if (read<std::uint64_t>(replica + 0x48) == transition.lost) {
                write<std::uint64_t>(replica + 0x48, peer);
                ++reassigned;
            }
        const auto address = read<std::uint64_t>(transition.lost + 0x10);
        original(0x432d70, c.R12, address);
        transition.lost = 0;
        replace_connection(reinterpret_cast<std::uint64_t>(&transition.retained_connection), 0);
    }
    std::printf(
        "{\"scope\":\"race_migration_peer_connected\",\"self\":%llu,\"account\":%llu,\"reassigned\":%u,\"session_preserved\":true}\n",
        static_cast<unsigned long long>(transition.self),
        static_cast<unsigned long long>(read<std::uint64_t>(c.R13 + 0x20)), reassigned);
    std::fflush(stdout);
    pending.erase(read<std::uint64_t>(c.R13 + 0x20));
    if (!transition.winner || pending.empty()) {
        if (transition.lost) {
            original(0x432d70, c.R12, read<std::uint64_t>(transition.lost + 0x10));
            transition.lost = 0;
            replace_connection(reinterpret_cast<std::uint64_t>(&transition.retained_connection), 0);
        }
        if (transition.winner && departed_player) {
            const auto manager = read<std::uint64_t>(reinterpret_cast<std::uint64_t>(image) + 0x2ad1160);
            const auto list = read<std::uint64_t>(manager + 0x10);
            const auto count = read<unsigned>(list + 0xc);
            const auto data = read<std::uint64_t>(list + 0x20);
            bool present = false;
            if (count <= 8)
                for (unsigned n = 0; n < count; ++n) present |= read<std::uint64_t>(data + n * 8ULL) == departed_player;
            if (present) {
                original(0xc22160, manager, departed_player);
                std::puts("{\"scope\":\"race_migration_departed_player_notified\"}");
            } else
                std::puts("{\"scope\":\"race_migration_departure_lookup_failed\"}");
            departed_player = 0;
        }
        transition.active = false;
        connected_owner = transition.successor;
        std::puts("{\"scope\":\"race_migration_complete\"}");
        std::fflush(stdout);
    }
    // Keep the existing player IDs, race and channel state. Only release the
    // temporary address and return through the original function's epilogue.
    c.Rip = reinterpret_cast<std::uint64_t>(image) + 0xa448d6;
}

inline bool handle(EXCEPTION_POINTERS* exception) {
    if (!image || exception->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) return false;
    auto& c = *exception->ContextRecord;
    const auto at = reinterpret_cast<unsigned char*>(exception->ExceptionRecord->ExceptionAddress);
    if (at != image + 0xa53100 && at != image + 0xf3abd0 && at != image + 0x3ffc30 && at != image + 0x43297c &&
        at != image + 0xa4482f && at != image + 0xa44d25 && at != image + 0xa440dc)
        return false;
    std::lock_guard<std::recursive_mutex> guard(state_mutex);
    try {
        if (at == image + 0xa440dc) {
            const auto manager = read<std::uint64_t>(c.R14 + 0x78);
            const auto socket = read<std::uint64_t>(manager + 0x20);
            const auto current = read<std::uint64_t>(socket + 0x10);
            if (current && c.R15 && current != c.R15 && read<unsigned char>(current + 0xf0) == 1) {
                std::puts("{\"scope\":\"race_session_preserved_after_unrelated_shutdown\"}");
                std::fflush(stdout);
                c.Rip = reinterpret_cast<std::uint64_t>(image) + 0xa44120;
            } else {
                c.Rdi = manager;
                c.Rip = reinterpret_cast<std::uint64_t>(image) + 0xa440e0;
            }
            return true;
        }
        if (at == image + 0xa53100) {
            if (within_window() && transition.winner && read<std::uint64_t>(c.Rdi + 0x10) == transition.session) {
                const auto data = read<std::uint64_t>(c.Rsi + 8);
                const auto count = read<unsigned>(c.Rsi + 20);
                bool allowed = data && count && count <= 8;
                for (unsigned n = 0; allowed && n < count; ++n)
                    allowed = pending.count(read<std::uint64_t>(data + n * 8ULL)) != 0;
                std::printf("{\"scope\":\"race_migration_admission\",\"count\":%u,\"accepted\":%s}\n", count,
                            allowed ? "true" : "false");
                std::fflush(stdout);
                if (allowed) {
                    write<unsigned>(c.Rdx, 0);
                    return_from(c, 1);
                    return true;
                }
            }
            c.Rsp -= 8;
            write(c.Rsp, c.Rbp);
            c.Rip = reinterpret_cast<std::uint64_t>(image) + 0xa53101;
            return true;
        }
        if (at == image + 0xf3abd0) {
            if (within_window()) {
                return_from(c, 0);
                return true;
            }
            c.Rsp -= 8;
            write(c.Rsp, c.Rbp);
            c.Rip = reinterpret_cast<std::uint64_t>(image) + 0xf3abd1;
            return true;
        }
        if (at == image + 0x3ffc30) {
            // Retail 0x3ffc68 calls 0x17cfd0, which returns [RDI]: the
            // decoder receives a string object, not its character buffer.
            host_document = c.Rdi;
            cache_routes(read<std::uint64_t>(c.Rsi));
            // Emulate only the displaced PUSH RBP; the entire decoder still runs.
            c.Rsp -= 8;
            write(c.Rsp, c.Rbp);
            c.Rip = reinterpret_cast<std::uint64_t>(image) + 0x3ffc31;
            return true;
        }
        if (at == image + 0x43297c) {
            if (within_window() && c.R14 == transition.local)
                c.Rip = reinterpret_cast<std::uint64_t>(image) + 0x432982;
            else {
                const auto target = read<std::uint64_t>(c.Rax + 0xb0);
                c.Rsp -= 8;
                write(c.Rsp, reinterpret_cast<std::uint64_t>(image) + 0x432982);
                c.Rip = target;
            }
            return true;
        }
        if (at == image + 0xa4482f) {
            if (within_window() && c.R12 == transition.local)
                connected(c);
            else {
                failed_accounts.clear();
                if (host_document) connected_owner = read<std::uint64_t>(host_document + 0x90);
                write<unsigned char>(c.R15 + 0x80, 0);
                c.Rip = reinterpret_cast<std::uint64_t>(image) + 0xa44837;
            }
            return true;
        }
        if (at != image + 0xa44d25) return false;
        const bool host = read<unsigned char>(c.Rbx + 0xc) != 0;
        const auto outcome = host ? migrate(c) : 1;
        c.Rip = reinterpret_cast<std::uint64_t>(image) + (outcome == 2 ? 0xa44d39 : outcome == 1 ? 0xa44dda : 0xa44d2f);
        return true;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Host migration failed: %s\n", e.what());
        std::fflush(stderr);
        return false;
    }
}
inline void install(unsigned char* base, const std::string& pin) {
    char option[2]{};
    if (GetEnvironmentVariableA("CTR_EXPERIMENTAL_HOST_MIGRATION", option, 2) != 1 || option[0] != '1') return;
    if (pin != retail) throw std::runtime_error("Host migration has no binding for this retail version");
    struct Site {
        std::uint64_t offset;
        std::vector<unsigned char> expected;
    };
    const Site sites[] = {{0xa44d25, {0x80, 0x7b, 0x0c, 0x00, 0x0f, 0x84}},
                          {0x3ffc30, {0x55, 0x48, 0x89, 0xe5}},
                          {0xa440dc, {0x49, 0x8b, 0x7e, 0x78}},
                          {0xa53100, {0x55, 0x48, 0x89, 0xe5}},
                          {0xf3abd0, {0x55, 0x48, 0x89, 0xe5}},
                          {0x43297c, {0xff, 0x90, 0xb0, 0, 0, 0}},
                          {0xa4482f, {0x41, 0xc6, 0x87, 0x80, 0, 0, 0, 0}}};
    for (const auto& site : sites)
        if (std::memcmp(base + site.offset, site.expected.data(), site.expected.size()))
            throw std::runtime_error("Migration hook signature differs");
    image = base;
    for (const auto& site : sites) {
        DWORD before = 0, ignored = 0;
        if (!VirtualProtect(base + site.offset, 1, PAGE_EXECUTE_READWRITE, &before))
            throw std::runtime_error("Cannot protect migration hook");
        base[site.offset] = 0xcc;
        if (!VirtualProtect(base + site.offset, 1, before, &ignored) ||
            !FlushInstructionCache(GetCurrentProcess(), base + site.offset, 1))
            throw std::runtime_error("Cannot publish migration hook");
    }
    std::puts(
        "{\"scope\":\"experimental_race_migration_enabled\",\"retail\":\"EU1.21\",\"variant\":\"cached_routes_and_session_guard\"}");
}
}
