// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/graphics_companion.h"
#include "video_session_owner.hpp"
#include <vector>
#include <cstdio>
#include <stdexcept>
namespace ctrport {
// Synchronous owner-thread scope: declare after buffer owners so layouts and
// their descriptor sets are destroyed first. Retention alone is not a fence.
class VideoLayoutBatch {
    void* context_;
    std::vector<uint64_t> tokens_;
    bool closed_ = false;

public:
    explicit VideoLayoutBatch(void* context) : context_(context) { tokens_.reserve(128); }
    VideoLayoutBatch(const VideoLayoutBatch&) = delete;
    VideoLayoutBatch& operator=(const VideoLayoutBatch&) = delete;
    int create(const CtrGraphicsBindingContractV1& ps, const CtrGraphicsBindingContractV1& vs, uint64_t* output) {
        if (closed_ || !output) return 60;
        if (tokens_.size() == 128) return 88;
        uint64_t token = 0;
        const int status = ctr_video_graphics_layout_create_v1(context_, &ps, &vs, &token);
        if (status) return status;
        if (!token) return 112;
        tokens_.push_back(token);
        *output = token;
        return 0;
    }
    int create_vertex(const CtrGraphicsBindingContractV1& vs, uint64_t* output) {
        if (closed_ || !output) return 60;
        if (tokens_.size() == 128) return 88;
        uint64_t token = 0;
        const int status = ctr_video_graphics_vertex_layout_create_v1(context_, &vs, &token);
        if (status) return status;
        if (!token) return 112;
        tokens_.push_back(token);
        *output = token;
        return 0;
    }
    size_t size() const { return tokens_.size(); }
    int close(unsigned* released = nullptr) noexcept {
        if (closed_) return 0;
        int failure = 0;
        unsigned count = 0;
        for (auto it = tokens_.rbegin(); it != tokens_.rend(); ++it) {
            const int status = ctr_video_graphics_layout_release_v1(context_, *it);
            if (!status)
                ++count;
            else if (!failure)
                failure = status;
        }
        closed_ = true;
        if (released) *released = count;
        return failure;
    }
    ~VideoLayoutBatch() {
        const int status = close();
        if (status) {
            std::fprintf(stderr, "ERROR selected-session layout batch cleanup status=%d\n", status);
            std::fflush(stderr);
        }
    }
};

}
