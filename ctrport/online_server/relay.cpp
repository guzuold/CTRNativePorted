// SPDX-License-Identifier: GPL-2.0-or-later
#include "relay.hpp"
#include <algorithm>
#include <array>
#include <cstdio>

namespace ctr_online {
namespace {
constexpr unsigned char kMagic[4] = {'C', 'T', 'R', 'R'};

std::optional<std::array<unsigned char, 4>> ipv4_bytes(const std::string& ip) {
    std::array<unsigned char, 4> out{};
    std::size_t i = 0;
    for (int part = 0; part < 4; ++part) {
        if (part && (i >= ip.size() || ip[i++] != '.')) return std::nullopt;
        unsigned value = 0, digits = 0;
        while (i < ip.size() && ip[i] >= '0' && ip[i] <= '9' && digits < 4)
            value = value * 10 + unsigned(ip[i++] - '0'), ++digits;
        if (!digits || value > 255) return std::nullopt;
        out[part] = static_cast<unsigned char>(value);
    }
    if (i != ip.size()) return std::nullopt;
    return out;
}
std::string ipv4_text(const unsigned char* p) {
    return std::to_string(p[0]) + "." + std::to_string(p[1]) + "." + std::to_string(p[2]) + "." + std::to_string(p[3]);
}
void put_address(Bytes& out, const Endpoint& address) {
    const auto ip = ipv4_bytes(address.ip);
    for (int i = 0; i < 4; ++i) out.push_back(ip ? (*ip)[i] : 0);
    out.push_back(static_cast<unsigned char>(address.port >> 8));
    out.push_back(static_cast<unsigned char>(address.port & 0xFF));
}
Endpoint get_address(const Bytes& data, std::size_t offset) {
    return {ipv4_text(&data[offset]), static_cast<std::uint16_t>(data[offset + 4] << 8 | data[offset + 5])};
}
Json member_json(std::uint64_t account) {
    return Json::of(account);
}
} // namespace

namespace relay {
Bytes encode(Type type, const Endpoint& address, const Bytes& payload) {
    Bytes out(kMagic, kMagic + 4);
    out.push_back(kVersion);
    out.push_back(type);
    put_address(out, address);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}
Bytes hello(const Bytes& token) {
    Bytes out(kMagic, kMagic + 4);
    out.push_back(kVersion);
    out.push_back(HELLO);
    out.insert(out.end(), token.begin(), token.end());
    return out;
}
const char* reason_name(unsigned char reason) {
    switch (reason) {
    case NOT_REGISTERED: return "not_registered";
    case UNKNOWN_DESTINATION: return "unknown_destination";
    case DESTINATION_OFFLINE: return "destination_offline";
    case RATE_LIMITED: return "rate_limited";
    case MALFORMED: return "malformed";
    default: return "unknown";
    }
}
} // namespace relay

std::optional<Endpoint> block_endpoint(const Bytes& block, std::size_t offset) {
    if (block.size() < offset + 6) return std::nullopt;
    const std::uint16_t port = static_cast<std::uint16_t>(block[offset + 4] | block[offset + 5] << 8);
    if (!port || !(block[offset] | block[offset + 1] | block[offset + 2] | block[offset + 3])) return std::nullopt;
    return Endpoint{ipv4_text(&block[offset]), port};
}

Relay::Relay(RelayLimits limits) : limits_(limits) {}

Bytes Relay::issue(std::uint64_t account, const std::string& room) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const auto old = token_of_.find(account); old != token_of_.end()) tokens_.erase(old->second);
    drop_binding(account);
    const Bytes token = random_bytes(relay::kTokenSize);
    tokens_[hex(token)] = {account, room};
    token_of_[account] = hex(token);
    return token;
}

void Relay::forget(std::uint64_t account) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (const auto old = token_of_.find(account); old != token_of_.end()) {
        tokens_.erase(old->second);
        token_of_.erase(old);
    }
    drop_binding(account);
}

void Relay::drop_binding(std::uint64_t account) {
    const auto it = bindings_.find(account);
    if (it == bindings_.end()) return;
    by_endpoint_.erase(it->second.endpoint.text());
    bindings_.erase(it);
    for (auto a = aliases_.begin(); a != aliases_.end();) {
        if (a->first.first == account || a->first.second == account)
            a = aliases_.erase(a);
        else
            ++a;
    }
}

Relay::Stats Relay::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

Relay::Outcome Relay::refuse(const Endpoint& to, const Endpoint& address, unsigned char reason) {
    ++stats_.refused;
    Outcome outcome;
    outcome.out.push_back({to, relay::encode(relay::REFUSED, address, Bytes{reason})});
    return outcome;
}

Relay::Outcome Relay::handle(const Bytes& datagram, const Endpoint& from, double now, const RelayRoster& roster) {
    if (datagram.size() < relay::kHeader || !std::equal(kMagic, kMagic + 4, datagram.begin()) ||
        datagram[4] != relay::kVersion)
        return {}; // not ours: no answer to unknown traffic
    const unsigned char type = datagram[5];
    std::lock_guard<std::mutex> lock(mutex_);
    if (type == relay::HELLO) {
        const Endpoint none{"0.0.0.0", 0};
        if (datagram.size() != relay::kHeader + relay::kTokenSize) return refuse(from, none, relay::MALFORMED);
        const auto grant = tokens_.find(hex(Bytes(datagram.begin() + relay::kHeader, datagram.end())));
        if (grant == tokens_.end()) {
            Outcome outcome = refuse(from, none, relay::NOT_REGISTERED);
            outcome.event = "relay_hello_refused";
            outcome.fields.set("peer", Json::of(from.text()));
            return outcome;
        }
        const std::uint64_t account = grant->second.account;
        Outcome outcome;
        auto binding = bindings_.find(account);
        if (binding == bindings_.end() || !(binding->second.endpoint == from)) {
            if (binding != bindings_.end()) by_endpoint_.erase(binding->second.endpoint.text());
            if (const auto other = by_endpoint_.find(from.text());
                other != by_endpoint_.end() && other->second != account)
                drop_binding(other->second); // the endpoint changed hands (NAT reuse)
            Binding fresh;
            fresh.endpoint = from;
            fresh.room = grant->second.room;
            fresh.tokens = limits_.burst;
            fresh.refill = now;
            bindings_[account] = fresh;
            by_endpoint_[from.text()] = account;
            outcome.event = "relay_bound";
            outcome.fields.set("account", member_json(account));
            outcome.fields.set("peer", Json::of(from.text()));
        }
        bindings_[account].last = now;
        Bytes lease{static_cast<unsigned char>(static_cast<unsigned>(limits_.binding_timeout) >> 8),
                    static_cast<unsigned char>(static_cast<unsigned>(limits_.binding_timeout) & 0xFF)};
        outcome.out.push_back({from, relay::encode(relay::WELCOME, from, lease)});
        return outcome;
    }
    if (type != relay::SEND) return {};
    if (datagram.size() < relay::kAddressed) return refuse(from, {"0.0.0.0", 0}, relay::MALFORMED);
    const Endpoint destination = get_address(datagram, relay::kHeader);
    const auto sender_entry = by_endpoint_.find(from.text());
    if (sender_entry == by_endpoint_.end()) return refuse(from, destination, relay::NOT_REGISTERED);
    const std::uint64_t sender = sender_entry->second;
    Binding& own = bindings_.at(sender);
    if (now - own.last > limits_.binding_timeout) {
        drop_binding(sender);
        return refuse(from, destination, relay::NOT_REGISTERED);
    }
    own.last = now;
    const std::size_t payload = datagram.size() - relay::kAddressed;
    if (payload > relay::kMaxPayload) return refuse(from, destination, relay::MALFORMED);
    own.tokens = std::min(limits_.burst, own.tokens + (now - own.refill) * limits_.bytes_per_second);
    own.refill = now;
    if (own.tokens < static_cast<double>(payload)) {
        Outcome outcome = refuse(from, destination, relay::RATE_LIMITED);
        if (now - own.last_refusal_log >= 5) {
            own.last_refusal_log = now;
            outcome.event = "relay_rate_limited";
            outcome.fields.set("account", member_json(sender));
        }
        return outcome;
    }

    const std::vector<RelayMember> members = roster(own.room);
    const auto member_of = [&](std::uint64_t account) -> const RelayMember* {
        for (const RelayMember& m : members)
            if (m.account == account) return &m;
        return nullptr;
    };
    const RelayMember* self = member_of(sender);
    if (!self) {
        drop_binding(sender); // left the room or the lobby
        return refuse(from, destination, relay::NOT_REGISTERED);
    }
    // The destination as the sender's game names it: an address it used before, the advertised
    // public endpoint, the advertised local endpoint (only when unambiguous), the relay endpoint.
    const RelayMember* target = nullptr;
    for (const RelayMember& m : members) {
        const auto alias = aliases_.find({sender, m.account});
        if (m.account != sender && alias != aliases_.end() && alias->second == destination) {
            target = &m;
            break;
        }
    }
    if (!target)
        for (const RelayMember& m : members)
            if (m.account != sender && m.public_endpoint && *m.public_endpoint == destination) {
                target = &m;
                break;
            }
    if (!target) {
        int matches = 0;
        for (const RelayMember& m : members)
            if (m.account != sender && m.local_endpoint && *m.local_endpoint == destination) {
                target = &m;
                ++matches;
            }
        if (matches > 1) target = nullptr;
    }
    if (!target)
        for (const RelayMember& m : members) {
            const auto b = bindings_.find(m.account);
            if (m.account != sender && b != bindings_.end() && b->second.endpoint == destination) {
                target = &m;
                break;
            }
        }
    if (!target) return refuse(from, destination, relay::UNKNOWN_DESTINATION);
    const auto peer = bindings_.find(target->account);
    if (peer == bindings_.end() || peer->second.room != own.room || now - peer->second.last > limits_.binding_timeout)
        return refuse(from, destination, relay::DESTINATION_OFFLINE);

    aliases_[{sender, target->account}] = destination;
    Endpoint source = own.endpoint;
    if (const auto alias = aliases_.find({target->account, sender}); alias != aliases_.end())
        source = alias->second;
    else if (self->public_endpoint)
        source = *self->public_endpoint;
    else if (self->local_endpoint)
        source = *self->local_endpoint;

    own.tokens -= static_cast<double>(payload);
    ++own.forwarded;
    own.bytes += payload;
    ++stats_.forwarded;
    stats_.forwarded_bytes += payload;
    Outcome outcome;
    outcome.out.push_back(
        {peer->second.endpoint,
         relay::encode(relay::DELIVER, source, Bytes(datagram.begin() + relay::kAddressed, datagram.end()))});
    if (own.forwarded == 1) {
        outcome.event = "relay_first_forward";
        outcome.fields.set("from", member_json(sender));
        outcome.fields.set("to", member_json(target->account));
        outcome.fields.set("destination", Json::of(destination.text()));
        outcome.fields.set("source_shown", Json::of(source.text()));
    }
    return outcome;
}
} // namespace ctr_online
