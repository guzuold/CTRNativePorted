// SPDX-License-Identifier: GPL-2.0-only
// Reloadable mutex-init transport; mutex ownership remains resident.
#include "service_abi.h"
#include "service_integer_host_api.h"
#include <cstring>
#include <atomic>
static CtrIntegerHostApi api{};
static CtrHostApi logging{};
static uint32_t adapter(uint64_t a, uint64_t b, uint64_t c, uint64_t* result) {
    if (!result || !api.call) return 1;
    const auto status = api.call(a, b, c, result);
    static std::atomic<bool> observed{false};
    if (!status && !observed.exchange(true)) {
#if CTR_CONTROL_GENERATION == 1
        const char line[] = "{\"scope\":\"original_mutex_dll_adapter_return\",\"generation\":1}\n";
#else
        const char line[] = "{\"scope\":\"original_mutex_dll_adapter_return\",\"generation\":2}\n";
#endif
        if (logging.log) logging.log(logging.context, line, sizeof(line) - 1);
    }
    return status;
}
extern "C" __declspec(dllexport) const CtrServiceManifest* ctr_get_service_manifest(const CtrHostApi* host) {
    if (!host || host->size != sizeof(CtrHostApi) || host->version != 1 || !host->context) return nullptr;
    const auto* write = static_cast<const CtrIntegerHostApi*>(host->context);
    if (write->size != sizeof(CtrIntegerHostApi) || write->version != 1 || !write->call) return nullptr;
    static bool initialized = false;
    static CtrServiceEntry entry{};
    static CtrServiceManifest manifest{};
    if (initialized) {
        if (api.call != write->call) return nullptr;
        return &manifest;
    }
    api = *write;
    logging = *host;
    entry.size = sizeof(entry);
    entry.signature_id = 2;
    std::memcpy(entry.nid, "cmo1RIYva9o", 12);
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
