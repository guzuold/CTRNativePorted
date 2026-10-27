// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "compute_guest_writeback.hpp"
namespace ctrport {
// An exact guest publication authorizes a later sampled snapshot only while no
// subsequent color writer overlaps it. Keep full bytes in memory, never on disk.
class PublishedColorGenerations {
    struct Entry {
        uint64_t address;
        std::vector<unsigned char> bytes;
        size_t epoch;
    };
    std::vector<Entry> entries_;
    static bool overlap(uint64_t a, uint64_t n, uint64_t b, uint64_t m) {
        return n && m && a <= UINT64_MAX - n && b <= UINT64_MAX - m && a < b + m && b < a + n;
    }

public:
    int publish(const ComputeGuestWriteback& writer, size_t epoch) {
        if (!writer.completed || writer.poisoned) return 91;
        if (!writer.address || writer.initial.empty() || writer.initial.size() > 32 * 1024 * 1024 ||
            writer.address > UINT64_MAX - writer.initial.size())
            return 60;
        entries_.push_back({writer.address, writer.initial, epoch});
        return 0;
    }
    // Reuse already verified pre-writer publication bytes, never capture new
    // CPU contents after a GPU writer. Commit still rechecks actual guest bytes.
    int baseline(uint64_t address, uint64_t bytes, const std::vector<std::pair<uint64_t, uint64_t>>& writers,
                 size_t before, std::vector<unsigned char>& output) const {
        if (before > writers.size()) return 60;
        for (auto entry = entries_.rbegin(); entry != entries_.rend(); ++entry)
            if (entry->address == address && entry->bytes.size() == bytes) {
                if (entry->epoch > before) return 88;
                for (size_t k = entry->epoch; k < before; ++k)
                    if (overlap(address, bytes, writers[k].first, writers[k].second)) return 88;
                output = entry->bytes;
                return 0;
            }
        return 62;
    }
    bool matches(uint64_t address, uint64_t bytes, const std::vector<unsigned char>& snapshot,
                 const std::vector<std::pair<uint64_t, uint64_t>>& writers) const {
        if (!bytes || address > UINT64_MAX - bytes || snapshot.size() != bytes) return false;
        for (auto entry = entries_.rbegin(); entry != entries_.rend(); ++entry) {
            if (entry->address != address || entry->bytes.size() != bytes) continue;
            if (entry->epoch > writers.size() || entry->bytes != snapshot) return false;
            for (size_t k = entry->epoch; k < writers.size(); ++k)
                if (overlap(address, bytes, writers[k].first, writers[k].second)) return false;
            return true;
        }
        return false;
    }
};
}
