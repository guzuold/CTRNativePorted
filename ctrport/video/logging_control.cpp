// SPDX-License-Identifier: GPL-2.0-only
#include <unordered_map>
#include <string_view>
#include <thread>
#include <vector>
#include <spdlog/logger.h>
namespace Common::Log {
extern std::unordered_map<std::string_view, std::shared_ptr<spdlog::logger>> ALL_LOGGERS;
}
int main() {
    auto& map = Common::Log::ALL_LOGGERS;
    const auto count = map.size();
    for (const auto& pair : map)
        if (!pair.second || !pair.second->should_log(spdlog::level::trace)) return 2;
    auto logger = map.at("Render.Recompiler");
    logger->info("payload value={} utf8={}", 42, "\xce\xb1");
    std::vector<std::thread> workers;
    for (int worker = 0; worker < 4; ++worker)
        workers.emplace_back([&, worker] {
            for (int row = 0; row < 50; ++row) map.at("Render.Vulkan")->warn("record worker={} row={}", worker, row);
        });
    for (auto& worker : workers) worker.join();
    return map.size() == count ? 0 : 3;
}
