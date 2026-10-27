// SPDX-License-Identifier: GPL-2.0-only
// Independent lifecycle control; public contract audit: thread_exit_join_lifecycle_audit.json.
// No guest pthread binding or scheduler implementation.
#pragma once
#include <mutex>
#include <stdexcept>
#include <cstdint>
#include <future>
#include <atomic>
namespace ctrport {
class ThreadCompletion {
    std::mutex lock;
    bool published = false, completed = false, claimed = false, detached = false, collected = false;
    std::uint64_t result = 0, joiner = 0;

public:
    void publish() {
        std::lock_guard<std::mutex> guard(lock);
        if (published) throw std::runtime_error("Thread already published");
        published = true;
    }
    bool abort_creation() {
        std::lock_guard<std::mutex> guard(lock);
        if (!published || completed || claimed || detached || collected) return false;
        collected = true;
        return true;
    }
    bool claim_join(std::uint64_t owner, bool self = false) {
        std::lock_guard<std::mutex> guard(lock);
        if (!owner || !published || self || claimed || detached || collected) return false;
        claimed = true;
        joiner = owner;
        return true;
    }
    void complete(std::uint64_t value) {
        std::lock_guard<std::mutex> guard(lock);
        if (!published || completed || collected) throw std::runtime_error("Invalid thread completion");
        result = value;
        completed = true;
        if (detached) collected = true;
    }
    bool consume_join(std::uint64_t owner, std::uint64_t& value) {
        std::lock_guard<std::mutex> guard(lock);
        if (!owner || owner != joiner || !claimed || !completed || collected || detached) return false;
        value = result;
        collected = true;
        return true;
    }
    bool backout_join(std::uint64_t owner) {
        std::lock_guard<std::mutex> guard(lock);
        if (!owner || owner != joiner || !claimed || collected) return false;
        claimed = false;
        joiner = 0;
        return true;
    }
    bool detach() {
        std::lock_guard<std::mutex> guard(lock);
        if (!published || claimed || detached || collected) return false;
        detached = true;
        if (completed) collected = true;
        return true;
    }
    bool has_joiner() {
        std::lock_guard<std::mutex> guard(lock);
        return claimed;
    }
    bool is_collected() {
        std::lock_guard<std::mutex> guard(lock);
        return collected;
    }
};
}
