// SPDX-License-Identifier: GPL-2.0-or-later
// Context identity/validation adapted from shadPS4 ajm.cpp136 and
// ajm_context.cpp24, e3ce810f3a653f43ac64ebab63023de281a4103a.
// Runtime-owned worker/queue. No codec, guest batch or audio output is stubbed.
#pragma once
#include "kernel_time.hpp"
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <thread>
#include "../engine/shadPS4/externals/LibAtrac9/C/src/libatrac9.h"
namespace ctrport {
class AudioJobContext {
    std::mutex mutex;
    std::condition_variable wake;
    bool stopped = false;
    std::deque<std::function<void()>> jobs;
    std::thread worker;
    bool at9_registered = false;

public:
    AudioJobContext() {
        std::promise<void> ready;
        auto started = ready.get_future();
        worker = std::thread([this, signal = std::move(ready)]() mutable {
            signal.set_value();
            std::unique_lock<std::mutex> lock(mutex);
            for (;;) {
                wake.wait(lock, [this] { return stopped || !jobs.empty(); });
                if (stopped && jobs.empty()) break;
                auto job = std::move(jobs.front());
                jobs.pop_front();
                lock.unlock();
                job();
                lock.lock();
            }
        });
        started.get();
    }
    ~AudioJobContext() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stopped = true;
        }
        wake.notify_all();
        worker.join();
    }
    int register_codec(uint32_t codec) {
        std::lock_guard<std::mutex> lock(mutex);
        if (codec >= 23) return static_cast<int>(0x80930005U);
        if (codec != 1) throw std::runtime_error("AJM requested codec factory unavailable");
        if (at9_registered) return static_cast<int>(0x80930009U);
        void* handle = Atrac9GetHandle();
        if (!handle) return static_cast<int>(0x80930006U);
        Atrac9ReleaseHandle(handle);
        at9_registered = true;
        return 0;
    }
    std::future<void> enqueue_host_control(std::function<void()> action) {
        auto job = std::make_shared<std::packaged_task<void()>>(std::move(action));
        auto result = job->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (stopped) throw std::runtime_error("AJM worker stopped");
            jobs.push_back([job] { (*job)(); });
        }
        wake.notify_one();
        return result;
    }
};
class AudioJobContexts {
    std::mutex mutex;
    std::map<uint32_t, std::shared_ptr<AudioJobContext>> contexts;
    uint32_t next = 1;

public:
    int initialize(int64_t reserved, uint32_t* output) {
        if (reserved || !output) return static_cast<int>(0x80930005U);
        require_time_output(output, 4);
        auto context = std::make_shared<AudioJobContext>();
        std::lock_guard<std::mutex> lock(mutex);
        if (!next) throw std::runtime_error("AJM context identities exhausted");
        const uint32_t id = next;
        contexts.emplace(id, std::move(context));
        *output = id;
        ++next;
        return 0;
    }
    std::shared_ptr<AudioJobContext> get(uint32_t id) {
        std::lock_guard<std::mutex> lock(mutex);
        auto i = contexts.find(id);
        return i == contexts.end() ? nullptr : i->second;
    }
};
inline AudioJobContexts audio_job_contexts;
extern "C" int ctr_ajm_initialize(int64_t reserved, uint32_t* output, uint64_t caller) {
    try {
        const int rc = audio_job_contexts.initialize(reserved, output);
        std::printf(
            "{\"scope\":\"original_AjmInitialize_return\",\"caller\":%llu,\"result\":%d,\"worker_queue_owned\":%s,\"codec_implemented\":false}\n",
            static_cast<unsigned long long>(caller), rc, rc ? "false" : "true");
        std::fflush(stdout);
        return rc;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=dl+4eHSzUu4 library=libSceAjm caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" int ctr_ajm_register(uint32_t context, uint32_t codec, int64_t reserved, uint64_t caller) {
    try {
        int rc = static_cast<int>(0x80930005U);
        if (!reserved) {
            auto owner = audio_job_contexts.get(context);
            rc = owner ? owner->register_codec(codec) : static_cast<int>(0x80930002U);
        }
        std::printf(
            "{\"scope\":\"original_AjmModuleRegister_return\",\"caller\":%llu,\"context\":%u,\"codec\":%u,\"result\":%d,\"native_ATRAC9_factory\":true,\"guest_decode_bound\":false}\n",
            static_cast<unsigned long long>(caller), context, codec, rc);
        std::fflush(stdout);
        return rc;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "NID=Q3dyFuwGn64 library=libSceAjm caller=0x%llx unsupported: %s\n",
                     static_cast<unsigned long long>(caller), e.what());
        ExitProcess(4);
    }
}
extern "C" void ctr_sysv_ajm_register();
extern "C" void ctr_sysv_ajm_initialize();
}
