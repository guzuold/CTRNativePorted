// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "flat_loader.hpp"
namespace ctrport {
struct OriginalCallDeclaration {
    std::string eboot_sha256, image_sha256;
    std::uint64_t address = 0;
};
// Integer/pointer arguments only; caller must establish the original input domain.
extern "C" std::uint64_t ctr_call_original_integer2(void*, std::uint64_t, std::uint64_t);
extern "C" std::uint64_t ctr_call_original_integer3(void*, std::uint64_t, std::uint64_t, std::uint64_t);
extern "C" void ctr_sysv_integer3_fixture();
inline void* resolve_original_call(const std::vector<MappedImage>& images, const std::vector<ImageMetadata>& metadata,
                                   const std::string& eboot_sha, const std::string& image_sha, std::uint64_t offset) {
    if (images.size() != metadata.size() || metadata.empty() || metadata.front().sha256 != eboot_sha)
        throw std::runtime_error("Original call eboot SHA differs");
    for (std::size_t i = 0; i < metadata.size(); ++i)
        if (metadata[i].sha256 == image_sha) {
            bool code = false;
            for (const auto& span : metadata[i].executable_spans)
                if (offset >= span.first && offset < span.second) code = true;
            if (!code) throw std::runtime_error("Original call outside executable span");
            auto* target = images[i].get() + offset;
            MEMORY_BASIC_INFORMATION info{};
            if (!VirtualQuery(target, &info, sizeof(info)) || info.State != MEM_COMMIT ||
                (info.Protect != PAGE_EXECUTE_READ && info.Protect != PAGE_EXECUTE_READWRITE))
                throw std::runtime_error("Original call execution permission missing");
            return target;
        }
    throw std::runtime_error("Original call image SHA missing");
}
// Resolve an absolute callback only through owned image metadata and RX publication.
inline std::size_t resolve_owned_callback(const std::vector<MappedImage>& images,
                                          const std::vector<ImageMetadata>& metadata, const std::string& eboot_sha,
                                          void* target) {
    if (images.size() != metadata.size() || metadata.empty() || metadata.front().sha256 != eboot_sha)
        throw std::runtime_error("Callback image set identity differs");
    const auto address = reinterpret_cast<std::uint64_t>(target);
    std::size_t selected = images.size();
    for (std::size_t i = 0; i < images.size(); ++i) {
        const auto base = reinterpret_cast<std::uint64_t>(images[i].get());
        if (!base || address < base) continue;
        const auto offset = address - base;
        bool executable = false;
        for (const auto& span : metadata[i].executable_spans)
            if (offset >= span.first && offset < span.second) executable = true;
        if (!executable) continue;
        if (selected != images.size()) throw std::runtime_error("Callback executable ownership ambiguous");
        if (resolve_original_call(images, metadata, eboot_sha, metadata[i].sha256, offset) != target)
            throw std::runtime_error("Callback resolved target differs");
        selected = i;
    }
    if (selected == images.size()) throw std::runtime_error("Callback outside owned executable images");
    return selected;
}
inline std::uint64_t call_original_integer2(const std::vector<MappedImage>& images,
                                            const std::vector<ImageMetadata>& metadata,
                                            const OriginalCallDeclaration& declaration, std::uint64_t first = 0,
                                            std::uint64_t second = 0) {
    return ctr_call_original_integer2(resolve_original_call(images, metadata, declaration.eboot_sha256,
                                                            declaration.image_sha256, declaration.address),
                                      first, second);
}
}
