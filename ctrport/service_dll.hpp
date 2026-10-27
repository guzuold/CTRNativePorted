// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_module_memory.hpp"
#include <filesystem>
namespace ctrport {
inline void load_service_generation(ServiceGenerations& generations, const std::filesystem::path& root,
                                    const std::filesystem::path& path, const CtrHostApi& host, const std::string& pin,
                                    std::uint64_t serial) {
    const auto directory = std::filesystem::canonical(root);
    const auto file = std::filesystem::canonical(path);
    if (file.parent_path() != directory || file.extension() != L".dll")
        throw std::runtime_error("Service DLL outside build directory");
    if (host.size != sizeof(host) || host.version != CTRPORT_SERVICE_ABI_VERSION)
        throw std::runtime_error("Service host ABI differs");
    HMODULE module =
        LoadLibraryExW(file.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module) throw std::runtime_error("Service DLL load failed");
    auto owner = std::shared_ptr<void>(module, [](void* value) { FreeLibrary(static_cast<HMODULE>(value)); });
    const auto symbol = GetProcAddress(module, "ctr_get_service_manifest");
    if (!symbol) throw std::runtime_error("Service DLL manifest export missing");
    CtrGetServiceManifest get{};
    static_assert(sizeof(get) == sizeof(symbol));
    std::memcpy(&get, &symbol, sizeof(get));
    require_service_module_range(module, reinterpret_cast<const void*>(get), 1, true);
    auto entries = snapshot_service_manifest(module, get(&host), pin);
    generations.publish(std::move(owner), std::move(entries), serial, pin);
}
}
