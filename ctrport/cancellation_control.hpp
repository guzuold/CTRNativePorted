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
}
