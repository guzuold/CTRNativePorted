// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "settings/hot_trace.hpp"
#include "guest_worker.hpp"
#include <deque>
#include "thread_create_domain.hpp"
namespace ctrport {
inline std::shared_ptr<std::vector<MappedImage>> startup_worker_images;
inline std::shared_ptr<const TlsStorage> startup_worker_tls;
inline std::vector<ImageMetadata> startup_worker_metadata;
inline std::mutex startup_worker_lock;
inline std::vector<std::shared_ptr<GuestWorkerControl>> startup_workers;
inline std::shared_ptr<GuestWorkerControl> find_startup_worker(std::uint64_t identity) {
    if (!identity) return {};
    std::lock_guard<std::mutex> guard(startup_worker_lock);
    for (const auto& worker : startup_workers)
        if (worker->identity() == identity) return worker;
    return {};
}
inline std::size_t reap_startup_workers(std::size_t scan_limit = 16) {
    if (!scan_limit || scan_limit > 128) throw std::runtime_error("Worker reaper scan bound rejected");
    std::vector<std::shared_ptr<GuestWorkerControl>> retired;
    {
        std::lock_guard<std::mutex> guard(startup_worker_lock);
        std::size_t scanned = 0;
        for (auto it = startup_workers.begin(); it != startup_workers.end() && scanned < scan_limit; ++scanned) {
            if ((*it)->has_exited() && (*it)->completion_collected()) {
                retired.push_back(std::move(*it));
                it = startup_workers.erase(it);
            } else
                ++it;
        }
    }
    // Host handle/stack/image teardown happens after releasing the registry mutex.
    return retired.size();
}
// Independent join bridge, public shadPS4 pthread.cpp112-209 at
// e3ce810f3a653f43ac64ebab63023de281a4103a, GPL-2.0-or-later contract.
inline int join_startup_worker(std::uint64_t identity, std::uint64_t* output) {
    if (!identity) return int(0x80020016U);
    if (identity == thread_self()) return int(0x80020023U);
    auto worker = find_startup_worker(identity);
    if (!worker) return int(0x80020003U);
    if (output) require_time_output(output, 8);
    if (worker->completion_collected()) return int(0x80020016U);
    if (worker->has_joiner()) return int(0x8002002dU);
    const auto value = worker->join();
    if (output) *output = value;
    std::cout << "{\"scope\":\"owned_worker_join_return\",\"identity\":" << identity
              << ",\"result_written\":" << (output ? "true" : "false") << "}\n"
              << std::flush;
    return 0;
}
extern "C" int ctr_thread_join_owned(std::uint64_t identity, std::uint64_t* output, std::uint64_t caller) {
    try {
        return join_startup_worker(identity, output);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=onNY9Byn-W8 library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_thread_join_owned();
extern "C" int ctr_thread_detach_owned(std::uint64_t identity, std::uint64_t caller) {
    try {
        if (!identity) return static_cast<int>(0x80020016U);
        auto worker = find_startup_worker(identity);
        if (!worker) return static_cast<int>(0x80020003U);
        return worker->detach() ? 0 : static_cast<int>(0x80020016U);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=4qGrR6eoP9Y library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_thread_detach_owned();
inline int set_owned_worker_priority(uint64_t handle, int priority) {
    if (priority < 256 || priority > 767) return static_cast<int>(0x80020016U);
    auto worker = find_startup_worker(handle);
    if (!worker) return static_cast<int>(0x80020003U);
    if (worker->has_exited()) return static_cast<int>(0x80020003U);
    ThreadAttributes attributes;
    attributes.priority = priority;
    worker->update_host_priority(startup_windows_priority(attributes), priority);
    return 0;
}
extern "C" int ctr_thread_set_priority(uint64_t handle, int priority, uint64_t caller) {
    try {
        int rc = set_owned_worker_priority(handle, priority);
        if (ctrport::settings::hot_trace()) {
            std::printf(
                "{\"scope\":\"original_PthreadSetprio_return\",\"caller\":%llu,\"priority\":%d,\"result\":%d,\"scheduler_equivalence\":false}\n",
                static_cast<unsigned long long>(caller), priority, rc);
            std::fflush(stdout);
        }
        return rc;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=W0Hpm2X0uPE library=libkernel caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}

// Independent Windows scheduling adapter; public pthread.cpp570-600 signature.
// Actual host priority change/readback; never claim PS4 FIFO/RR quantum equivalence.
inline int posix_set_sched(std::uint64_t handle, int policy, const int* parameter) {
    if (!handle || !parameter) return 22;
    require_time_output(parameter, 4);
    const int priority = *parameter;
    if ((policy != 1 && policy != 3) || priority < 256 || priority > 767) return 22;
    std::shared_ptr<ThreadIdentity> identity;
    {
        std::lock_guard<std::mutex> lock(thread_identity_lock);
        const auto f = thread_identities.find(handle);
        if (f == thread_identities.end()) return 3;
        identity = f->second;
    }
    ThreadAttributes attributes;
    attributes.priority = priority;
    const int host = startup_windows_priority(attributes);
    if (identity->host == std::this_thread::get_id()) {
        if (!SetThreadPriority(GetCurrentThread(), host) || GetThreadPriority(GetCurrentThread()) != host)
            throw std::runtime_error("Current Windows priority application/readback failed");
        identity->priority_override.store(priority);
    } else {
        auto worker = find_startup_worker(handle);
        if (!worker || worker->has_exited()) return 3;
        worker->update_host_priority(host, priority);
    }
    identity->guest_policy.store(policy);
    return 0;
}
extern "C" int ctr_posix_set_sched(std::uint64_t handle, int policy, const int* parameter, std::uint64_t caller) {
    try {
        const auto result = posix_set_sched(handle, policy, parameter);
        std::printf(
            "{\"scope\":\"owned_posix_setschedparam\",\"nid\":\"Xs9hdiD7sAA\",\"library\":\"libScePosix\",\"caller\":%llu,\"policy\":%d,\"priority\":%d,\"result\":%d,\"ps4_scheduler_equivalence\":false}\n",
            static_cast<unsigned long long>(caller), policy, (result == 0 && parameter) ? *parameter : -1, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=Xs9hdiD7sAA library=libScePosix caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_posix_set_sched();
extern "C" void ctr_sysv_thread_set_priority();
inline int create_owned_startup_thread(std::uint64_t* output, const std::uint64_t* attributes, void* target,
                                       void* argument, const char* name, std::uint64_t caller, bool posix) {
    try {
        validate_startup_thread_output(output);
        auto value = snapshot_create_attributes(attributes);
        if (value.inherit == 4) {
            value.inherit = 0;
            value.priority = effective_guest_priority();
        }
        std::cout << "{\"scope\":\"startup_create_actual_attributes\",\"policy\":" << value.policy
                  << ",\"priority\":" << value.priority << ",\"inherit\":" << value.inherit
                  << ",\"stack_size\":" << value.stack_size << ",\"guard_size\":" << value.guard_size
                  << ",\"detached\":" << value.detached << ",\"suspended\":" << value.suspended
                  << ",\"system_scope\":" << value.system_scope << "}\n"
                  << std::flush;
        if (startup_worker_images && !startup_worker_images->empty() &&
            target == startup_worker_images->front().get() + 0x1409af0) {
            const auto address = reinterpret_cast<std::uintptr_t>(argument);
            if (!address || address > std::numeric_limits<std::uintptr_t>::max() - 0x5c)
                throw std::runtime_error("Original worker selector address rejected");
            int selector = 0;
            SIZE_T read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address + 0x58), &selector,
                                   sizeof(selector), &read) ||
                read != sizeof(selector))
                throw std::runtime_error("Original worker selector unreadable");
            std::cout << "{\"scope\":\"original_startup_affinity_selector\",\"object_offset\":88,\"selector\":"
                      << selector << ",\"worker_entered\":false}\n"
                      << std::flush;
        }
        validate_startup_create_domain(value);
        if (!startup_worker_images || !startup_worker_tls) throw std::runtime_error("Startup worker context missing");
        if (startup_worker_metadata.empty()) throw std::runtime_error("Worker image metadata missing");
        const auto target_image = resolve_owned_callback(*startup_worker_images, startup_worker_metadata,
                                                         startup_worker_metadata.front().sha256, target);
        auto& image = (*startup_worker_images)[target_image];
        std::cout << "{\"scope\":\"startup_callback_image\",\"image_sha256\":\""
                  << startup_worker_metadata[target_image].sha256
                  << "\",\"entry_offset\":" << (static_cast<const unsigned char*>(target) - image.get()) << "}\n"
                  << std::flush;
        (void)name; // Naming has no guest execution effect; do not read an untrusted string.
        auto worker = std::make_shared<GuestWorkerControl>(startup_worker_tls, startup_worker_images, target, argument,
                                                           static_cast<std::size_t>(value.stack_size),
                                                           static_cast<std::size_t>(value.guard_size));
        worker->set_host_priority(startup_windows_priority(value), value.priority);
        // Unrestricted default uses the actual process mask; explicit masks stay strict.
        if (value.cpu_set_size) {
            const auto host = startup_host_affinity(value.cpu_set);
            worker->set_host_affinity(host);
            std::cout << "{\"scope\":\"startup_affinity_applied\",\"guest_mask\":" << value.cpu_set
                      << ",\"host_mask\":" << host
                      << ",\"unrestricted_default\":" << (value.cpu_set == ~std::uint64_t{0} ? "true" : "false")
                      << "}\n"
                      << std::flush;
        }
        const auto identity = worker->identity();
        if (!identity) throw std::runtime_error("Worker identity unavailable");
        if (value.cpu_set_size) record_thread_affinity(identity, value.cpu_set);
        std::lock_guard<std::mutex> guard(startup_worker_lock);
        startup_workers.push_back(std::move(worker));
        *output = identity;
        std::cout << "{\"scope\":\"original_startup_worker_publish\",\"entry_offset\":"
                  << static_cast<const unsigned char*>(target) - image.get() << ",\"identity\":" << identity
                  << ",\"handle_before_start\":true,\"scheduler_equivalence\":false}\n"
                  << std::flush;
        startup_workers.back()->publish_and_start();
        if (value.detached && !startup_workers.back()->detach())
            throw std::runtime_error("Initial worker detach transition failed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=%s library=%s caller=0x%llx unsupported: %s\n", posix ? "Jmi+9w9u0E4" : "6UgtwV+0zb4",
                     posix ? "libScePosix" : "libkernel", static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
// Independent registration storage; public net_ctl_obj.cpp32-46, net_ctl_obj.h
// eight-slot contract. GPL-2.0-or-later reference; no callback dispatch claimed.
struct NetToolkitCallback {
    void* function = nullptr;
    void* argument = nullptr;
};
inline std::mutex net_toolkit_callback_lock;
inline std::array<NetToolkitCallback, 8> net_toolkit_callbacks{};
inline int register_net_toolkit_callback(void* function, void* argument, int* output) {
    if (!function || !output) return static_cast<int>(0x80412107U);
    require_time_output(output, sizeof(*output));
    std::lock_guard<std::mutex> lock(net_toolkit_callback_lock);
    for (std::size_t i = 0; i < net_toolkit_callbacks.size(); ++i)
        if (!net_toolkit_callbacks[i].function) {
            net_toolkit_callbacks[i] = {function, argument};
            *output = static_cast<int>(i);
            return 0;
        }
    return static_cast<int>(0x80412103U);
}
extern "C" int ctr_net_toolkit_register(void* function, void* argument, int* output, std::uint64_t caller) {
    try {
        if (function && output) {
            if (!startup_worker_images || startup_worker_metadata.empty())
                throw std::runtime_error("Callback image ownership unavailable");
            resolve_owned_callback(*startup_worker_images, startup_worker_metadata,
                                   startup_worker_metadata.front().sha256, function);
        }
        const int result = register_net_toolkit_callback(function, argument, output);
        std::printf("{\"scope\":\"net_toolkit_callback_register\",\"caller\":%llu,\"result\":%d,\"id\":%d}\n",
                    static_cast<unsigned long long>(caller), result, result ? -1 : *output);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=wIsKy+TfeLs library=libSceNetCtlForNpToolkit caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
// Public net_ctl_obj.cpp59-70 dispatches the actual offline disconnected
// condition (event1) to retained callbacks. No online state is synthesized.
inline void dispatch_net_toolkit_offline(const std::array<NetToolkitCallback, 8>& callbacks) {
    for (const auto& cb : callbacks)
        if (cb.function) ctr_call_original_integer2(cb.function, 1, reinterpret_cast<std::uint64_t>(cb.argument));
}
extern "C" int ctr_net_toolkit_check(std::uint64_t caller) {
    try {
        std::array<NetToolkitCallback, 8> snapshot;
        {
            std::lock_guard<std::mutex> lock(net_toolkit_callback_lock);
            snapshot = net_toolkit_callbacks;
        }
        for (const auto& cb : snapshot)
            if (cb.function) {
                if (!startup_worker_images || startup_worker_metadata.empty())
                    throw std::runtime_error("Callback image owner unavailable");
                resolve_owned_callback(*startup_worker_images, startup_worker_metadata,
                                       startup_worker_metadata.front().sha256, cb.function);
            }
        dispatch_net_toolkit_offline(snapshot);
        std::printf("{\"scope\":\"net_toolkit_offline_callback_dispatch\",\"caller\":%llu,\"event\":1,\"result\":0}\n",
                    static_cast<unsigned long long>(caller));
        std::fflush(stdout);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=u5oqtlIP+Fw library=libSceNetCtlForNpToolkit caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_offline_callback_fixture();
extern "C" int ctr_thread_create(std::uint64_t* output, const std::uint64_t* attributes, void* target, void* argument,
                                 const char* name, std::uint64_t caller) {
    return create_owned_startup_thread(output, attributes, target, argument, name, caller, false);
}
extern "C" int ctr_posix_thread_create(std::uint64_t* output, const std::uint64_t* attributes, void* target,
                                       void* argument, const char* name, std::uint64_t caller) {
    return create_owned_startup_thread(output, attributes, target, argument, name, caller, true);
}
extern "C" void ctr_sysv_posix_thread_create();
extern "C" void ctr_sysv_thread_create();
extern "C" int ctr_user_callback_register(void* function, void* argument, std::uint64_t caller) {
    try {
        if (!startup_worker_images || startup_worker_metadata.empty())
            throw std::runtime_error("User callback image owner unavailable");
        const auto index = resolve_owned_callback(*startup_worker_images, startup_worker_metadata,
                                                  startup_worker_metadata.front().sha256, function);
        const auto offset = static_cast<unsigned char*>(function) - (*startup_worker_images)[index].get();
        const int result = user_callback_register_owned(function, argument);
        std::printf(
            "{\"scope\":\"user_callback_registered\",\"caller\":%llu,\"image_sha256\":\"%s\",\"offset\":%llu,\"result\":%d}\n",
            static_cast<unsigned long long>(caller), startup_worker_metadata[index].sha256.c_str(),
            static_cast<unsigned long long>(offset), result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=wuI7c7UNk0A library=libSceUserServiceForNpToolkit caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}

// Independent state storage: public pinned np_manager.cpp1049-1067,
// GPL-2.0-or-later. Registration lifecycle only; no online state fabricated.
inline std::mutex np_toolkit_callback_lock;
inline void* np_toolkit_callback = nullptr;
inline void* np_toolkit_argument = nullptr;
inline int np_toolkit_register(void* function, void* argument) {
    std::lock_guard<std::mutex> guard(np_toolkit_callback_lock);
    np_toolkit_callback = function;
    np_toolkit_argument = argument;
    return 0;
}
inline int np_toolkit_unregister() {
    std::lock_guard<std::mutex> guard(np_toolkit_callback_lock);
    if (!np_toolkit_callback) return static_cast<int>(0x80550009U);
    np_toolkit_callback = nullptr;
    np_toolkit_argument = nullptr;
    return 0;
}
extern "C" int ctr_np_toolkit_register(void* function, void* argument, std::uint64_t caller) {
    try {
        if (function) {
            if (!startup_worker_images || startup_worker_metadata.empty())
                throw std::runtime_error("NP callback image ownership unavailable");
            resolve_owned_callback(*startup_worker_images, startup_worker_metadata,
                                   startup_worker_metadata.front().sha256, function);
        }
        const int rc = np_toolkit_register(function, argument);
        std::printf("{\"scope\":\"np_toolkit_callback_register\",\"caller\":%llu,\"result\":%d}\n",
                    static_cast<unsigned long long>(caller), rc);
        std::fflush(stdout);
        return rc;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=0c7HbXRKUt4 library=libSceNpManagerForToolkit caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" int ctr_np_toolkit_unregister(std::uint64_t caller) {
    const int rc = np_toolkit_unregister();
    std::printf("{\"scope\":\"np_toolkit_callback_unregister\",\"caller\":%llu,\"result\":%d}\n",
                static_cast<unsigned long long>(caller), rc);
    std::fflush(stdout);
    return rc;
}
// Public np_manager.cpp902-979: drain queued state events, dispatch snapshot
// on calling thread. Offline runtime has no PSN state producer; never invent one.
inline std::deque<std::pair<int, int>> np_toolkit_pending_events;
inline std::size_t dispatch_np_toolkit_pending() {
    std::deque<std::pair<int, int>> pending;
    void* callback = nullptr;
    void* argument = nullptr;
    {
        std::lock_guard<std::mutex> lock(np_toolkit_callback_lock);
        pending.swap(np_toolkit_pending_events);
        callback = np_toolkit_callback;
        argument = np_toolkit_argument;
    }
    for (const auto& event : pending)
        if (callback)
            ctr_call_original_integer3(callback, static_cast<std::uint64_t>(event.first),
                                       static_cast<std::uint64_t>(event.second),
                                       reinterpret_cast<std::uint64_t>(argument));
    return pending.size();
}
inline int np_check_pending(std::uint64_t caller, const char* nid, const char* library, const char* scope) {
    try {
        {
            std::lock_guard<std::mutex> lock(np_toolkit_callback_lock);
            if (np_toolkit_callback && !np_toolkit_pending_events.empty()) {
                if (!startup_worker_images || startup_worker_metadata.empty())
                    throw std::runtime_error("NP callback image owner unavailable");
                resolve_owned_callback(*startup_worker_images, startup_worker_metadata,
                                       startup_worker_metadata.front().sha256, np_toolkit_callback);
            }
        }
        const auto count = dispatch_np_toolkit_pending();
        std::printf("{\"scope\":\"%s\",\"caller\":%llu,\"events_consumed\":%llu,\"result\":0}\n", scope,
                    static_cast<unsigned long long>(caller), static_cast<unsigned long long>(count));
        std::fflush(stdout);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=%s library=%s caller=0x%llx unsupported: %s\n", nid, library,
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" int ctr_np_toolkit_check(std::uint64_t caller) {
    return np_check_pending(caller, "JELHf4xPufo", "libSceNpManagerForToolkit", "np_toolkit_event_queue_checked");
}
// Public np_manager.cpp964-973: same state queue plus registered async jobs.
// Async request/registration producers remain missing and cannot create jobs;
// do not manufacture completion or online state in this offline domain.
extern "C" int ctr_np_check(std::uint64_t caller) {
    return np_check_pending(caller, "3Zl8BePTh9Y", "libSceNpManager", "np_general_event_queue_checked");
}
extern "C" void ctr_sysv_np_check();
extern "C" void ctr_sysv_np_callback_fixture();

}
