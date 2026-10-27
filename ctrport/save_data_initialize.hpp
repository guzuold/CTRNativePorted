// SPDX-License-Identifier: GPL-2.0-only
// Own lifecycle implementation. Public contract: shadPS4 savedata.cpp318-325,
// 1206-1210 and save_backup.cpp177-205 at e3ce810f3a653f43ac64ebab63023de281a4103a
// (GPL-2.0-or-later). Mount/backup requests and all other SaveData APIs remain missing.
#pragma once
#include <filesystem>
#include <thread>
#include <condition_variable>
#include <fstream>
namespace ctrport {
class SaveDataInitialization {
    mutable std::mutex lock;
    std::condition_variable changed;
    std::thread maintenance;
    bool initialized = false, waiting = false, stop = false;
    std::filesystem::path root;
    std::string title;
    std::uint32_t firmware = 0;

public:
    ~SaveDataInitialization() {
        {
            std::lock_guard<std::mutex> guard(lock);
            stop = true;
            changed.notify_all();
        }
        if (maintenance.joinable()) maintenance.join();
    }
    int initialize(const void* parameters, const std::string& id, std::uint32_t version,
                   const std::filesystem::path& directory) {
        if (parameters) throw std::runtime_error("SaveDataInitialize3 nonnull parameters unverified");
        if (id != "CUSA14876" || version != 0x06008000) throw std::runtime_error("SaveData retail context unverified");
        std::unique_lock<std::mutex> guard(lock);
        if (initialized) return 0;
        const auto allowed = std::filesystem::weakly_canonical("runtime/user");
        const auto actual = std::filesystem::weakly_canonical(directory);
        const auto relative = actual.lexically_relative(allowed);
        if (relative.empty() || relative.is_absolute()) throw std::runtime_error("SaveData root rejected");
        for (const auto& part : relative)
            if (part == "..") throw std::runtime_error("SaveData root outside runtime/user");
        std::filesystem::create_directories(actual);
        if (!std::filesystem::is_directory(actual)) throw std::runtime_error("SaveData root is not directory");
        root = actual;
        title = id;
        firmware = version;
        maintenance = std::thread([this] {
            std::unique_lock<std::mutex> worker(lock);
            waiting = true;
            changed.notify_all();
            changed.wait(worker, [this] { return stop; });
            waiting = false;
        });
        changed.wait(guard, [this] { return waiting; });
        initialized = true;
        return 0;
    }
    bool ready() const {
        std::lock_guard<std::mutex> guard(lock);
        return initialized && waiting && title == "CUSA14876" && firmware == 0x06008000 &&
               std::filesystem::is_directory(root);
    }
};
inline SaveDataInitialization save_data_initialization;
extern "C" int ctr_save_data_initialize3(const void* parameters, std::uint64_t caller) {
    try {
        const auto result =
            save_data_initialization.initialize(parameters, "CUSA14876", 0x06008000, "runtime/user/savedata/CUSA14876");
        std::cout << "{\"scope\":\"save_data_initialize3\",\"result\":" << result
                  << ",\"owned_worker_waiting\":" << (save_data_initialization.ready() ? "true" : "false") << "}\n"
                  << std::flush;
        return result;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "NID=TywrFKCoLGY library=libSceSaveData caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), error.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_save_data_initialize3();
}
