// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "video/graphics_companion.h"
#include <vector>
#include <cstdio>
#include <stdexcept>
#include "video_session_owner.hpp"
namespace ctrport {
// Owner-thread synchronous retention; destroy descriptor layout owners first.
class VideoSamplerBatch {
    void* context_;
    std::vector<uint64_t> tokens_;
    bool closed_ = false;

public:
    explicit VideoSamplerBatch(void* context) : context_(context) { tokens_.reserve(256); }
    VideoSamplerBatch(const VideoSamplerBatch&) = delete;
    VideoSamplerBatch& operator=(const VideoSamplerBatch&) = delete;
    int create(const CtrGraphicsSamplerParametersV1& parameters, uint64_t* output) {
        if (closed_ || !output) return 60;
        if (tokens_.size() == 256) return 88;
        uint64_t token = 0;
        const int status = ctr_video_sampler_create_v1(context_, &parameters, &token);
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
            const int status = ctr_video_sampler_release_v1(context_, *it);
            if (!status)
                ++count;
            else if (!failure)
                failure = status;
        }
        closed_ = true;
        if (released) *released = count;
        return failure;
    }
    ~VideoSamplerBatch() {
        const int status = close();
        if (status) {
            std::fprintf(stderr, "ERROR selected-session sampler batch cleanup status=%d\n", status);
            std::fflush(stderr);
        }
    }
};

}
