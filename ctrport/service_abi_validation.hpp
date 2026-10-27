// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_abi.h"
#include "service_generation.hpp"
#include <cstring>
namespace ctrport {
inline std::vector<ServiceGeneration::Entry> validate_service_manifest(const CtrServiceManifest& manifest,
                                                                       const std::string& pin) {
    if (manifest.size != sizeof(manifest) || manifest.version != CTRPORT_SERVICE_ABI_VERSION ||
        manifest.entry_size != sizeof(CtrServiceEntry) || !manifest.entry_count || manifest.entry_count > 4096 ||
        !manifest.entries)
        throw std::runtime_error("Service ABI schema unsupported");
    for (auto value : manifest.reserved)
        if (value) throw std::runtime_error("Service ABI reserved field set");
    if (pin.size() != 64 || manifest.eboot_sha256[64] || std::memcmp(manifest.eboot_sha256, pin.data(), 64))
        throw std::runtime_error("Service ABI retail identity differs");
    std::vector<ServiceGeneration::Entry> result;
    auto bounded = [](const char* data, std::size_t capacity) {
        const auto end = static_cast<const char*>(std::memchr(data, 0, capacity));
        if (!end || end == data) throw std::runtime_error("Service ABI string invalid");
        return std::string(data, end);
    };
    for (std::uint32_t i = 0; i < manifest.entry_count; ++i) {
        const auto& entry = manifest.entries[i];
        if (entry.size != sizeof(entry) || !entry.signature_id || !entry.win64_adapter)
            throw std::runtime_error("Service ABI adapter incomplete");
        // Integer3: signature1 direct result; signature2 explicit transport/output.
        if (entry.signature_id != 1 && entry.signature_id != 2 && entry.signature_id != 9)
            throw std::runtime_error("Service ABI signature unsupported");
        ServiceGeneration::Entry next{
            bounded(entry.nid, sizeof(entry.nid)), bounded(entry.library, sizeof(entry.library)),
            bounded(entry.module, sizeof(entry.module)), entry.win64_adapter, entry.signature_id};
        for (const auto& prior : result)
            if (prior.nid == next.nid && prior.library == next.library && prior.module == next.module)
                throw std::runtime_error("Service ABI duplicate identity");
        result.push_back(std::move(next));
    }
    return result;
}
}
