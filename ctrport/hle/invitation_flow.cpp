// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Event ABI follows pinned invitation_dialog.h and systemservice.h. Transport is project-owned.
#include "invitation_flow.h"
#include "invitation_ui.h"
#include "ui_text.h"
#include "online_config.h"
#include "online_http.h"
#include "ctr_hle.h"
#include "../online_server/json.hpp"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <deque>
#include <future>
#include <mutex>
#include <set>
namespace ctr_hle::invitation {
std::atomic<unsigned long> dialog_thread{0};
namespace {
using ctr_online::Json;
constexpr int failure = static_cast<int>(0x80B8000Eu);
constexpr const wchar_t* title = ctr_ui::kInvitationTitle;
using ctr_ui::Text;
using ctr_ui::ui_text;
using ctr_ui::ui_format;
struct Pending {
    std::string id, session, sender;
};
struct Event {
    std::int32_t type;
    unsigned char data[8192];
};
static_assert(sizeof(Event) == 8196);
std::mutex service_mutex, events_mutex;
std::deque<Event> events;
std::deque<std::string> delivered;
std::set<std::string> notified; // Under service_mutex; cleared when no longer pending.
std::future<std::vector<Pending>> polling;
std::future<Outcome> notice;
std::atomic<unsigned long> notice_thread{0};
std::chrono::steady_clock::time_point next_poll{};
int (*owner)(int) = nullptr;
std::wstring wide(const std::string& text) {
    const auto n =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!n && !text.empty()) throw std::runtime_error("invalid UTF-8 invitation text");
    std::wstring result(n, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), n);
    return result;
}
bool safe_id(const std::string& id, std::size_t maximum) {
    return !id.empty() && id.size() <= maximum &&
           id.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-_") ==
               std::string::npos;
}
Json exchange(const std::string& method, const std::string& path, const std::string& body, int expected) {
    online::HttpResponse response;
    if (!online::http_exchange(method, path, {{"Content-Type", "application/json"}}, body, &response))
        throw std::runtime_error(method + " " + path + ": local service unreachable");
    if (response.status != expected)
        throw std::runtime_error(method + " " + path + ": HTTP " + std::to_string(response.status));
    return response.body.empty() ? Json::null() : ctr_online::parse_json(response.body);
}
std::vector<Pending> fetch() {
    auto rows = exchange("GET", "/ctr/v1/session-invitations", "", 200);
    if (!rows.is_array() || rows.items.size() > 64) throw std::runtime_error("invalid invitation list");
    std::vector<Pending> result;
    for (const auto& row : rows.items) {
        Pending entry{row.at("invitationId").text, row.at("sessionId").text, row.at("onlineId").text};
        if (!safe_id(entry.id, 60) || !safe_id(entry.session, 45) || entry.sender.size() > 16)
            throw std::runtime_error("invalid invitation record");
        result.push_back(entry);
    }
    return result;
}
Outcome accept(const Pending& entry) {
    {
        std::scoped_lock lock{events_mutex};
        if (events.size() >= 64) throw std::runtime_error("system invitation queue full");
    }
    auto reply = exchange("PUT", "/ctr/v1/session-invitations/" + entry.id, "{}", 200);
    if (reply.at("sessionId").text != entry.session || reply.at("invitationId").text != entry.id)
        throw std::runtime_error("invitation acceptance response mismatch");
    Outcome result;
    result.result = 0;
    result.session = entry.session;
    result.invitation = entry.id;
    return result;
}
void log_failure(const std::exception& error) {
    ctr_hle::log_call("libSceInvitationDialog", "invitation_transport_failure", failure);
    std::fprintf(stderr, "invitation transport: %s\n", error.what());
}
}
Outcome send(std::string session, std::vector<std::uint64_t> recipients, unsigned maximum,
             std::atomic<unsigned long>& ui_thread) {
    Outcome result;
    try {
        if (maximum) {
            const auto& identity = online::identity();
            auto page = exchange("GET",
                                 "/webapi/sdk:userProfile/v1/users/" + std::to_string(identity.account_id) +
                                     "/friendList?offset=0&limit=500",
                                 "", 200);
            const auto& friends = page.at("friendList");
            if (!friends.is_array() || friends.items.size() > 500) throw std::runtime_error("invalid friend list");
            std::vector<std::wstring> rows;
            std::vector<std::uint64_t> accounts;
            std::vector<std::string> names;
            std::set<std::uint64_t> seen;
            for (const auto& row : friends.items) {
                const auto& user = row.at("user");
                const auto text = user.at("accountId").text;
                if (text.empty() || text.size() > 20 || text.find_first_not_of("0123456789") != std::string::npos)
                    throw std::runtime_error("invalid friend account");
                const auto account = std::stoull(text);
                if (!account || account == identity.account_id || !seen.insert(account).second) continue;
                const auto name = user.at("onlineId").text;
                if (name.empty() || name.size() > 16) throw std::runtime_error("invalid friend name");
                accounts.push_back(account);
                names.push_back(name);
                rows.push_back(wide(name));
            }
            ui_thread = GetCurrentThreadId();
            if (rows.empty()) {
                MessageBoxW(nullptr, ui_text(Text::NoFriends), title, MB_OK);
                ui_thread = 0;
                return result;
            }
            const auto selected = choose(rows, maximum, title, ui_text(Text::Send));
            ui_thread = 0;
            if (selected.empty()) return result;
            for (int i : selected) {
                recipients.push_back(accounts.at(i));
                result.names.push_back(names.at(i));
            }
        }
        // Recipients the game already chose (INVITE on a friend) are sent at once, without a
        // confirmation: only the recipient decides, by accepting (user decision 2026-10-20).
        Json request = Json::object(), ids = Json::array();
        request.set("sessionId", Json::of(session));
        for (auto account : recipients) ids.items.push_back(Json::of(account));
        request.set("recipients", ids);
        const auto receipt = exchange("POST", "/ctr/v1/session-invitations", ctr_online::dump_json(request), 201);
        if (!receipt.is_array() || receipt.items.size() != recipients.size())
            throw std::runtime_error("invalid send receipt");
        for (std::size_t i = 0; i < recipients.size(); ++i)
            if (receipt.items[i].at("accountId").text != std::to_string(recipients[i]) ||
                !safe_id(receipt.items[i].at("invitationId").text, 60))
                throw std::runtime_error("send receipt mismatch");
        result.result = 0;
        result.sent = std::move(recipients);
    } catch (const std::exception& error) {
        ui_thread = 0;
        result = {};
        result.error = failure;
        log_failure(error);
    }
    return result;
}
Outcome receive(std::atomic<unsigned long>& ui_thread) {
    try {
        const auto pending = fetch();
        std::vector<std::wstring> rows;
        for (const auto& entry : pending) rows.push_back(ui_format(Text::InvitationFrom, wide(entry.sender)));
        ui_thread = GetCurrentThreadId();
        if (rows.empty()) {
            MessageBoxW(nullptr, ui_text(Text::NoInvitations), title, MB_OK);
            ui_thread = 0;
            return {};
        }
        const auto selected = choose(rows, 1, title, ui_text(Text::Accept));
        ui_thread = 0;
        return selected.empty() ? Outcome{} : accept(pending.at(selected[0]));
    } catch (const std::exception& error) {
        ui_thread = 0;
        log_failure(error);
        Outcome result;
        result.error = failure;
        return result;
    }
}
void set_owner(int (*operation)(int)) {
    std::scoped_lock lock{service_mutex};
    owner = operation;
}
void post_accepted(const Outcome& outcome) {
    if (outcome.error || outcome.result || !safe_id(outcome.session, 45) || !safe_id(outcome.invitation, 60)) return;
    std::scoped_lock lock{events_mutex};
    if (std::find(delivered.begin(), delivered.end(), outcome.invitation) != delivered.end()) return;
    if (events.size() >= 64) throw std::runtime_error("system invitation queue full");
    Event event{};
    event.type = 0x10000002;
    std::memcpy(event.data, outcome.session.data(), outcome.session.size());            // SessionId[48]
    std::memcpy(event.data + 48, outcome.invitation.data(), outcome.invitation.size()); // InvitationId[61]
    const std::int32_t flag = 1;
    std::memcpy(event.data + 112, &flag, 4);
    const auto& name = online::identity().online_id; // Accepter, as pinned NpHandler does.
    std::memcpy(event.data + 120, name.data(), std::min<std::size_t>(16, name.size()));
    // Retail EU 1.21 copies 176 payload bytes at 0xb89dc4; 0xa46691 compares
    // object+0x9c (payload+140) with the signed-in local user ID. NP's online
    // implementation currently exposes local user 1 exclusively.
    const std::int32_t local_user = 1;
    std::memcpy(event.data + 140, &local_user, sizeof(local_user));
    events.push_back(event);
    delivered.push_back(outcome.invitation);
    if (delivered.size() > 256) delivered.pop_front();
    ctr_hle::log_call("libSceSystemService", "SESSION_INVITATION_accepted", 0);
}
void tick() {
    std::unique_lock lock{service_mutex, std::try_to_lock};
    if (!lock.owns_lock()) return;
    try {
        if (notice.valid()) {
            if (notice.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
            auto outcome = notice.get();
            if (owner) owner(2);
            post_accepted(outcome);
        }
        const auto now = std::chrono::steady_clock::now();
        if (polling.valid() && polling.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            const auto pending = polling.get();
            for (auto it = notified.begin(); it != notified.end();) {
                const bool present =
                    std::any_of(pending.begin(), pending.end(), [&](const Pending& entry) { return entry.id == *it; });
                it = present ? std::next(it) : notified.erase(it);
            }
            const auto candidate = std::find_if(pending.begin(), pending.end(), [&](const Pending& entry) {
                if (notified.count(entry.id)) return false;
                std::scoped_lock event_lock{events_mutex};
                return std::find(delivered.begin(), delivered.end(), entry.id) == delivered.end();
            });
            if (candidate != pending.end() && owner && owner(0) && owner(1)) {
                const auto entry = *candidate;
                try {
                    notice = std::async(std::launch::async, [entry] {
                        try {
                            notice_thread = GetCurrentThreadId();
                            const auto message = ui_format(Text::JoinQuestion, wide(entry.sender));
                            const int choice = MessageBoxW(nullptr, message.c_str(), title, MB_YESNO | MB_ICONQUESTION);
                            notice_thread = 0;
                            if (!choice) throw std::runtime_error("invitation prompt unavailable");
                            for (;;) {
                                try {
                                    if (choice == IDYES) return accept(entry);
                                    exchange("DELETE", "/ctr/v1/session-invitations/" + entry.id, "", 204);
                                    return Outcome{};
                                } catch (const std::exception& error) {
                                    log_failure(error);
                                    const auto message = std::wstring(ui_text(choice == IDYES ? Text::AcceptFailed
                                                                                              : Text::DeclineFailed)) +
                                                         L"\n\n" + wide(error.what()) + L"\n\n" +
                                                         ui_text(Text::RetryHint);
                                    notice_thread = GetCurrentThreadId();
                                    const int retry = MessageBoxW(nullptr, message.c_str(), ui_text(Text::ErrorTitle),
                                                                  MB_RETRYCANCEL | MB_ICONERROR);
                                    notice_thread = 0;
                                    if (retry != IDRETRY) {
                                        Outcome result;
                                        result.error = failure;
                                        return result;
                                    }
                                }
                            }
                        } catch (const std::exception& error) {
                            notice_thread = 0;
                            log_failure(error);
                            Outcome result;
                            result.error = failure;
                            return result;
                        }
                    });
                    notified.insert(entry.id);
                } catch (...) {
                    owner(2);
                    throw;
                }
            }
        }
        if (!notice.valid() && !polling.valid() && now >= next_poll) {
            next_poll = now + std::chrono::seconds(3);
            polling = std::async(std::launch::async, fetch);
        }
    } catch (const std::exception& error) {
        next_poll = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        log_failure(error);
    }
}
int CTR_SYSV system_status(void* output) {
    if (!output) return static_cast<int>(0x80A10003u);
    tick();
    std::scoped_lock lock{events_mutex};
    auto* bytes = static_cast<unsigned char*>(output);
    const auto count = static_cast<std::int32_t>(events.size());
    std::memcpy(bytes, &count, 4);
    bytes[4] = notice_thread.load() != 0 || dialog_thread.load() != 0;
    bytes[5] = 0;
    bytes[6] = 1;
    bytes[7] = 0;
    bytes[8] = 0;
    return 0;
}
int CTR_SYSV receive_event(Event* event) {
    if (!event) return static_cast<int>(0x80A10003u);
    std::scoped_lock lock{events_mutex};
    if (events.empty()) return static_cast<int>(0x80A10004u);
    *event = events.front();
    events.pop_front();
    ctr_hle::log_call("libSceSystemService", "sceSystemServiceReceiveEvent", 0);
    return 0;
}
CTR_HLE_REGISTER_ONLINE("rPo6tV8D9bM", "libSceSystemService", system_status);
CTR_HLE_REGISTER_ONLINE("656LMQSrg6U", "libSceSystemService", receive_event);
}
CTR_HLE_API void ctr_hle_invitation_set_owner(int (*operation)(int)) {
    ctr_hle::invitation::set_owner(operation);
}
