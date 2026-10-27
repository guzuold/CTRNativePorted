// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <vector>
namespace ctrport {
// Retain only a completed fenced publication. The caller must invalidate before
// every potentially writing submission, then republish after full completion.
// Bytes stay in memory; actual slot registration and guest-copy comparison are
// separate mandatory checks before presentation. This is not a flip completion.
class RetainedDisplayPublication {
    uint64_t session_ = 0, address_ = 0, generation_ = 0;
    std::vector<unsigned char> bytes_;

public:
    void invalidate() {
        session_ = address_ = generation_ = 0;
        bytes_.clear();
    }
    int publish(uint64_t session, uint64_t address, uint64_t generation, const std::vector<unsigned char>& bytes,
                bool completed, bool poisoned) {
        if (!completed || poisoned) return 91;
        if (!session || !address || !generation || bytes.empty() || bytes.size() > 32u * 1024u * 1024u ||
            address > UINT64_MAX - bytes.size())
            return 60;
        auto copy = bytes;
        bytes_ = std::move(copy);
        session_ = session;
        address_ = address;
        generation_ = generation;
        return 0;
    }
    int acquire(uint64_t session, uint64_t generation, uint64_t address, uint64_t size,
                const std::vector<unsigned char>& current, std::vector<unsigned char>& output) const {
        if (!session || !generation || !address || !size || address > UINT64_MAX - size) return 60;
        if (session != session_ || generation != generation_ || address != address_ || size != bytes_.size() ||
            current != bytes_)
            return 88;
        output = bytes_;
        return 0;
    }
};
}
