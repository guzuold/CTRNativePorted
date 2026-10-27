// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_integer_dispatch.hpp"
#include "gnm_executor_host.hpp"
namespace ctrport {
inline std::mutex gnm_pending_mutex;
inline std::vector<std::unique_ptr<ServiceIntegerThunk>> gnm_pending_thunks;
inline CtrHostApi gnm_reload_host{sizeof(CtrHostApi), CTRPORT_SERVICE_ABI_VERSION, &gnm_executor_host_api, nullptr};
struct GnmServicePublisher {
    std::atomic<bool> stop{false};
    std::thread worker;
    ~GnmServicePublisher() {
        stop.store(true);
        if (worker.joinable()) worker.join();
    }
};
inline GnmServicePublisher gnm_service_publisher;
inline void start_gnm_service_publisher(const std::string& pin) {
    if (gnm_service_publisher.worker.joinable()) return;
    const auto root = std::filesystem::absolute("runtime/user/services/gnm");
    std::filesystem::create_directories(root);
    gnm_service_publisher.worker = std::thread([root, pin] {
        uint64_t serial = 1;
        std::filesystem::file_time_type rejected{};
        while (!gnm_service_publisher.stop.load()) {
            try {
                const auto file = root / ("service_gnm_" + std::to_string(serial) + ".dll");
                if (std::filesystem::exists(file)) {
                    const auto modified = std::filesystem::last_write_time(file);
                    if (modified != rejected) {
                        try {
                            load_service_generation(integer9_dispatch_generations, root, file, gnm_reload_host, pin,
                                                    serial);
                            std::fprintf(stderr, "Gnm service9 generation published serial=%llu\n",
                                         static_cast<unsigned long long>(serial));
                            ++serial;
                            rejected = {};
                        } catch (const std::exception& error) {
                            rejected = modified;
                            std::fprintf(stderr, "Gnm service9 rejected generation: %s\n", error.what());
                        }
                        std::fflush(stderr);
                    }
                }
            } catch (const std::exception& error) {
                std::fprintf(stderr, "Gnm service9 watcher failure: %s\n", error.what());
                std::fflush(stderr);
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
}
// Redirect only this trapped context; never mutate a live retail import slot.
// Nine scalar arguments become resident owned values before the service wait.
// Guest buffers remain guest pointers: no GPU snapshot/coherence claim here.
inline uint64_t prepare_gnm_pending(const ImageMetadata& image, uint64_t slot, const std::string& pin,
                                    const uint64_t args[5], uint64_t caller) {
    uint64_t extent = 0;
    for (const auto& span : image.spans) extent = (std::max)(extent, span.second);
    const OriginalSymbol* selected = nullptr;
    for (const auto& symbol : image.imports)
        if (symbol.slot == slot) {
            if (selected) throw std::runtime_error("Ambiguous Gnm pending slot");
            selected = &symbol;
        }
    if (!selected || selected->nid != "xbxNatawohc" || selected->library != "libSceGnmDriver" || selected->type != 2 ||
        selected->relocation != 7 || selected->addend)
        throw std::runtime_error("Gnm pending import identity differs");
    ServiceImportRecord record{pin, image.sha256, selected->nid, selected->library, selected->module, slot, 9};
    const auto token = integer_dispatch_records.add(record, pin, image.sha256, extent);
    auto thunk = std::make_unique<ServiceIntegerThunk>(token, 9);
    const auto target = reinterpret_cast<uint64_t>(thunk->target());
    if (args[0] > UINT32_MAX || gnm_executor_host_owner.retain_pending(caller, static_cast<uint32_t>(args[0]), args[1],
                                                                       args[2], args[3], args[4]))
        throw std::runtime_error("Gnm pending command ownership capture rejected");
    std::lock_guard<std::mutex> guard(gnm_pending_mutex);
    gnm_pending_thunks.push_back(std::move(thunk));
    start_gnm_service_publisher(pin);
    return target;
}
}
