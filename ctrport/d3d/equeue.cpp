// SPDX-License-Identifier: GPL-2.0-or-later
// Event layout and semantics: public kqueue (FreeBSD) plus the PS4 conventions documented by
// shadPS4's kernel/equeue (GPL-2.0-or-later); this file is written for ctr_d3d.
#include "equeue.h"

#include <windows.h>
#include <intrin.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

struct Equeue {
    std::mutex m;
    std::condition_variable cv;
    struct Reg {
        u64 ident;
        s16 filter;
        u16 flags;
        u32 fflags;
        u64 data;
        void* udata;
        bool triggered;
    };
    std::vector<Reg> regs;
    std::string name;
};

namespace {
std::mutex g_queues_mutex;
std::unordered_set<Equeue*> g_queues;

struct KEvent {
    u64 ident;
    s16 filter;
    u16 flags;
    u32 fflags;
    u64 data;
    void* udata;
};
static_assert(sizeof(KEvent) == 32);
}  // namespace

Equeue* EqueueFromHandle(u64 handle) {
    std::lock_guard lk(g_queues_mutex);
    auto* q = reinterpret_cast<Equeue*>(handle);
    return g_queues.count(q) ? q : nullptr;
}

void EqueueAdd(Equeue* eq, u64 ident, s16 filter, u16 flags, u64 data, void* udata) {
    std::lock_guard lk(eq->m);
    for (auto& r : eq->regs) {
        if (r.ident == ident && r.filter == filter) {  // EV_ADD on an existing one modifies it
            r.flags = flags;
            r.udata = udata;
            return;
        }
    }
    eq->regs.push_back({ident, filter, flags, 0, data, udata, false});
}

bool EqueueDelete(Equeue* eq, u64 ident, s16 filter) {
    std::lock_guard lk(eq->m);
    return std::erase_if(eq->regs, [&](auto& r) { return r.ident == ident && r.filter == filter; }) != 0;
}

void EqueueTrigger(Equeue* eq, u64 ident, s16 filter, u64 data) {
    bool any = false;
    {
        std::lock_guard lk(eq->m);
        for (auto& r : eq->regs) {
            if (r.ident == ident && r.filter == filter) {
                r.triggered = true;
                r.data = data;
                any = true;
            }
        }
    }
    if (any) eq->cv.notify_all();
}

void EqueueTriggerDisplay(Equeue* eq, u64 ident, u64 hint) {
    bool any = false;
    {
        std::lock_guard lk(eq->m);
        for (auto& r : eq->regs) {
            if (r.ident != ident || r.filter != kFilterVideoOut) continue;
            u64 count = (r.data >> 12) & 0xF;
            if (count != 0xF) count++;
            r.data = (__rdtsc() & 0xFFF) | (count << 12) | (hint & 0xFFFFFFFFFFFF0000ull);
            r.triggered = true;
            any = true;
        }
    }
    if (any) eq->cv.notify_all();
}

namespace {
using namespace err;

SYSV int sceKernelCreateEqueue(u64* out, const char* name) {
    if (!out || !name) return SceErr(EINVAL_);
    auto* q = new Equeue();
    q->name = name;
    {
        std::lock_guard lk(g_queues_mutex);
        g_queues.insert(q);
    }
    *out = reinterpret_cast<u64>(q);
    return 0;
}

SYSV int sceKernelDeleteEqueue(u64 handle) {
    Equeue* q = EqueueFromHandle(handle);
    if (!q) return SceErr(EBADF_);
    {
        std::lock_guard lk(g_queues_mutex);
        g_queues.erase(q);
    }
    delete q;
    return 0;
}

// timeout: microseconds, nullptr = wait forever, 0 = poll
SYSV int sceKernelWaitEqueue(u64 handle, KEvent* ev, int num, int* out, const u32* timeout) {
    Equeue* eq = EqueueFromHandle(handle);
    if (!eq) return SceErr(EBADF_);
    if (!ev || num < 1) return SceErr(EINVAL_);
    std::unique_lock lk(eq->m);
    auto ready = [&] {
        for (auto& r : eq->regs)
            if (r.triggered) return true;
        return false;
    };
    if (!timeout) {
        eq->cv.wait(lk, ready);
    } else if (!eq->cv.wait_for(lk, std::chrono::microseconds(*timeout), ready)) {
        if (out) *out = 0;
        return SceErr(ETIMEDOUT_);
    }
    int n = 0;
    for (auto& r : eq->regs) {
        if (n >= num) break;
        if (!r.triggered) continue;
        ev[n++] = {r.ident, r.filter, r.flags, r.fflags, r.data, r.udata};
        if (r.flags & kEvClear) {
            r.triggered = false;
            if (r.filter == kFilterVideoOut) r.data &= ~0xF000ull;  // the count restarts
        }
    }
    if (out) *out = n;
    return 0;
}

SYSV int sceKernelAddUserEvent(u64 handle, int id) {
    Equeue* eq = EqueueFromHandle(handle);
    if (!eq) return SceErr(EBADF_);
    EqueueAdd(eq, (u64)id, kFilterUser, kEvAdd, 0, nullptr);
    return 0;
}
SYSV int sceKernelAddUserEventEdge(u64 handle, int id) {
    Equeue* eq = EqueueFromHandle(handle);
    if (!eq) return SceErr(EBADF_);
    EqueueAdd(eq, (u64)id, kFilterUser, kEvAdd | kEvClear, 0, nullptr);
    return 0;
}
SYSV int sceKernelTriggerUserEvent(u64 handle, int id, void* udata) {
    Equeue* eq = EqueueFromHandle(handle);
    if (!eq) return SceErr(EBADF_);
    bool found = false;
    {
        std::lock_guard lk(eq->m);
        for (auto& r : eq->regs) {
            if (r.ident == (u64)id && r.filter == kFilterUser) {
                r.triggered = true;
                r.udata = udata;
                found = true;
            }
        }
    }
    if (!found) return SceErr(ENOENT_);
    eq->cv.notify_all();
    return 0;
}
SYSV int sceKernelDeleteUserEvent(u64 handle, int id) {
    Equeue* eq = EqueueFromHandle(handle);
    if (!eq) return SceErr(EBADF_);
    return EqueueDelete(eq, (u64)id, kFilterUser) ? 0 : SceErr(ENOENT_);
}
SYSV void* sceKernelGetEventUserData(const KEvent* ev) { return ev ? ev->udata : nullptr; }
SYSV u64 sceKernelGetEventId(const KEvent* ev) { return ev ? ev->ident : 0; }
SYSV int sceKernelGetEventFilter(const KEvent* ev) { return ev ? ev->filter : 0; }
SYSV u64 sceKernelGetEventData(const KEvent* ev) { return ev ? ev->data : 0; }

EXPORT("D0OdFMjp46I", "libkernel", sceKernelCreateEqueue);
EXPORT("jpFjmgAC5AE", "libkernel", sceKernelDeleteEqueue);
EXPORT("fzyMKs9kim0", "libkernel", sceKernelWaitEqueue);
EXPORT("4R6-OvI2cEA", "libkernel", sceKernelAddUserEvent);
EXPORT("WDszmSbWuDk", "libkernel", sceKernelAddUserEventEdge);
EXPORT("F6e0kwo4cnk", "libkernel", sceKernelTriggerUserEvent);
EXPORT("LJDwdSNTnDg", "libkernel", sceKernelDeleteUserEvent);
EXPORT("vz+pg2zdopI", "libkernel", sceKernelGetEventUserData);
EXPORT("mJ7aghmgvfc", "libkernel", sceKernelGetEventId);
EXPORT("23CPPI1tyBY", "libkernel", sceKernelGetEventFilter);
EXPORT("kwGyyjohI50", "libkernel", sceKernelGetEventData);
}  // namespace
