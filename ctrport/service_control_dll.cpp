// SPDX-License-Identifier: GPL-2.0-only
// Synthetic arithmetic adapter only; never a PS4 service implementation.
#include "service_abi.h"
#include <cstring>
#ifndef CTR_CONTROL_GENERATION
#error CTR_CONTROL_GENERATION required
#endif
static uint64_t adapter(uint64_t a, uint64_t b, uint64_t c) {
    return a + b + c + CTR_CONTROL_GENERATION;
}
extern "C" __declspec(dllexport) const CtrServiceManifest* ctr_get_service_manifest(const CtrHostApi* host) {
    if (!host || host->size != sizeof(CtrHostApi) || host->version != 1) return nullptr;
    static CtrServiceEntry entry{};
    entry.size = sizeof(entry);
    entry.signature_id = 1;
    std::memcpy(entry.nid, "synthetic", 10);
    std::memcpy(entry.library, "test", 5);
    std::memcpy(entry.module, "test", 5);
    entry.win64_adapter = reinterpret_cast<const void*>(&adapter);
    static CtrServiceManifest manifest{};
    manifest.size = sizeof(manifest);
    manifest.version = 1;
    manifest.entry_size = sizeof(entry);
    manifest.entry_count = 1;
    manifest.entries = &entry;
    std::memset(manifest.eboot_sha256, 'a', 64);
    return &manifest;
}
