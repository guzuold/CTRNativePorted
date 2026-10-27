// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "flat_loader.hpp"
#include <tuple>
namespace ctrport {
inline void bind_original_images(std::vector<MappedImage>& images, const std::vector<ImageMetadata>& metadata) {
    if (images.size() != metadata.size()) throw std::runtime_error("Original binding image count differs");
    using Key = std::tuple<std::string, std::string, std::string, std::uint64_t>;
    struct Target {
        std::size_t image;
        std::uint64_t address;
    };
    std::map<Key, std::vector<Target>> exports;
    for (std::size_t i = 0; i < metadata.size(); ++i)
        for (const auto& symbol : metadata[i].exports)
            exports[{symbol.nid, symbol.library, symbol.module, symbol.type}].push_back({i, symbol.address});
    std::uint64_t bound = 0, missing = 0;
    bool first_record = true;
    std::cout << "{\"import_records\":[";
    for (std::size_t i = 0; i < metadata.size(); ++i)
        for (const auto& symbol : metadata[i].imports) {
            const auto found = exports.find({symbol.nid, symbol.library, symbol.module, symbol.type});
            if (!first_record) std::cout << ',';
            first_record = false;
            std::cout << "{\"source_sha256\":" << std::quoted(metadata[i].sha256) << ",\"slot\":" << symbol.slot
                      << ",\"nid\":" << std::quoted(symbol.nid) << ",\"library\":" << std::quoted(symbol.library)
                      << ",\"module\":" << std::quoted(symbol.module) << ",\"symbol_type\":" << symbol.type
                      << ",\"relocation_type\":" << symbol.relocation << ",\"addend_bits\":" << symbol.addend
                      << ",\"caller_address\":null,\"caller_status\":\"not_executed\"";
            if (symbol.type != 2 || found == exports.end() || found->second.size() != 1) {
                std::cout << ",\"status\":\"missing\",\"reason\":"
                          << std::quoted(symbol.type != 2         ? "nonfunction_original_binding_unsupported"
                                         : found == exports.end() ? "original_export_missing"
                                                                  : "original_export_ambiguous")
                          << '}';
                ++missing;
                continue;
            }
            const auto target = found->second.front();
            const auto offset = target.address + symbol.addend;
            bool code = false, slot = false;
            for (const auto& span : metadata[target.image].executable_spans)
                if (offset >= span.first && offset < span.second) code = true;
            for (const auto& span : metadata[i].spans)
                if (symbol.slot >= span.first && symbol.slot <= span.second && 8 <= span.second - symbol.slot)
                    slot = true;
            if (!code || !slot) throw std::runtime_error("Original NID binding outside mapped code/slot");
            const auto address = reinterpret_cast<std::uint64_t>(images[target.image].get()) + offset;
            auto* destination = images[i].get() + symbol.slot;
            std::memcpy(destination, &address, 8);
            std::uint64_t observed = 0;
            std::memcpy(&observed, destination, 8);
            if (observed != address) throw std::runtime_error("Original NID binding readback differs");
            std::cout << ",\"status\":\"original_bound_pointer_verified\",\"target_sha256\":"
                      << std::quoted(metadata[target.image].sha256) << ",\"target_offset\":" << offset << '}';
            ++bound;
        }
    std::cout << "]}\n";
    std::cout << "{\"original_bindings_applied\":" << bound << ",\"missing_imports\":" << missing
              << ",\"executable_permissions\":false,\"main_reached\":false,\"runtime_replacements\":0}\n";
}
}
