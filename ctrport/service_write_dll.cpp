// SPDX-License-Identifier: GPL-2.0-only
// Reloadable adapter; actual descriptor state and checked write remain host-owned.
#include "service_abi.h"
#include "service_write_api.h"
#include <cstring>
static CtrWriteHostApi api{};
static uint32_t adapter(uint64_t fd, uint64_t bytes, uint64_t count, uint64_t* result) {
    if (!result || !api.write || fd > 0x7fffffff) return 1;
    int64_t value = 0;
    const auto status =
        api.write(api.context, static_cast<int32_t>(fd), reinterpret_cast<const void*>(bytes), count, &value);
    if (status) return status;
    std::memcpy(result, &value, sizeof(value));
    return 0;
}
extern "C" __declspec(dllexport) const CtrServiceManifest* ctr_get_service_manifest(const CtrHostApi* host) {
    if (!host || host->size != sizeof(CtrHostApi) || host->version != 1 || !host->context) return nullptr;
    const auto* write = static_cast<const CtrWriteHostApi*>(host->context);
    if (write->size != sizeof(CtrWriteHostApi) || write->version != 1 || !write->context || !write->write)
        return nullptr;
    static bool initialized = false;
    static CtrServiceEntry entry{};
    static CtrServiceManifest manifest{};
    if (initialized) {
        if (api.context != write->context || api.write != write->write) return nullptr;
        return &manifest;
    }
    api = *write;
    entry.size = sizeof(entry);
    entry.signature_id = 2;
    std::memcpy(entry.nid, "4wSze92BhLI", 12);
    std::memcpy(entry.library, "libkernel", 10);
    std::memcpy(entry.module, "libkernel", 10);
    entry.win64_adapter = reinterpret_cast<const void*>(&adapter);
    manifest.size = sizeof(manifest);
    manifest.version = 1;
    manifest.entry_size = sizeof(entry);
    manifest.entry_count = 1;
    manifest.entries = &entry;
    std::memcpy(manifest.eboot_sha256, "ca9c94411375ebec52de70fbc62f605ac8e2cea95595c627029969b5b9d9b5b1", 65);
    initialized = true;
    return &manifest;
}
