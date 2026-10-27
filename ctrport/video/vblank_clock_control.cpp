#include "video_display_events.h"
#include "../display_event_host.hpp"
// SPDX-License-Identifier: GPL-2.0-only
#include "vblank_clock.h"
#include "videoout_port.h"
#include "videoout_buffer_input.h"
#include <atomic>
#include <cstdio>
int main() {
    using namespace std::chrono;
    {
        struct Barrier {
            std::atomic<bool> entered{false}, release{false};
        } barrier;
        CtrVideo::VblankClock held(
            [](void* raw) {
                auto& b = *static_cast<Barrier*>(raw);
                b.entered = true;
                while (!b.release.load()) std::this_thread::yield();
                return 0;
            },
            &barrier);
        const auto limit = steady_clock::now() + seconds(1);
        while (!barrier.entered.load() && steady_clock::now() < limit) std::this_thread::yield();
        const bool entered = barrier.entered.load();
        const auto premature = held.Count();
        const bool earlyWait = held.WaitAfter(0, milliseconds(1));
        barrier.release = true;
        const bool completed = held.WaitAfter(0, milliseconds(500));
        held.Stop();
        if (!entered || premature || earlyWait || !completed) return 50;
        std::puts(
            "vblank publication passed callback_barrier=true no_early_count_or_wait=true completion_published=true");
    }
    CtrVideo::VblankClock clock;
    if (clock.WaitAfter(UINT64_MAX, milliseconds(2))) return 1;
    const auto start = steady_clock::now();
    const auto initial = clock.Count();
    std::atomic<int> woke{0};
    std::thread first([&] {
        if (clock.WaitAfter(initial, milliseconds(500))) ++woke;
    });
    std::thread second([&] {
        if (clock.WaitAfter(initial, milliseconds(500))) ++woke;
    });
    first.join();
    second.join();
    if (woke != 2) return 2;
    while (clock.Count() < initial + 12)
        if (!clock.WaitAfter(clock.Count(), milliseconds(500))) return 3;
    const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();
    if (elapsed < 150 || elapsed > 1000) return 4;
    std::atomic<bool> stopped{false};
    std::thread waiting([&] { stopped = !clock.WaitAfter(UINT64_MAX, milliseconds(500)); });
    clock.Stop();
    waiting.join();
    if (!stopped) return 5;
    const auto final = clock.Count();
    std::this_thread::sleep_for(milliseconds(25));
    if (clock.Count() != final || clock.WaitAfter(0, milliseconds(1))) return 6;
    clock.Stop();
    std::printf("vblank_ticks=%llu elapsed_ms=%lld waiters=2 stopped=1\n",
                static_cast<unsigned long long>(final - initial), static_cast<long long>(elapsed));
    CtrVideo::VideoOutputPort port;
    std::uint64_t value = 999;
    if (port.Count(1, value) || value != 999 || port.Close(1)) return 7;
    const auto token = port.Open();
    if (!token || port.Open() != 0 || !port.Count(token, value)) return 8;
    if (!port.WaitAfter(token, value, milliseconds(500))) return 9;
    std::atomic<bool> cancelled{false};
    std::thread waiter([&] { cancelled = !port.WaitAfter(token, UINT64_MAX, milliseconds(500)); });
    if (!port.Close(token)) {
        waiter.join();
        return 10;
    }
    waiter.join();
    if (!cancelled || port.Close(token)) return 11;
    const auto reopened = port.Open();
    value = 999;
    if (reopened <= token || port.Count(token, value) || value != 999 || port.Close(token)) return 12;
    if (!port.Count(reopened, value) || !port.Close(reopened)) return 13;
    std::printf("port_lifecycle=passed stale_tokens_rejected=1 close_cancels_wait=1\n");
    using CtrVideo::DisplayInputResult;
    CtrVideo::DisplayBufferAttributes a{0x80000000U, 1, 0, 4, 3, 8, 0, 0, 0};
    CtrVideo::PreparedDisplayRange range{11, 22};
    if (CtrVideo::PrepareDisplayRange(a, 4096, range) != DisplayInputResult::Prepared || range.size != 96) return 14;
    a.format = 0xC1060000U;
    if (CtrVideo::PrepareDisplayRange(a, 4096, range) != DisplayInputResult::Prepared || range.size != 192) return 15;
    const auto reject = [&](DisplayInputResult expected) {
        range = {11, 22};
        return CtrVideo::PrepareDisplayRange(a, 4096, range) == expected && range.address == 11 && range.size == 22;
    };
    a.tiling = 0;
    if (!reject(DisplayInputResult::MissingTiledLayout)) return 16;
    a.tiling = 1;
    a.format = 123;
    if (!reject(DisplayInputResult::MissingFormat)) return 17;
    a.format = 0x80000000U;
    a.pitch = 3;
    if (!reject(DisplayInputResult::Invalid)) return 18;
    a.pitch = 8;
    a.reserved1 = 1;
    if (!reject(DisplayInputResult::Invalid)) return 19;
    a.reserved1 = 0;
    a.height = 0;
    if (!reject(DisplayInputResult::Invalid)) return 20;
    a.height = UINT32_MAX;
    a.pitch = UINT32_MAX;
    a.format = 0xC1060000U;
    if (!reject(DisplayInputResult::Invalid)) return 21;
    a = {0x80000000U, 1, 0, 4, 3, 8, 0, 0, 0};
    range = {11, 22};
    if (CtrVideo::PrepareDisplayRange(a, UINT64_MAX - 2, range) != DisplayInputResult::Invalid || range.size != 22)
        return 22;
    std::printf("buffer_metadata=passed linear_only=1 tiled_missing=1 overflow_rejected=1\n");
    std::uint64_t raw_addresses[2] = {4096, 8192};
    CtrShaderSnapshot attr_snapshot{&a, sizeof(a), 100, 1};
    CtrShaderSnapshot address_snapshot{raw_addresses, sizeof(raw_addresses), 200, 2};
    CtrVideo::PreparedDisplayBatch batch{};
    if (CtrVideo::PrepareDisplayBatch(attr_snapshot, 100, address_snapshot, 200, 14, 2, batch) !=
            DisplayInputResult::Prepared ||
        batch.count != 2 || batch.first != 14 || batch.ranges[1].address != 8192)
        return 23;
    const auto previous = batch;
    const auto badbatch = [&](int first, int count) {
        return CtrVideo::PrepareDisplayBatch(attr_snapshot, 100, address_snapshot, 200, first, count, batch) ==
                   DisplayInputResult::Invalid &&
               batch.count == previous.count && batch.ranges[1].address == previous.ranges[1].address;
    };
    if (!badbatch(-1, 2) || !badbatch(15, 2) || !badbatch(0, 0) || !badbatch(0, 17)) return 24;
    address_snapshot.size = 8;
    if (!badbatch(0, 2)) return 25;
    address_snapshot.size = 16;
    raw_addresses[1] = UINT64_MAX - 2;
    if (!badbatch(0, 2)) return 26;
    raw_addresses[1] = 8192;
    attr_snapshot.snapshot_token = 0;
    if (!badbatch(0, 2)) return 27;
    std::printf("captured_buffer_batch=passed partial_and_invalid_rejected=1 atomic_output=1\n");
    ctrport::DisplayEventHost kernel;
    const auto kernelApi = ctrport::display_event_api(kernel);
    const auto queue = kernel.queues.create();
    const auto separateQueue = kernel.queues.create();
    VideoDisplayEvent flipOnly, vblankOnly;
    if (flipOnly.Attach(kernelApi, queue, 6, 111) || vblankOnly.Attach(kernelApi, separateQueue, 7, 222) ||
        !flipOnly.Attach(kernelApi, queue, 6, 333))
        return 43;
    if (flipOnly.Emit(321) || vblankOnly.Emit()) return 44;
    const auto flipEvent = kernel.queues.poll(queue, 2), blankEvent = kernel.queues.poll(separateQueue, 2);
    if (flipEvent.size() != 1 || flipEvent[0].ident != 6 || flipEvent[0].udata != 111 ||
        flipEvent[0].data >> 16 != 321 || blankEvent.size() != 1 || blankEvent[0].ident != 7 ||
        blankEvent[0].udata != 222)
        return 45;
    if (flipOnly.Detach() || !flipOnly.Emit() || vblankOnly.Emit()) return 46;
    const auto stillBlank = kernel.queues.poll(separateQueue, 2);
    if (stillBlank.size() != 1 || stillBlank[0].ident != 7 || vblankOnly.Detach()) return 47;
    kernel.queues.destroy(separateQueue);
    std::printf("independent event subscriptions passed separate_queues=true removing_flip_preserves_vblank=true\n");
    VideoDisplayEventSet eventSet;
    const auto setQueue = kernel.queues.create();
    if (eventSet.BindClock(kernelApi)) return 54;
    if (eventSet.Add(kernelApi, queue, 6, 111) || eventSet.Add(kernelApi, setQueue, 7, 222) ||
        !eventSet.Add(kernelApi, setQueue, 7, 333))
        return 48;
    {
        CtrVideo::VblankClock setClock([](void* c) { return static_cast<VideoDisplayEventSet*>(c)->VblankTick(); },
                                       &eventSet);
        const uint32_t waitUs = 500000;
        if (kernel.queues.wait(setQueue, 2, &waitUs).size() != 1) return 49;
        for (unsigned n = 0; n < 100; ++n) {
            if (eventSet.Remove(setQueue, 7) || eventSet.Add(kernelApi, setQueue, 7, 222)) return 50;
        }
        setClock.Stop();
        if (setClock.CallbackError()) return 51;
    }
    uint64_t tickCount = 0, tickMicros = 0, tickTsc = 0;
    if (!eventSet.VblankSnapshot(tickCount, tickMicros, tickTsc)) return 56;
    if (!tickCount || !tickMicros || !tickTsc) return 55;
    if (eventSet.Emit(6, 432)) return 52;
    const auto setFlip = kernel.queues.poll(queue, 2);
    if (!eventSet.VblankCount() || setFlip.size() != 1 || setFlip[0].data >> 16 != 432 || eventSet.Clear() ||
        !eventSet.Remove(queue, 6))
        return 53;
    kernel.queues.destroy(setQueue);
    std::printf("event set passed concurrent_registration_cycles=100 real_clock=true independent_flip=true\n");
    VideoDisplayEventSet failedClock;
    auto badClockApi = kernelApi;
    badClockApi.clock = [](void*, uint64_t* micros, uint64_t* tsc) -> int {
        *micros = 123;
        *tsc = 456;
        return 1;
    };
    if (failedClock.BindClock(badClockApi) || failedClock.VblankTick() != 2) return 57;
    uint64_t failedCount = 9, failedMicros = 8, failedTsc = 7;
    if (!failedClock.VblankSnapshot(failedCount, failedMicros, failedTsc) || failedCount || failedMicros || failedTsc)
        return 58;
    VideoDisplayEvents events;
    if (events.Attach(kernelApi, queue, 0x1234, 0x5678) || !events.Attach(kernelApi, queue, 0, 0)) return 28;
    if (events.Emit(6, 99) || events.Emit(7) || events.Emit(7)) return 29;
    const auto delivered = kernel.queues.poll(queue, 2);
    if (delivered.size() != 2 || delivered[0].ident != 6 || delivered[0].udata != 0x1234 ||
        delivered[0].data >> 16 != 99 || ((delivered[1].data >> 12) & 15) != 2 || delivered[1].udata != 0x5678)
        return 30;
    uint64_t micros = 0, tsc = 0;
    if (events.Clock(micros, tsc) || !tsc) return 31;
    if (events.Detach() || !events.Emit(6) || !events.Detach()) return 32;
    if (events.Attach(kernelApi, queue, 0, 0)) return 33;
    {
        CtrVideo::VblankClock emitting([](void* context) { return static_cast<VideoDisplayEvents*>(context)->Emit(7); },
                                       &events);
        const uint32_t timeoutUs = 500000;
        const auto ticks = kernel.queues.wait(queue, 2, &timeoutUs);
        if (ticks.size() != 1 || ticks[0].ident != 7) return 35;
        emitting.Stop();
        if (emitting.CallbackError()) return 36;
    }
    kernel.queues.poll(queue, 2);
    CtrVideo::VideoOutputPort eventPort;
    const auto eventToken =
        eventPort.Open([](void* c) { return static_cast<VideoDisplayEvents*>(c)->Emit(7); }, &events);
    const uint32_t eventTimeout = 500000;
    const auto portEvents = kernel.queues.wait(queue, 2, &eventTimeout);
    int eventError = 99;
    if (!eventToken || portEvents.size() != 1 || portEvents[0].ident != 7 ||
        !eventPort.CallbackError(eventToken, eventError) || eventError)
        return 38;
    if (!eventPort.Close(eventToken) || events.Detach()) return 39;
    if (eventPort.CallbackError(eventToken, eventError)) return 40;
    if (events.Attach(kernelApi, queue, 0, 0)) return 41;
    kernel.queues.destroy(queue);
    const auto failedToken =
        eventPort.Open([](void* c) { return static_cast<VideoDisplayEvents*>(c)->Emit(7); }, &events);
    eventPort.WaitAfter(failedToken, UINT64_MAX, milliseconds(500));
    if (!eventPort.CallbackError(failedToken, eventError) || eventError != 2 || !eventPort.Close(failedToken))
        return 42;
    std::printf("port owned event worker passed close_joins_before_detach=true destroyed_queue_error=2\n");
    if (!events.Emit(7) || events.Detach() != 2) return 34;
    {
        CtrVideo::VblankClock failing([](void*) { return 77; }, nullptr);
        failing.WaitAfter(UINT64_MAX, milliseconds(500));
        failing.Stop();
        if (failing.CallbackError() != 77 || failing.Count() != 1) return 37;
    }
    std::printf("real 60Hz worker emits kernel vblank; callback failure stops worker with error77\n");
    std::printf(
        "kernel display subscription passed real_callback_bridge=true coalescing=2 stale_queue_rejected=true\n");
    return 0;
}
