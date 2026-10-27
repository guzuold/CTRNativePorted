// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <future>
#include <functional>
#include <stdexcept>
#include <iostream>
namespace ctrport {
// Construct/pump on presentation owner thread. Consumers call without holding
// loader/kernel/session locks. Close cancels queued work; executing work finishes.
// Stop/join consumers and finish pump before destruction or callback DLL unload.
class OwnerDispatch {
    struct Request {
        std::function<int()> action;
        std::promise<int> result;
    };
    const std::thread::id owner = std::this_thread::get_id();
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::shared_ptr<Request>> queue;
    bool closed = false;

public:
    int invoke(std::function<int()> action) {
        if (!action) throw std::runtime_error("Owner dispatch action absent");
        if (std::this_thread::get_id() == owner) {
            {
                std::lock_guard<std::mutex> guard(mutex);
                if (closed) throw std::runtime_error("Owner dispatch closed");
            }
            return action();
        }
        auto request = std::make_shared<Request>();
        request->action = std::move(action);
        auto result = request->result.get_future();
        {
            std::lock_guard<std::mutex> guard(mutex);
            if (closed || queue.size() >= 256) throw std::runtime_error("Owner dispatch closed or full");
            queue.push_back(request);
        }
        changed.notify_one();
        return result.get(); // No queue lock held while waiting.
    }
    bool pump(unsigned timeout_ms) {
        if (std::this_thread::get_id() != owner || timeout_ms > 1000)
            throw std::runtime_error("Owner pump thread/timeout invalid");
        std::shared_ptr<Request> request;
        {
            std::unique_lock<std::mutex> guard(mutex);
            changed.wait_for(guard, std::chrono::milliseconds(timeout_ms), [&] { return closed || !queue.empty(); });
            if (queue.empty()) return false;
            request = queue.front();
            queue.pop_front();
        }
        try {
            request->result.set_value(request->action());
        } catch (...) {
            request->result.set_exception(std::current_exception());
        }
        return true;
    }
    std::size_t pending() {
        std::lock_guard<std::mutex> guard(mutex);
        return queue.size();
    }
    void close() {
        std::deque<std::shared_ptr<Request>> canceled;
        {
            std::lock_guard<std::mutex> guard(mutex);
            closed = true;
            canceled.swap(queue);
        }
        for (auto& request : canceled)
            request->result.set_exception(std::make_exception_ptr(std::runtime_error("Owner dispatch canceled")));
        changed.notify_all();
    }
    ~OwnerDispatch() { close(); }
};
}
