// SPDX-License-Identifier: GPL-2.0-only
// Authored synthetic transport control; never exports any PS4 service NID.
#include "service_abi.h"
#include <cstring>
#ifndef CTR_CONTROL_GENERATION
#error CTR_CONTROL_GENERATION required
#endif
static uint32_t adapter(const uint64_t* args, uint64_t caller, uint64_t* result) {
    const uint64_t expected[9] = {1, 0x8000000000000002ULL, 3, 0xffffffff00000004ULL, 5,
                                  6, 0x123456789abcdef0ULL, 8, 0xfedcba9876543210ULL};
    if (!args || !caller || !result || std::memcmp(args, expected, sizeof(expected))) return 60;
    *result = 0x80020008ULL + CTR_CONTROL_GENERATION;
    return 0;
}
extern "C" __declspec(dllexport) const CtrServiceManifest* ctr_get_service_manifest(const CtrHostApi* host) {
    if (!host || host->size != sizeof(*host) || host->version != 1) return nullptr;
    static CtrServiceEntry entry{};
    entry.size = sizeof(entry);
    entry.signature_id = 9;
    std::memcpy(entry.nid, "synthetic9", 10);
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
