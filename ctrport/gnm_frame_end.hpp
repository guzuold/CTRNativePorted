// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <cstdint>
#include <mutex>
#include <iostream>
namespace ctrport {
// Own transaction design; public contract: shadPS4 e3ce810f3a653f43ac64ebab63023de281a4103a
// gnmdriver.cpp2313, liverpool.h73 / liverpool.cpp139 (GPL-2.0-or-later).
// A receipt is NOT GPU idle. Only a genuine owner-thread provider may finish it.
class GnmFrameEnd {
    std::mutex mutex_;
    uint64_t count_ = 0, token_ = 0, thread_ = 0, frames_ = 0, argument_ = 0;
    bool pending_ = false, running_ = false;

public:
    int publish(uint64_t count, uint64_t token, uint64_t thread, bool commands, bool resources, bool present,
                uint64_t argument = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!count || !token || !thread || !commands || !resources || !present) return 60;
        if (pending_ || running_) return 91;
        if (count <= count_) return 88;
        count_ = count;
        token_ = token;
        thread_ = thread;
        argument_ = argument;
        pending_ = true;
        return 0;
    }
    template <class Provider> int finish(uint64_t thread, Provider&& provider) {
        uint64_t count = 0, token = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!pending_) return 62;
            if (running_) return 91;
            if (thread != thread_) return 116;
            running_ = true;
            count = count_;
            token = token_;
        }
        int status = 112;
        try {
            status = provider(count, token);
        } catch (...) {
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
            throw;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        if (status) return status;
        pending_ = false;
        ++frames_;
        return 0;
    }
    struct Snapshot {
        uint64_t count, token, thread, frames, argument;
        bool pending, running;
    };
    Snapshot snapshot() {
        std::lock_guard<std::mutex> lock(mutex_);
        return {count_, token_, thread_, frames_, argument_, pending_, running_};
    }
    uint64_t frames() {
        std::lock_guard<std::mutex> lock(mutex_);
        return frames_;
    }
};
inline GnmFrameEnd original_gnm_frame_end;
}
