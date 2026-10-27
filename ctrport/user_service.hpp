// SPDX-License-Identifier: GPL-2.0-only
// Independent local service lifecycle. Public null-init usage:
// Scene-Collective/ps4-payload-sdk libPS4/source/sysutil.c getUserIDList.
// Error values from pinned shadPS4 userservice_error.h (GPL-2.0-or-later),
// revision e3ce810f3a653f43ac64ebab63023de281a4103a. No upstream dummy copied.
#pragma once
#include "settings/hot_trace.hpp"
#include <mutex>
#include <memory>
#include <stdexcept>
#include <array>
#include <deque>
#include <string>
#include "kernel_time.hpp"
namespace ctrport {
// Local split-screen players 2..4 (controller ports 1..3) are further local users with fixed
// identities 101..103, logged in and out by the pad backend as their controllers join and
// leave. Event layout {type,user}: type 0 login, 1 logout (pinned shadPS4 userservice.h
// OrbisUserServiceEventType). The primary user's login stays the first event.
class UserService {
    struct Event {
        std::int32_t type, user;
    };
    struct Session {
        int local_user_id;
        bool login_pending = true;
        std::string name;
        std::array<bool, 4> extra{};
        std::deque<Event> events;
    }; // Runtime-owned identity, no external account.
    int next_local_user_id = 1;
    std::mutex lock;
    std::unique_ptr<Session> session;
    static constexpr int extra_base = 100;
    static int port_of(int id) { return id > extra_base && id <= extra_base + 3 ? id - extra_base : 0; }

public:
    static constexpr int extra_user_id(int port) { return extra_base + port; }
    // Logs the local player of controller port 1..3 in or out; false when nothing changed.
    bool set_extra_user(int port, bool login) {
        if (port < 1 || port > 3) throw std::runtime_error("Local user port out of range");
        std::lock_guard<std::mutex> guard(lock);
        if (!session || session->extra[port] == login) return false;
        session->extra[port] = login;
        session->events.push_back({login ? 0 : 1, extra_user_id(port)});
        return true;
    }
    // Controller port of a logged-in local user: 0 primary, 1..3 split-screen players, -1 none.
    int port_of_user(int id) {
        std::lock_guard<std::mutex> guard(lock);
        if (!session) return -1;
        if (id == session->local_user_id) return 0;
        const int port = port_of(id);
        return port && session->extra[port] ? port : -1;
    }
    int initialize(const void* params) {
        if (params) throw std::runtime_error("User service priority parameters unsupported");
        std::lock_guard<std::mutex> guard(lock);
        if (session) return static_cast<int>(0x80960003U);
        if (next_local_user_id > extra_base) throw std::runtime_error("Local user identity space exhausted");
        char configured[17]{};
        const DWORD length = GetEnvironmentVariableA("CTR_ONLINE_ID", configured, sizeof(configured));
        const std::string name = length >= 3 && length <= 16 ? std::string(configured, length) : std::string{};
        const bool valid = !name.empty() &&
                           name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") ==
                               std::string::npos;
        session = std::make_unique<Session>(Session{next_local_user_id++, true, valid ? name : "CTRPlayer"});
        return 0;
    }
    int login_user_list(std::int32_t* output) {
        if (!output) return static_cast<int>(0x80960005U);
        require_time_output(output, 4 * sizeof(*output));
        std::lock_guard<std::mutex> guard(lock);
        if (!session) return static_cast<int>(0x80960002U);
        std::array<std::int32_t, 4> ids{session->local_user_id, -1, -1, -1};
        std::size_t count = 1;
        for (int port = 1; port <= 3; ++port)
            if (session->extra[port]) ids[count++] = extra_user_id(port);
        std::memcpy(output, ids.data(), sizeof(ids));
        return 0;
    }
    // Public ABI/null behavior: pinned userservice.cpp513; identity comes from
    // this runtime's real initialized offline session, never an invented account.
    int initial_user(std::int32_t* output) {
        if (!output) return static_cast<int>(0x80960005U);
        require_time_output(output, sizeof(*output));
        std::lock_guard<std::mutex> guard(lock);
        if (!session) return static_cast<int>(0x80960002U);
        *output = session->local_user_id;
        return 0;
    }
    int get_event(std::int32_t* output) {
        if (!output) return static_cast<int>(0x80960005U);
        require_time_output(output, 2 * sizeof(*output));
        std::lock_guard<std::mutex> guard(lock);
        if (!session) return static_cast<int>(0x80960002U);
        if (session->login_pending) {
            const std::array<std::int32_t, 2> event{0, session->local_user_id};
            std::memcpy(output, event.data(), sizeof(event));
            session->login_pending = false;
            return 0;
        }
        if (session->events.empty()) return static_cast<int>(0x80960007U);
        const std::array<std::int32_t, 2> event{session->events.front().type, session->events.front().user};
        std::memcpy(output, event.data(), sizeof(event));
        session->events.pop_front();
        return 0;
    }
    int get_name(int id, char* output, size_t size) {
        if (id == -1 || !output) return static_cast<int>(0x80960005U);
        std::lock_guard<std::mutex> guard(lock);
        if (!session) return static_cast<int>(0x80960002U);
        const int port = port_of(id);
        if (id != session->local_user_id && !(port && session->extra[port])) return static_cast<int>(0x80960009U);
        const std::string name = id == session->local_user_id ? session->name : "Player " + std::to_string(port + 1);
        const auto bytes = name.size() + 1;
        if (size < bytes) return static_cast<int>(0x8096000aU);
        require_time_output(output, bytes);
        std::memcpy(output, name.c_str(), bytes);
        return 0;
    }
    int terminate() {
        std::lock_guard<std::mutex> guard(lock);
        if (!session) return static_cast<int>(0x80960002U);
        session.reset();
        return 0;
    }
    bool has_user(int id) { return port_of_user(id) >= 0; }
    bool initialized() {
        std::lock_guard<std::mutex> guard(lock);
        return bool(session);
    }
};
inline UserService user_service;
// ABI, null/short-buffer behavior: pinned shadPS4 userservice.cpp1098-1131,
// revision e3ce810f3a653f43ac64ebab63023de281a4103a; independent owned-session implementation.
extern "C" int ctr_user_name(int id, char* output, size_t size, std::uint64_t caller) {
    try {
        return user_service.get_name(id, output, size);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=1xxcMiGu2fo library=libSceUserService caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_user_name();
// Offline account has no PSN identity: error without writing a fabricated ID.
// Public ABI/errors: pinned np_manager.cpp594-615 and np_error.h, GPL-2.0-or-later.
inline int offline_np_id(int id, void* output) {
    if (id == -1 || !output) return static_cast<int>(0x80550003U);
    if (!user_service.has_user(id)) return static_cast<int>(0x80550007U);
    return static_cast<int>(0x80550006U);
}
extern "C" int ctr_np_id(int id, void* output, std::uint64_t caller) {
    const int result = offline_np_id(id, output);
    std::printf("{\"scope\":\"original_NpGetNpId_return\",\"caller\":%llu,\"result\":%d,\"output_written\":false}\n",
                static_cast<unsigned long long>(caller), result);
    std::fflush(stdout);
    return result;
}
extern "C" void ctr_sysv_np_id();
inline int user_name_control(const char* expected = "CTRPlayer") {
    UserService service;
    char output[19];
    std::memset(output, 0x5a, sizeof(output));
    const auto bytes = std::strlen(expected) + 1;
    if (bytes > 17) return 8;
    if (service.get_name(1, output + 1, bytes) != static_cast<int>(0x80960002U) || service.initialize(nullptr))
        return 1;
    if (service.get_name(2, output + 1, bytes) != static_cast<int>(0x80960009U) ||
        service.get_name(1, output + 1, bytes - 1) != static_cast<int>(0x8096000aU) || output[1] != 0x5a)
        return 2;
    if (service.get_name(1, output + 1, bytes) || std::strcmp(output + 1, expected) || output[0] != 0x5a ||
        output[bytes + 1] != 0x5a)
        return 3;
    if (user_service.initialize(nullptr)) return 4;
    std::memset(output, 0x5a, sizeof(output));
    if (ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_user_name), 1,
                                   reinterpret_cast<std::uint64_t>(output + 1), bytes) ||
        std::strcmp(output + 1, expected) || output[0] != 0x5a || output[bytes + 1] != 0x5a)
        return 5;
    if (static_cast<int>(ctr_call_original_integer3(reinterpret_cast<void*>(&ctr_sysv_np_id), 1,
                                                    reinterpret_cast<std::uint64_t>(output), 0)) !=
            static_cast<int>(0x80550006U) ||
        output[0] != 0x5a)
        return 6;
    if (offline_np_id(-1, output) != static_cast<int>(0x80550003U) ||
        offline_np_id(2, output) != static_cast<int>(0x80550007U))
        return 7;
    std::cout << "{\"scope\":\"owned_user_name\",\"passed\":true}\n";
    return 0;
}
extern "C" int ctr_user_login_list(std::int32_t* output, std::uint64_t caller) {
    try {
        return user_service.login_user_list(output);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=fPhymKNvK-A library=libSceUserService caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_user_login_list();
extern "C" int ctr_user_event(std::int32_t* output, uint64_t caller) {
    try {
        const int result = user_service.get_event(output);
        if (ctrport::settings::hot_trace()) {
            std::printf("{\"scope\":\"original_UserServiceGetEvent_return\",\"caller\":%llu,\"result\":%d}\n",
                        static_cast<unsigned long long>(caller), result);
            std::fflush(stdout);
        }
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=yH17Q6NWtVg library=libSceUserService caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_user_event();
extern "C" int ctr_user_initial(std::int32_t* output, uint64_t caller) {
    try {
        const int result = user_service.initial_user(output);
        std::printf("{\"scope\":\"original_UserServiceGetInitialUser_return\",\"caller\":%llu,\"result\":%d}\n",
                    static_cast<unsigned long long>(caller), result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=CdWp0oHWGr0 library=libSceUserService caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_user_initial();

// Retail NpToolkit2 SHA480a769d...e5c9c: call0x41702 passes function in
// RDI and context in RSI; callback0x41470 consumes an event pointer.
// Registration only; no fabricated login events or arbitrary callback dispatch.
inline std::mutex user_callback_lock;
inline void* user_callback_function = nullptr;
inline void* user_callback_argument = nullptr;
inline int user_callback_register_owned(void* function, void* argument) {
    if (!function) throw std::runtime_error("Null user callback contract not characterized");
    std::lock_guard<std::mutex> lock(user_callback_lock);
    if (user_callback_function) throw std::runtime_error("Multiple user callback registration not characterized");
    user_callback_function = function;
    user_callback_argument = argument;
    return 0;
}

}
