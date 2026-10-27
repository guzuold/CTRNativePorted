// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_slot_admission.hpp"
#include "service_integer_dispatch.hpp"
#include "service_write_host.hpp"
#include "service_dll.hpp"
#include <thread>
#include <atomic>
namespace ctrport {
// Process-resident ownership: original workers may outlive the entry launcher.
inline std::vector<std::unique_ptr<ServiceIntegerThunk>> startup_write_thunks;
inline CtrWriteHostApi startup_write_api{};
inline CtrHostApi startup_write_host{};
struct StartupWritePublisher {
    std::atomic<bool> stop{false};
    std::thread worker;
    ~StartupWritePublisher() {
        stop.store(true);
        if (worker.joinable()) worker.join();
    }
};
// Destroy/join publisher before destroying the contexts declared above.
inline StartupWritePublisher startup_write_publisher;
inline void install_startup_write_reload(std::vector<MappedImage>& images, const std::vector<ImageMetadata>& metadata,
                                         const std::string& pin) {
    char directory[32768]{};
    const auto size = GetEnvironmentVariableA("CTR_STARTUP_WRITE_DLL_DIR", directory, sizeof(directory));
    if (!size) return;
    if (size >= sizeof(directory)) throw std::runtime_error("Startup DLL directory too long");
    const std::filesystem::path root = std::filesystem::canonical(directory);
    startup_write_api = make_write_host_api(kernel_files);
    startup_write_host = {sizeof(CtrHostApi), CTRPORT_SERVICE_ABI_VERSION, &startup_write_api, nullptr};
    unsigned installed = 0;
    for (std::size_t i = 0; i < metadata.size(); ++i) {
        std::uint64_t extent = 0;
        for (const auto& span : metadata[i].spans) extent = (std::max)(extent, span.second);
        for (const auto& symbol : metadata[i].imports) {
            if (symbol.nid != "4wSze92BhLI" || symbol.library != "libkernel" || symbol.module != "libkernel") continue;
            ServiceImportRecord record{pin,           metadata[i].sha256, symbol.nid, symbol.library,
                                       symbol.module, symbol.slot,        2};
            const auto token = integer_dispatch_records.add(record, pin, metadata[i].sha256, extent);
            auto thunk = std::make_unique<ServiceIntegerThunk>(token);
            admit_service_slot(images[i], metadata[i], pin, record,
                               reinterpret_cast<const void*>(&ctr_sysv_console_write), thunk->target());
            startup_write_thunks.push_back(std::move(thunk));
            ++installed;
        }
    }
    if (!installed) throw std::runtime_error("Startup reload write import absent");
    // This bounded publisher is a bring-up control, not a general file watcher.
    // All context and thunk storage remains resident until process exit.
    if (startup_write_publisher.worker.joinable()) throw std::runtime_error("Startup write publisher already started");
    startup_write_publisher.worker = std::thread([root, pin] {
        try {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
            while (!integer_dispatch_generations.waiting_count()) {
                if (startup_write_publisher.stop.load()) return;
                if (std::chrono::steady_clock::now() > deadline) {
                    std::cerr << "startup write DLL: original call not observed within20s\n" << std::flush;
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            std::cout << "{\"scope\":\"original_startup_write_wait_observed\"}\n" << std::flush;
            load_service_generation(integer_dispatch_generations, root, root / L"service_write_1.dll",
                                    startup_write_host, pin, 1);
            std::cout << "{\"scope\":\"original_startup_write_dll_published\",\"serial\":1}\n" << std::flush;
        } catch (const std::exception& error) {
            std::cerr << "startup write DLL: " << error.what() << '\n' << std::flush;
            integer_dispatch_generations.shutdown();
        }
    });
    std::cout << "{\"scope\":\"startup_write_reload_admitted\",\"slots\":" << installed << "}\n" << std::flush;
}
}
