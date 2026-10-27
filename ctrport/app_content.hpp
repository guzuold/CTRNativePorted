// SPDX-License-Identifier: GPL-2.0-only
// Own bounded PSF reader and no-addons AppContent lifecycle. Public contracts:
// shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a psf.h/cpp and
// app_content.cpp:132..169,284..356 (GPL-2.0-or-later). No SKU entitlement fabricated.
#pragma once
#include "kernel_file.hpp"
#include "flat_loader.hpp"
namespace ctrport {
struct AppMetadata {
    std::string title;
    std::map<unsigned, std::int32_t> parameters;
};
inline AppMetadata parse_app_metadata(const std::vector<unsigned char>& bytes) {
    if (little(bytes, 0, 4) != 0x46535000 || (little(bytes, 4, 4) != 0x101 && little(bytes, 4, 4) != 0x100))
        throw std::runtime_error("AppContent PSF header invalid");
    const auto keys = static_cast<std::size_t>(little(bytes, 8, 4)),
               data = static_cast<std::size_t>(little(bytes, 12, 4)),
               count = static_cast<std::size_t>(little(bytes, 16, 4));
    if (count > 4096 || keys < 20 + count * 16 || keys > data || data > bytes.size())
        throw std::runtime_error("AppContent PSF tables invalid");
    AppMetadata result;
    std::map<std::string, bool> seen;
    for (std::size_t i = 0; i < count; ++i) {
        const auto e = 20 + i * 16;
        const auto key = keys + static_cast<std::size_t>(little(bytes, e, 2));
        if (key >= data) throw std::runtime_error("AppContent PSF key invalid");
        auto end = key;
        while (end < data && bytes[end]) ++end;
        if (end == data) throw std::runtime_error("AppContent PSF key unterminated");
        const std::string name(bytes.begin() + key, bytes.begin() + end);
        if (!seen.emplace(name, true).second) throw std::runtime_error("AppContent duplicate PSF key");
        const auto fmt = little(bytes, e + 2, 2), length = little(bytes, e + 4, 4), capacity = little(bytes, e + 8, 4),
                   offset = little(bytes, e + 12, 4);
        if (length > capacity || offset > bytes.size() - data || capacity > bytes.size() - data - offset)
            throw std::runtime_error("AppContent PSF value extent invalid");
        const auto begin = data + static_cast<std::size_t>(offset);
        if (name == "TITLE_ID") {
            if (fmt != 0x0204 || !length || bytes[begin + static_cast<std::size_t>(length) - 1])
                throw std::runtime_error("AppContent TITLE_ID invalid");
            result.title =
                std::string(reinterpret_cast<const char*>(bytes.data() + begin), static_cast<std::size_t>(length) - 1);
        }
        for (unsigned n = 1; n <= 4; ++n)
            if (name == "USER_DEFINED_PARAM_" + std::to_string(n)) {
                if (fmt != 0x0404 || length != 4) throw std::runtime_error("AppContent integer parameter invalid");
                result.parameters[n] = static_cast<std::int32_t>(little(bytes, begin, 4));
            }
    }
    if (result.title.size() != 9 || result.title.rfind("CUSA", 0) != 0 ||
        result.title.find_first_not_of("0123456789", 4) != std::string::npos)
        throw std::runtime_error("AppContent TITLE_ID domain unsupported");
    return result;
}
class AppContent {
    std::mutex mutex;
    bool loaded = false, initialized = false;
    AppMetadata metadata;

public:
    int load(const std::filesystem::path& path) {
        std::lock_guard<std::mutex> lock(mutex);
        if (loaded) return 0;
        const auto size = std::filesystem::file_size(path);
        if (size > 1024 * 1024) throw std::runtime_error("AppContent PSF too large");
        std::ifstream in(path, std::ios::binary);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
        if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
            throw std::runtime_error("AppContent PSF read failed");
        auto parsed = parse_app_metadata(bytes);
        // No addon mount exists in this runtime. Refuse a configured nonempty addon catalog.
        const auto addons = std::filesystem::path("runtime/user/addons") / parsed.title;
        if (std::filesystem::exists(addons) && !std::filesystem::is_empty(addons))
            throw std::runtime_error("AppContent addon catalog implementation missing");
        metadata = std::move(parsed);
        loaded = true;
        std::cout << "{\"scope\":\"app_content_metadata_loaded\",\"psf_bytes\":" << size
                  << ",\"addon_mounts\":0,\"sku_available\":false}\n"
                  << std::flush;
        return 0;
    }
    int initialize() {
        std::lock_guard<std::mutex> lock(mutex);
        if (!loaded) throw std::runtime_error("AppContent module not loaded");
        initialized = true;
        return 0;
    }
    // Public app_content.cpp229-255: enumerate the actual empty configured addon catalog.
    int list_addons(void* list, unsigned capacity, unsigned* hits) {
        if ((!capacity || !list) && !hits) return int(0x80d90002U);
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) throw std::runtime_error("AppContent not initialized");
        // load rejects a nonempty configured catalog; no entries means list is untouched.
        if (hits) {
            require_time_output(hits, 4);
            *hits = 0;
        }
        return 0;
    }
    int get_int(unsigned id, std::int32_t* output) {
        if (!output) return static_cast<int>(0x80d90002U);
        if (id > 4) return static_cast<int>(0x80d90002U);
        std::lock_guard<std::mutex> lock(mutex);
        if (!initialized) throw std::runtime_error("AppContent not initialized");
        if (id == 0)
            throw std::runtime_error(
                "SKU entitlement query requires documented retail entitlement; no forced FULL value");
        require_time_output(output, 4);
        const auto it = metadata.parameters.find(id);
        *output = it == metadata.parameters.end() ? 0 : it->second;
        return 0;
    }
};
inline AppContent app_content;
extern "C" int ctr_app_content_initialize(const void*, void*, std::uint64_t caller) {
    try {
        return app_content.initialize();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=R9lA82OraNs library=libSceAppContent caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" int ctr_app_content_get_int(unsigned id, std::int32_t* output, std::uint64_t caller) {
    std::cout << "{\"scope\":\"app_content_get_int\",\"id\":" << id << "}\n" << std::flush;
    try {
        return app_content.get_int(id, output);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=99b82IKXpH4 library=libSceAppContent caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" int ctr_app_content_list(unsigned label, void* list, unsigned capacity, unsigned* hits,
                                    std::uint64_t caller) {
    try {
        const int result = app_content.list_addons(list, capacity, hits);
        std::printf(
            "{\"scope\":\"app_content_addon_list\",\"nid\":\"xnd8BJzAxmk\",\"library\":\"libSceAppContent\",\"caller\":%llu,\"service_label\":%u,\"capacity\":%u,\"result\":%d,\"addon_mounts\":0}\n",
            static_cast<unsigned long long>(caller), label, capacity, result);
        std::fflush(stdout);
        return result;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=xnd8BJzAxmk library=libSceAppContent caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        std::fflush(stderr);
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_app_content_list();
extern "C" void ctr_sysv_app_list_fixture();
extern "C" void ctr_sysv_app_content_initialize();
extern "C" void ctr_sysv_app_content_get_int();
}
