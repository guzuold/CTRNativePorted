// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "vblank_clock.h"
#include <memory>
namespace CtrVideo {
// Host-owned port token; deliberately not a guest handle or bound PS4 service.
class VideoOutputPort {
    std::mutex mutex_;
    std::shared_ptr<VblankClock> clock_;
    std::uint64_t generation_=0;
public:
    std::uint64_t Open(int (*callback)(void*) = nullptr, void* context = nullptr){
        std::lock_guard lock(mutex_);
        if(clock_ || generation_==UINT64_MAX)return 0;
        clock_=std::make_shared<VblankClock>(callback, context);
        return ++generation_;
    }
    bool Close(std::uint64_t token){
        std::shared_ptr<VblankClock> closing;
        {std::lock_guard lock(mutex_);
         if(!clock_ || !token || token!=generation_)return false;
         closing=std::move(clock_);}
        closing->Stop();return true;
    }
    ~VideoOutputPort(){Close(generation_);}
    bool Count(std::uint64_t token,std::uint64_t& output){
        std::lock_guard lock(mutex_);
        if(!clock_ || !token || token!=generation_)return false;
        output=clock_->Count();return true;
    }
    bool CallbackError(std::uint64_t token, int& output){
        std::lock_guard lock(mutex_);
        if(!clock_ || !token || token!=generation_)return false;
        output=clock_->CallbackError();return true;
    }
    bool WaitAfter(std::uint64_t token,std::uint64_t previous,
                   std::chrono::milliseconds timeout){
        std::shared_ptr<VblankClock> live;
        {std::lock_guard lock(mutex_);
         if(!clock_ || !token || token!=generation_)return false;
         live=clock_;}
        return live->WaitAfter(previous,timeout);
    }
};
}
