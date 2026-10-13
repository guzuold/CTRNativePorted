// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "../display_event_api.h"
// Borrowed kernel callbacks/context must outlive this subscription and all emitters.
// Owner stops/joins emitters before Detach. No private kernel queue implementation.
class VideoDisplayEvents {
    CtrDisplayEventApi api_{};
    uint64_t queue_ = 0;
public:
    VideoDisplayEvents() = default;
    VideoDisplayEvents(const VideoDisplayEvents&) = delete;
    VideoDisplayEvents& operator=(const VideoDisplayEvents&) = delete;
    int Attach(const CtrDisplayEventApi& api, uint64_t queue, uint64_t flipData, uint64_t vblankData) {
        if (queue_ || !queue || api.size != sizeof(api) || api.version != 1 || !api.context ||
            !api.add || !api.remove || !api.trigger || !api.clock) return 1;
        if (api.add(api.context, queue, 6, flipData)) return 2;
        if (api.add(api.context, queue, 7, vblankData)) {
            return api.remove(api.context, queue, 6) ? 4 : 3;
        }
        api_ = api; queue_ = queue; return 0;
    }
    int Emit(uint64_t ident, uint64_t argument = 0) const {
        if (!queue_ || (ident != 6 && ident != 7)) return 1;
        // Kernel owns packing/coalescing and samples its native TSC.
        const uint64_t hint = (argument << 16) | ident;
        return api_.trigger(api_.context, queue_, ident, hint) ? 2 : 0;
    }
    int Clock(uint64_t& micros, uint64_t& tsc) const {
        if (!queue_) return 1;
        uint64_t m = 0, t = 0;
        if (api_.clock(api_.context, &m, &t)) return 2;
        micros = m; tsc = t; return 0;
    }
    int Detach() {
        if (!queue_) return 1;
        const auto queue = queue_; queue_ = 0;
        const int flip = api_.remove(api_.context, queue, 6);
        const int vblank = api_.remove(api_.context, queue, 7);
        return flip || vblank ? 2 : 0;
    }
    // Explicit Detach required so callback errors are observed by the owner.
};

// One independently owned event registration. Multiple instances can target
// distinct queues/identifiers. Configure/remove only after emitters are joined.
class VideoDisplayEvent {
    CtrDisplayEventApi api_{};
    uint64_t queue_=0, ident_=0;
public:
    VideoDisplayEvent()=default;
    VideoDisplayEvent(const VideoDisplayEvent&)=delete;
    VideoDisplayEvent& operator=(const VideoDisplayEvent&)=delete;
    int Attach(const CtrDisplayEventApi& api, uint64_t queue, uint64_t ident, uint64_t userdata) {
        if(queue_ || !queue || (ident!=6 && ident!=7) || api.size!=sizeof(api) || api.version!=1 ||
           !api.context || !api.add || !api.remove || !api.trigger || !api.clock) return 1;
        if(api.add(api.context,queue,ident,userdata)) return 2;
        api_=api; queue_=queue; ident_=ident; return 0;
    }
    int Emit(uint64_t argument=0) const {
        if(!queue_) return 1;
        return api_.trigger(api_.context,queue_,ident_,(argument<<16)|ident_) ? 2 : 0;
    }
    int Detach() {
        if(!queue_) return 1;
        const auto queue=queue_; queue_=0;
        return api_.remove(api_.context,queue,ident_) ? 2 : 0;
    }
};

#include <atomic>
#include <array>
#include <memory>
#include <mutex>
// Bounded registration ownership only: kernel retains queue/event semantics.
// Kernel callbacks must not re-enter this collection. All calls serialize with Emit.
class VideoDisplayEventSet {
    struct Entry { uint64_t queue, ident; std::unique_ptr<VideoDisplayEvent> event; };
    std::mutex mutex_;
    std::mutex tickMutex_;
    CtrDisplayEventApi clockApi_{};
    uint64_t vblankCount_=0, micros_=0, tsc_=0;
    std::array<Entry,32> entries_{};
public:
    int Add(const CtrDisplayEventApi& api,uint64_t queue,uint64_t ident,uint64_t data) {
        std::lock_guard lock(mutex_);
        Entry* free=nullptr;
        for(auto& e:entries_) {
            if(e.event && e.queue==queue && e.ident==ident) return 1;
            if(!e.event && !free) free=&e;
        }
        if(!free) return 3;
        auto event=std::make_unique<VideoDisplayEvent>();
        const int error=event->Attach(api,queue,ident,data);
        if(error) return error;
        free->queue=queue; free->ident=ident; free->event=std::move(event); return 0;
    }
    int Remove(uint64_t queue,uint64_t ident) {
        std::lock_guard lock(mutex_);
        for(auto& e:entries_) if(e.event && e.queue==queue && e.ident==ident) {
            const int error=e.event->Detach(); e.event.reset(); return error;
        }
        return 1;
    }
    int Emit(uint64_t ident,uint64_t argument=0) {
        if(ident!=6 && ident!=7) return 1;
        std::lock_guard lock(mutex_);
        int error=0;
        for(auto& e:entries_) if(e.event && e.ident==ident) {
            if(e.event->Emit(argument)) error=2;
        }
        return error; // No subscribers is normal; no kernel operation fabricated.
    }
    int BindClock(const CtrDisplayEventApi& api) {
        std::lock_guard lock(tickMutex_);
        if(api.size!=sizeof(api) || api.version!=1 || !api.context || !api.clock) return 1;
        if(clockApi_.clock) return clockApi_.context==api.context && clockApi_.clock==api.clock ? 0 : 1;
        clockApi_=api; return 0;
    }
    int ReadClock(uint64_t& micros,uint64_t& tsc) {
        std::lock_guard lock(tickMutex_);
        if(!clockApi_.clock) return 1;
        uint64_t m=0,t=0;
        if(clockApi_.clock(clockApi_.context,&m,&t)) return 2;
        micros=m; tsc=t; return 0;
    }
    int VblankTick() {
        std::lock_guard lock(tickMutex_);
        if(vblankCount_==UINT64_MAX) return 3;
        uint64_t micros=0,tsc=0;
        if(clockApi_.clock && clockApi_.clock(clockApi_.context,&micros,&tsc)) return 2;
        const int error=Emit(7,vblankCount_);
        ++vblankCount_; micros_=micros; tsc_=tsc; return error;
    }
    bool VblankSnapshot(uint64_t& count,uint64_t& micros,uint64_t& tsc) {
        std::lock_guard lock(tickMutex_);
        if(!clockApi_.clock) return false;
        count=vblankCount_; micros=micros_; tsc=tsc_; return true;
    }
    uint64_t VblankCount() { std::lock_guard lock(tickMutex_); return vblankCount_; }
    int Clear() {
        std::lock_guard lock(mutex_);
        int error=0;
        for(auto& e:entries_) if(e.event) {
            if(e.event->Detach()) error=2;
            e.event.reset();
        }
        return error;
    }
    // Owner joins emitters then explicitly Clear and observes errors.
};
