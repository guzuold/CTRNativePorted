// SPDX-License-Identifier: GPL-2.0-only
// Isolated deferred-cancellation state control. No guest service or asynchronous cancellation.
#pragma once
#include <mutex>
#include <future>
namespace ctrport {
class DeferredCancellationControl {
    std::mutex lock;bool enabled=true,pending=false,delivered=false,exited=false;
public:
    bool request(){std::lock_guard<std::mutex> g(lock);if(exited || delivered)return false;pending=true;return true;}
    bool set_enabled(bool value){std::lock_guard<std::mutex> g(lock);const bool old=enabled;enabled=value;return old;}
    bool cancellation_point(){std::lock_guard<std::mutex> g(lock);if(!enabled || !pending || delivered || exited)return false;pending=false;delivered=true;return true;}
    void finish(){std::lock_guard<std::mutex> g(lock);exited=true;pending=false;}
};
inline int deferred_cancellation_control() {
    DeferredCancellationControl state;std::promise<void> disabled,requested;auto ready=disabled.get_future();auto proceed=requested.get_future();
    auto worker=std::async(std::launch::async,[&]{
        if(!state.set_enabled(false))return false;disabled.set_value();proceed.get();
        if(state.cancellation_point() || state.set_enabled(true))return false;
        if(!state.cancellation_point() || state.cancellation_point())return false;
        state.finish();return true;
    });ready.get();const bool requested_once=state.request();requested.set_value();
    if(!worker.get() || !requested_once || state.request() || state.cancellation_point())return 1;
    std::cout<<"{\"scope\":\"isolated_deferred_cancellation\",\"actual_worker\":true,\"disabled_request_retained\":true,\"delivered_once_at_point\":true,\"post_exit_rejected\":true,\"guest_bound\":false}\n";return 0;
}
}
