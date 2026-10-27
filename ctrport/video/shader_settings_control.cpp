// SPDX-License-Identifier: GPL-2.0-only
#include "shader_settings.h"
#include <atomic>
#include <string_view>
#include <thread>
#include <vector>
#include <stdexcept>
int main(int argc, char** argv) {
    if (argc != 3) return 1;
    const bool dump = std::string_view(argv[1]) == "1";
    const bool direct = std::string_view(argv[2]) == "1";
    try {
        (void)CtrVideo::GetShaderSettings();
        return 2;
    } catch (const std::logic_error&) {}
    CtrVideo::InitializeShaderSettings({dump, direct});
    const auto* identity = &CtrVideo::GetShaderSettings();
    try {
        CtrVideo::InitializeShaderSettings({!dump, !direct});
        return 3;
    } catch (const std::logic_error&) {}
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
        threads.emplace_back([&] {
            for (int j = 0; j < 1000; ++j) {
                const auto& settings = CtrVideo::GetShaderSettings();
                if (&settings != identity || settings.IsDumpShaders() != dump ||
                    settings.IsDirectMemoryAccessEnabled() != direct)
                    ++errors;
            }
        });
    for (auto& thread : threads) thread.join();
    return errors.load() == 0 ? 0 : 4;
}
