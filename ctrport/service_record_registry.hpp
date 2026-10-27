// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "service_import_record.hpp"
#include <map>
namespace ctrport {
// Monotonic tokens avoid raw-pointer dereference and address-reuse ambiguity.
// Retiring blocks new acquisition; an acquired immutable record survives in flight.
class ServiceRecordRegistry {
    std::mutex lock;
    std::uint64_t next = 1;
    std::map<std::uint64_t, std::shared_ptr<const ServiceImportRecord>> records;

public:
    std::uint64_t add(ServiceImportRecord record, const std::string& version, const std::string& image,
                      std::uint64_t extent) {
        validate_service_import_record(record, version, image, extent);
        auto owned = std::make_shared<const ServiceImportRecord>(std::move(record));
        std::lock_guard<std::mutex> guard(lock);
        if (next == UINT64_MAX) throw std::runtime_error("Resident service token exhausted");
        const auto token = next++;
        records.emplace(token, std::move(owned));
        return token;
    }
    std::shared_ptr<const ServiceImportRecord> acquire(std::uint64_t token) {
        std::lock_guard<std::mutex> guard(lock);
        auto found = records.find(token);
        if (found == records.end()) throw std::runtime_error("Resident service token stale or unknown");
        return found->second;
    }
    void retire(std::uint64_t token) {
        std::shared_ptr<const ServiceImportRecord> old;
        {
            std::lock_guard<std::mutex> guard(lock);
            auto found = records.find(token);
            if (found == records.end()) throw std::runtime_error("Resident service retirement token invalid");
            old = std::move(found->second);
            records.erase(found);
        }
    }
};
}
