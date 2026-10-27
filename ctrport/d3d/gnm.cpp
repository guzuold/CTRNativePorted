// SPDX-License-Identifier: MIT
// libSceGnmDriver as an engine-state backend: command buffers are not executed by a GPU model.
// Draws are issued at record time from the engine's device context (renderer.cpp); submitted
// command buffers are scanned only for the packets whose memory writes and interrupts the CPU
// waits on (EOP/EOS/RELEASE_MEM/WRITE_DATA/DMA, flip labels), which are performed at once.
// Adapted from FrankHUN88453/CTR-NitroFueled-PC 96b4aa5474b1d42b0d9f383349dc091ea9a2d385
// port/src/hle/gnm.cpp (MIT, ctrport/d3d/LICENSE.FrankHUN88453). Added for ctrport: all 86
// libSceGnmDriver functions CUSA14876 imports (bound by NID), flip-label and flip-interrupt
// payloads, asynchronous compute queues (MapComputeQueue / DingDong), workload streams,
// indexed-offset draws, and a per-function call census of draw and dispatch variants that are
// not rendered yet (logged with their callers, never silently).
#include <windows.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "equeue.h"
#include "gfx.h"
#include "guest.h"
#include "videoout.h"

namespace {

enum : u32 {
    IT_NOP = 0x10,
    IT_INDIRECT_BUFFER_CONST = 0x33,
    IT_WRITE_DATA = 0x37,
    IT_INDIRECT_BUFFER = 0x3F,
    IT_EVENT_WRITE_EOP = 0x47,
    IT_EVENT_WRITE_EOS = 0x48,
    IT_RELEASE_MEM = 0x49,
    IT_DMA_DATA = 0x50,
};
// NOP payloads written by the Gnm library (prepareFlip and its variants)
constexpr u32 kPrepareFlip = 0x68750777u, kPrepareFlipLabel = 0x68750778u, kPrepareFlipInterrupt = 0x68750780u,
              kPrepareFlipInterruptLabel = 0x68750781u;
constexpr u64 kIrqGfxEop = 0x40;
constexpr int kGnmErrFailure = (int)0x8EEE00FFu, kGnmErrRazorNotLoaded = (int)0x80D15001u,
              kGnmErrCaptureFailed = (int)0x80D1500Fu, kGnmErrInvalidPipe = (int)0x80D17000u,
              kGnmErrInvalidQueue = (int)0x80D17001u;

std::mutex g_eq_mutex;
std::vector<std::pair<Equeue*, u64>> g_gpu_events; // (queue, interrupt id)
std::atomic<u64> g_submits{0};

u64 Addr(u32 lo, u32 hi16) {
    return (u64)lo | ((u64)(hi16 & 0xFFFF) << 32);
}

void WriteData(u64 addr, u32 sel, u32 lo, u32 hi) {
    switch (sel) {
    case 1: // 32-bit data
        if (GuestReadable(addr, 4)) *(u32*)addr = lo;
        break;
    case 2: // 64-bit data
        if (GuestReadable(addr, 8)) *(u64*)addr = (u64)lo | ((u64)hi << 32);
        break;
    case 3: // 64-bit GPU clock
    case 4: {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        if (GuestReadable(addr, 8)) *(u64*)addr = (u64)c.QuadPart;
        break;
    }
    default: break;
    }
}

void RaiseInterrupt(u64 id) {
    std::lock_guard lk(g_eq_mutex);
    for (auto [q, ident] : g_gpu_events)
        if (ident == id) EqueueTrigger(q, id, kFilterGraphicsCore, id);
}

// Performs the CPU-visible effects of a command buffer. `compute`: an asynchronous compute
// ring (RELEASE_MEM interrupts are that pipe's).
void Process(const u32* cmd, u64 dwords, int depth = 0, int compute_pipe = -1) {
    if (!cmd || depth > 4) return;
    u64 i = 0;
    while (i < dwords) {
        u32 h = cmd[i];
        u32 type = h >> 30;
        if (type == 2) { // filler
            i++;
            continue;
        }
        u32 count = ((h >> 16) & 0x3FFF) + 2;
        if (type == 0 || type == 1) {
            i += count;
            continue;
        }
        if (i + count > dwords) break;
        const u32* b = cmd + i + 1;
        switch ((h >> 8) & 0xFF) {
        case IT_EVENT_WRITE_EOP: {
            u32 sel = (b[2] >> 29) & 7, intsel = (b[2] >> 24) & 3;
            WriteData(Addr(b[1], b[2]), sel, b[3], b[4]);
            if (intsel) RaiseInterrupt(kIrqGfxEop);
            break;
        }
        case IT_EVENT_WRITE_EOS: {
            u32 cmdsel = (b[2] >> 29) & 7;
            if (cmdsel == 2) WriteData(Addr(b[1], b[2]), 1, b[3], 0);
            break;
        }
        case IT_RELEASE_MEM: {
            u32 sel = (b[1] >> 29) & 7, intsel = (b[1] >> 24) & 3;
            WriteData(Addr(b[2], b[3]), sel, b[4], b[5]);
            if (intsel) RaiseInterrupt(compute_pipe >= 0 ? (u64)compute_pipe : kIrqGfxEop);
            break;
        }
        case IT_WRITE_DATA: {
            u32 dst = (b[0] >> 8) & 0xF;
            if (dst == 5 || dst == 2) { // memory
                u64 a = (u64)b[1] | ((u64)b[2] << 32);
                u32 n = count - 4;
                if (GuestReadable(a, n * 4ull)) memcpy((void*)a, b + 3, n * 4ull);
            }
            break;
        }
        case IT_DMA_DATA: {
            u32 src_sel = (b[0] >> 29) & 3, dst_sel = (b[0] >> 20) & 3;
            u32 bytes = b[5] & 0x1FFFFF;
            u64 dst = (u64)b[3] | ((u64)b[4] << 32);
            if (dst_sel == 0 && GuestReadable(dst, bytes)) {
                if (src_sel == 2) { // fill with immediate
                    for (u32 k = 0; k + 4 <= bytes; k += 4) *(u32*)(dst + k) = b[1];
                } else if (src_sel == 0) {
                    u64 src = (u64)b[1] | ((u64)b[2] << 32);
                    if (GuestReadable(src, bytes)) memmove((void*)dst, (void*)src, bytes);
                }
            }
            break;
        }
        case IT_INDIRECT_BUFFER:
        case IT_INDIRECT_BUFFER_CONST: {
            u64 a = (u64)(b[0] & ~3u) | ((u64)(b[1] & 0xFFFF) << 32);
            u32 n = b[2] & 0xFFFFF;
            if (GuestReadable(a, n * 4ull)) Process((const u32*)a, n, depth + 1, compute_pipe);
            break;
        }
        default: break;
        }
        i += count;
    }
}

void ProcessSubmit(u32 count, const u32* const* dcb, const u32* dcb_bytes, const u32* const* ccb,
                   const u32* ccb_bytes) {
    for (u32 k = 0; k < count; k++) {
        if (ccb && ccb[k] && ccb_bytes) Process(ccb[k], ccb_bytes[k] / 4);
        if (dcb && dcb[k]) Process(dcb[k], dcb_bytes[k] / 4);
    }
    u64 n = ++g_submits;
    if (n == 1 || (n & (n - 1)) == 0) LOG("gnm: %llu submits", (unsigned long long)n);
}

// The prepareFlip packet the Gnm library reserves in the last 64 dwords of the last command
// buffer of a submit-and-flip: after the flip, its label is written and its interrupt raised.
void CompleteFlipPacket(const u32* last, u32 bytes) {
    if (!last || bytes < 64 * 4) return;
    const u32* p = last + bytes / 4 - 64;
    if (p[0] != 0xc03e1000u) return;
    const u32 kind = p[1];
    if (kind == kPrepareFlipLabel || kind == kPrepareFlipInterruptLabel) {
        const u64 label = (u64)(p[2] & ~3u) | ((u64)p[3] << 32);
        if (GuestReadable(label, 4)) *(u32*)label = p[4];
    }
    if (kind == kPrepareFlipInterrupt || kind == kPrepareFlipInterruptLabel) RaiseInterrupt(kIrqGfxEop);
}

// ---- submission
SYSV int sceGnmSubmitCommandBuffers(u32 count, const u32* const* dcb, const u32* dcb_bytes, const u32* const* ccb,
                                    const u32* ccb_bytes) {
    ProcessSubmit(count, dcb, dcb_bytes, ccb, ccb_bytes);
    return 0;
}
SYSV int sceGnmSubmitCommandBuffersForWorkload(u64, u32 count, const u32* const* dcb, const u32* dcb_bytes,
                                               const u32* const* ccb, const u32* ccb_bytes) {
    ProcessSubmit(count, dcb, dcb_bytes, ccb, ccb_bytes);
    return 0;
}
SYSV int sceGnmSubmitAndFlipCommandBuffers(u32 count, const u32* const* dcb, const u32* dcb_bytes,
                                           const u32* const* ccb, const u32* ccb_bytes, int handle, int index, int mode,
                                           s64 arg) {
    if (!count || !dcb || !dcb_bytes) return -1;
    ProcessSubmit(count, dcb, dcb_bytes, ccb, ccb_bytes);
    VideoOutFlip(handle, index, arg);
    CompleteFlipPacket(dcb[count - 1], dcb_bytes[count - 1]);
    return 0;
}
SYSV int sceGnmSubmitAndFlipCommandBuffersForWorkload(u64, u32 count, const u32* const* dcb, const u32* dcb_bytes,
                                                      const u32* const* ccb, const u32* ccb_bytes, int handle,
                                                      int index, int mode, s64 arg) {
    return sceGnmSubmitAndFlipCommandBuffers(count, dcb, dcb_bytes, ccb, ccb_bytes, handle, index, mode, arg);
}
SYSV int sceGnmSubmitDone() {
    return 0;
}
SYSV int sceGnmAreSubmitsAllowed() {
    return 1;
}
SYSV int sceGnmFlushGarlic() {
    return 0;
}
EXPORT("zwY0YV91TTI", "libSceGnmDriver", sceGnmSubmitCommandBuffers);
EXPORT("jRcI8VcgTz4", "libSceGnmDriver", sceGnmSubmitCommandBuffersForWorkload);
EXPORT("xbxNatawohc", "libSceGnmDriver", sceGnmSubmitAndFlipCommandBuffers);
EXPORT("Ga6r7H6Y0RI", "libSceGnmDriver", sceGnmSubmitAndFlipCommandBuffersForWorkload);
EXPORT("yvZ73uQUqrk", "libSceGnmDriver", sceGnmSubmitDone);
EXPORT("b08AgtPlHPg", "libSceGnmDriver", sceGnmAreSubmitsAllowed);
EXPORT("iBt3Oe00Kvc", "libSceGnmDriver", sceGnmFlushGarlic);

// ---- GPU interrupts on event queues
struct KEvent {
    u64 ident;
    s16 filter;
    u16 flags;
    u32 fflags;
    u64 data;
    void* udata;
};
SYSV int sceGnmAddEqEvent(u64 eqh, u64 id, void* udata) {
    Equeue* eq = EqueueFromHandle(eqh);
    if (!eq) return SceErr(err::EBADF_);
    EqueueAdd(eq, id, kFilterGraphicsCore, kEvAdd | kEvClear, id, udata);
    std::lock_guard lk(g_eq_mutex);
    g_gpu_events.push_back({eq, id});
    return 0;
}
SYSV int sceGnmDeleteEqEvent(u64 eqh, u64 id) {
    Equeue* eq = EqueueFromHandle(eqh);
    if (!eq) return SceErr(err::EBADF_);
    EqueueDelete(eq, id, kFilterGraphicsCore);
    std::lock_guard lk(g_eq_mutex);
    std::erase_if(g_gpu_events, [&](auto& e) { return e.first == eq && e.second == id; });
    return 0;
}
// the event data is the interrupt id (EOP 0x40, compute pipes 0..6)
SYSV int sceGnmGetEqEventType(const KEvent* ev) {
    return ev ? (int)ev->data : 0;
}
EXPORT("b0xyllnVY-I", "libSceGnmDriver", sceGnmAddEqEvent);
EXPORT("PVT+fuoS9gU", "libSceGnmDriver", sceGnmDeleteEqEvent);
EXPORT("UoYY0DWMC0U", "libSceGnmDriver", sceGnmGetEqEventType);

// ---- command-buffer writers: (u32* cmd, u32 dwords, ...) -> fill with a NOP packet
void Nop(u32* cmd, u32 dwords) {
    if (!cmd || dwords == 0) return;
    if (dwords == 1) {
        cmd[0] = 0x80000000;
        return;
    }
    cmd[0] = 0xC0000000 | ((dwords - 2) << 16) | (IT_NOP << 8);
    memset(cmd + 1, 0, (dwords - 1) * 4ull);
}
#define GNM_NOP(nid, fn)                                                                                               \
    SYSV int fn(u32* cmd, u32 dwords) {                                                                                \
        Nop(cmd, dwords);                                                                                              \
        return 0;                                                                                                      \
    }                                                                                                                  \
    EXPORT(nid, "libSceGnmDriver", fn)

// Draw / dispatch variants the renderer does not issue yet: each call is counted and logged
// with its caller (the first few, then at powers of two), so a run shows what is missing.
void Census(const char* fn, u64 caller) {
    static std::mutex m;
    static std::unordered_map<const char*, u64> calls;
    u64 n;
    {
        std::lock_guard lk(m);
        n = ++calls[fn];
    }
    if (n <= 3 || (n & (n - 1)) == 0)
        LOG("gnm: %s not rendered by the D3D11 backend (call %llu, caller %llx)", fn, (unsigned long long)n,
            (unsigned long long)(caller - (u64)g_eboot.base));
}
#define GNM_NOT_RENDERED(nid, fn)                                                                                      \
    SYSV int fn(u32* cmd, u32 dwords) {                                                                                \
        Census(#fn, CALLER());                                                                                         \
        Nop(cmd, dwords);                                                                                              \
        return 0;                                                                                                      \
    }                                                                                                                  \
    EXPORT(nid, "libSceGnmDriver", fn)

SYSV int sceGnmDrawIndex(u32* cmd, u32 dwords, u32 count, u64 addr, u32 flags, u32 type) {
    GfxDraw(count, addr, true);
    Nop(cmd, dwords);
    return 0;
}
SYSV int sceGnmDrawIndexAuto(u32* cmd, u32 dwords, u32 count, u32 flags) {
    GfxDraw(count, 0, false);
    Nop(cmd, dwords);
    return 0;
}
// Indexed draw from the index buffer bound in the device context, starting `index_offset`
// indices in.
SYSV int sceGnmDrawIndexOffset(u32* cmd, u32 dwords, u32 index_offset, u32 count, u32 flags) {
    GfxDrawIndexOffset(index_offset, count);
    Nop(cmd, dwords);
    return 0;
}
EXPORT("HlTPoZ-oY7Y", "libSceGnmDriver", sceGnmDrawIndex);
EXPORT("GGsn7jMTxw4", "libSceGnmDriver", sceGnmDrawIndexAuto);
EXPORT("oYM+YzfCm2Y", "libSceGnmDriver", sceGnmDrawIndexOffset);

GNM_NOT_RENDERED("4v+otIIdjqg", sceGnmDrawIndirect);
GNM_NOT_RENDERED("ED9-Fjr8Ta4", sceGnmDrawIndexIndirect);
GNM_NOT_RENDERED("thbPcG7E7qk", sceGnmDrawIndexIndirectCountMulti);
GNM_NOT_RENDERED("cUCo8OvArrw", sceGnmDrawIndirectCountMulti);
GNM_NOT_RENDERED("jHdPvIzlpKc", sceGnmDrawIndexMultiInstanced);
GNM_NOT_RENDERED("stDSYW2SBVM", sceGnmDrawOpaqueAuto);
GNM_NOT_RENDERED("Z43vKp5k7r0", sceGnmDispatchIndirect);
GNM_NOT_RENDERED("wED4ZXCFJT0", sceGnmDispatchIndirectOnMec);

// Compute: the program set by SetCsShaderWithModifier (COMPUTE_PGM_LO / HI, RSRC1 / 2,
// NUM_THREAD_X / Y / Z) is reported per dispatch site until the backend runs compute shaders.
thread_local u64 t_cs_program = 0;
thread_local u32 t_cs_regs[7];
SYSV int sceGnmSetCsShaderWithModifier(u32* cmd, u32 dwords, const u32* regs, u32 modifier) {
    if (regs) {
        t_cs_program = ((u64)regs[1] << 40 | (u64)regs[0] << 8);
        memcpy(t_cs_regs, regs, sizeof(t_cs_regs));
    }
    Nop(cmd, dwords);
    return 0;
}
SYSV int sceGnmDispatchDirect(u32* cmd, u32 dwords, u32 x, u32 y, u32 z, u32 mode) {
    GfxDispatch(t_cs_program, t_cs_regs, x, y, z, CALLER());
    Nop(cmd, dwords);
    return 0;
}
EXPORT("Kx-h-nWQJ8A", "libSceGnmDriver", sceGnmSetCsShaderWithModifier);
EXPORT("0BzLGljcwBo", "libSceGnmDriver", sceGnmDispatchDirect);

// Shader stage setters: the renderer takes the shaders from the engine's context, so the
// register writes are not needed.
GNM_NOP("gAhCn6UiU4Y", sceGnmSetVsShader);
GNM_NOP("5uFKckiJYRM", sceGnmSetPsShader350);
GNM_NOP("FUHG8sQ3R58", sceGnmSetEsShader);
GNM_NOP("UJwNuMBcUAk", sceGnmSetGsShader);
GNM_NOP("VJNjFtqiF5w", sceGnmSetHsShader);
GNM_NOP("vckdzbQ46SI", sceGnmSetLsShader);
GNM_NOP("+AFvOEXrKJk", sceGnmSetEmbeddedVsShader);
GNM_NOP("X9Omw9dwv5M", sceGnmSetEmbeddedPsShader);
GNM_NOP("V31V01UiScY", sceGnmUpdateVsShader);
GNM_NOP("mLVL7N7BVBg", sceGnmUpdatePsShader350);
GNM_NOP("nLM2i2+65hA", sceGnmUpdateGsShader);
GNM_NOP("GNlx+y7xPdE", sceGnmUpdateHsShader);
GNM_NOP("cFCp0NX8wf0", sceGnmSetVgtControl);
GNM_NOP("MYRtYhojKdA", sceGnmResetVgtControl);
// Waits and markers: every submitted buffer completes at once, debugger markers have no reader.
GNM_NOP("1qXLHIpROPE", sceGnmInsertWaitFlipDone);
GNM_NOP("ffrNQOshows", sceGnmComputeWaitOnAddress);
GNM_NOP("EJapNl2+pgU", sceGnmComputeWaitSemaphore);
GNM_NOP("W1Etj-jlW7Y", sceGnmInsertPushMarker);
GNM_NOP("7qZVNgEu+SY", sceGnmInsertPopMarker);
GNM_NOP("jiItzS6+22g", sceGnmInsertSetMarker);
GNM_NOP("aPIZJTXC+cU", sceGnmInsertPushColorMarker);
GNM_NOP("aj3L-iaFmyk", sceGnmInsertSetColorMarker);
GNM_NOP("NfvOrNzy6sk", sceGnmInsertDingDongMarker);

// init-state writers return the number of dwords they wrote
SYSV u32 sceGnmDrawInitDefaultHardwareState350(u32* cmd, u32 dwords) {
    Nop(cmd, dwords);
    return dwords;
}
SYSV u32 sceGnmDispatchInitDefaultHardwareState(u32* cmd, u32 dwords) {
    Nop(cmd, dwords);
    return dwords;
}
SYSV u32 sceGnmDrawInitToDefaultContextState400(u32* cmd, u32 dwords) {
    Nop(cmd, dwords);
    return dwords;
}
EXPORT("yb2cRhagD1I", "libSceGnmDriver", sceGnmDrawInitDefaultHardwareState350);
EXPORT("nF6bFRUBRAU", "libSceGnmDriver", sceGnmDispatchInitDefaultHardwareState);
EXPORT("im2ZuItabu4", "libSceGnmDriver", sceGnmDrawInitToDefaultContextState400);

// ---- asynchronous compute queues: rings the game writes and kicks with DingDong
struct ComputeQueue {
    u64 ring = 0;
    u32 dwords = 0, pipe = 0, read = 0;
    u32* read_ptr = nullptr;
};
std::mutex g_cq_mutex;
std::vector<ComputeQueue> g_compute_queues; // vqid - 1
SYSV int sceGnmMapComputeQueue(u32 pipe, u32 queue, u64 ring, u32 dwords, u32* read_ptr) {
    if (pipe >= 7) return kGnmErrInvalidPipe;
    if (queue >= 8) return kGnmErrInvalidQueue;
    if (ring % 256 || !dwords || (dwords & (dwords - 1)) || ((u64)read_ptr & 3)) return kGnmErrFailure;
    std::lock_guard lk(g_cq_mutex);
    g_compute_queues.push_back({ring, dwords, pipe, 0, read_ptr});
    if (read_ptr) *read_ptr = 0;
    LOG("gnm: compute queue %zu: pipe %u queue %u ring %llx (%u dwords)", g_compute_queues.size(), pipe, queue,
        (unsigned long long)ring, dwords);
    return (int)g_compute_queues.size();
}
SYSV int sceGnmMapComputeQueueWithPriority(u32 pipe, u32 queue, u64 ring, u32 dwords, u32* read_ptr, u32) {
    return sceGnmMapComputeQueue(pipe, queue, ring, dwords, read_ptr);
}
SYSV int sceGnmUnmapComputeQueue(u32 vqid) {
    std::lock_guard lk(g_cq_mutex);
    if (vqid < 1 || vqid > g_compute_queues.size()) return kGnmErrFailure;
    g_compute_queues[vqid - 1] = {};
    return 0;
}
SYSV void sceGnmDingDong(u32 vqid, u32 next) {
    std::lock_guard lk(g_cq_mutex);
    if (vqid < 1 || vqid > g_compute_queues.size() || !g_compute_queues[vqid - 1].ring) {
        LOG("gnm: DingDong on unknown compute queue %u", vqid);
        return;
    }
    ComputeQueue& q = g_compute_queues[vqid - 1];
    const u32* ring = (const u32*)q.ring;
    next %= q.dwords;
    if (next >= q.read) {
        Process(ring + q.read, next - q.read, 0, (int)q.pipe);
    } else { // wrapped
        Process(ring + q.read, q.dwords - q.read, 0, (int)q.pipe);
        Process(ring, next, 0, (int)q.pipe);
    }
    q.read = next;
    if (q.read_ptr) *q.read_ptr = next;
}
SYSV void sceGnmDingDongForWorkload(u32 vqid, u32 next, u64) {
    sceGnmDingDong(vqid, next);
}
EXPORT("29oKvKXzEZo", "libSceGnmDriver", sceGnmMapComputeQueue);
EXPORT("A+uGq+3KFtQ", "libSceGnmDriver", sceGnmMapComputeQueueWithPriority);
EXPORT("ArSg-TGinhk", "libSceGnmDriver", sceGnmUnmapComputeQueue);
EXPORT("bX5IbRvECXk", "libSceGnmDriver", sceGnmDingDong);
EXPORT("byXlqupd8cE", "libSceGnmDriver", sceGnmDingDongForWorkload);

// ---- workload streams (bookkeeping only, as the retail library validates them)
SYSV int sceGnmCreateWorkloadStream(u64 name, u32* stream) {
    if (!name || !stream) return 3;
    *stream = 1;
    return 0;
}
SYSV int sceGnmBeginWorkload(u32 stream, u64* workload) {
    if (!workload) return 3;
    *workload = stream < 0x10 ? 1 : 0;
    return stream > 0xf;
}
SYSV int sceGnmEndWorkload(u64 workload) {
    return workload ? (((workload >> 56) & 0xff) > 0xf) * 2 : 2;
}
SYSV int sceGnmDestroyWorkloadStream(u32) {
    return 0;
}
EXPORT("5udAm+6boVg", "libSceGnmDriver", sceGnmCreateWorkloadStream);
EXPORT("ihxrbsoSKWc", "libSceGnmDriver", sceGnmBeginWorkload);
EXPORT("Fa3x75OOLRA", "libSceGnmDriver", sceGnmEndWorkload);
EXPORT("UtObDRQiGbs", "libSceGnmDriver", sceGnmDestroyWorkloadStream);

// ---- queries and development-kit services (retail behaviour: tools absent)
u8 g_tess_ring[0x10000];
SYSV void* sceGnmGetTheTessellationFactorRingBufferBaseAddress() {
    return g_tess_ring;
}
SYSV u32 sceGnmGetOffChipTessellationBufferSize() {
    return 0x800000;
}
SYSV int sceGnmIsUserPaEnabled() {
    return 0;
}
SYSV int sceGnmDriverCaptureInProgress() {
    return 0;
}
SYSV int sceGnmDriverTraceInProgress() {
    return 0;
}
SYSV int sceGnmDriverTriggerCapture() {
    return kGnmErrRazorNotLoaded;
}
SYSV int sceRazorIsLoaded() {
    return 0;
}
SYSV int sceRazorCaptureImmediate() {
    return kGnmErrCaptureFailed;
}
SYSV u32 sceGnmGetNumTcaUnits() {
    return 18;
}
SYSV int sceGnmGetGpuInfoStatus() {
    return 0;
}
SYSV int sceGnmGetShaderStatus() {
    return 0;
}
SYSV int sceGnmGetGpuBlockStatus() {
    return 0;
}
SYSV int sceGnmDebugHardwareStatus() {
    return 0;
}
SYSV int sceGnmGetLastWaitedAddress() {
    return 0;
}
SYSV u32 sceGnmGetProtectionFaultTimeStamp() {
    return 0;
}
SYSV u64 sceGnmGetEqTimeStamp() {
    return 0;
}
SYSV int sceGnmGetShaderProgramBaseAddress() {
    return 0;
}
SYSV int sceGnmGetPhysicalCounterFromVirtualized() {
    return kGnmErrFailure;
}
SYSV int sceGnmLogicalCuIndexToPhysicalCuIndex() {
    return 0;
}
SYSV s32 sceGnmLogicalCuMaskToPhysicalCuMask(s64, s32 mask) {
    return mask;
}
SYSV int sceGnmLogicalTcaUnitToPhysical() {
    return 0;
}
SYSV int sceGnmSetGsRingSizes() {
    return 0;
}
SYSV int sceGnmSetWaveLimitMultipliers() {
    return 0;
}
SYSV int sceGnmSetupMipStatsReport() {
    return 0;
}
SYSV int sceGnmRequestMipStatsReportAndReset() {
    return 0;
}
SYSV int sceGnmDisableMipStatsReport() {
    return 0;
}
EXPORT("ln33zjBrfjk", "libSceGnmDriver", sceGnmGetTheTessellationFactorRingBufferBaseAddress);
EXPORT("FFVZcCu3zWU", "libSceGnmDriver", sceGnmGetOffChipTessellationBufferSize);
EXPORT("jg33rEKLfVs", "libSceGnmDriver", sceGnmIsUserPaEnabled);
EXPORT("TLV4mswiZ4A", "libSceGnmDriver", sceGnmDriverCaptureInProgress);
EXPORT("R6z1xM3pW-w", "libSceGnmDriver", sceGnmDriverTraceInProgress);
EXPORT("d88anrgNoKY", "libSceGnmDriver", sceGnmDriverTriggerCapture);
EXPORT("f33OrruQYbM", "libSceGnmDriver", sceRazorIsLoaded);
EXPORT("u9YKpRRHe-M", "libSceGnmDriver", sceRazorCaptureImmediate);
EXPORT("KnldROUkWJY", "libSceGnmDriver", sceGnmGetNumTcaUnits);
EXPORT("tZCSL5ulnB4", "libSceGnmDriver", sceGnmGetGpuInfoStatus);
EXPORT("k7iGTvDQPLQ", "libSceGnmDriver", sceGnmGetShaderStatus);
EXPORT("oL4hGI1PMpw", "libSceGnmDriver", sceGnmGetGpuBlockStatus);
EXPORT("qpGITzPE+Zc", "libSceGnmDriver", sceGnmDebugHardwareStatus);
EXPORT("iFirFzgYsvw", "libSceGnmDriver", sceGnmGetLastWaitedAddress);
EXPORT("fzJdEihTFV4", "libSceGnmDriver", sceGnmGetProtectionFaultTimeStamp);
EXPORT("H7-fgvEutM0", "libSceGnmDriver", sceGnmGetEqTimeStamp);
EXPORT("nEyFbYUloIM", "libSceGnmDriver", sceGnmGetShaderProgramBaseAddress);
EXPORT("dewXw5roLs0", "libSceGnmDriver", sceGnmGetPhysicalCounterFromVirtualized);
EXPORT("26PM5Mzl8zc", "libSceGnmDriver", sceGnmLogicalCuIndexToPhysicalCuIndex);
EXPORT("RU74kek-N0c", "libSceGnmDriver", sceGnmLogicalCuMaskToPhysicalCuMask);
EXPORT("Kl0Z3LH07QI", "libSceGnmDriver", sceGnmLogicalTcaUnitToPhysical);
EXPORT("jtkqXpAOY6w", "libSceGnmDriver", sceGnmSetGsRingSizes);
EXPORT("XiyzNZ9J4nQ", "libSceGnmDriver", sceGnmSetWaveLimitMultipliers);
EXPORT("+xuDhxlWRPg", "libSceGnmDriver", sceGnmSetupMipStatsReport);
EXPORT("f85orjx7qts", "libSceGnmDriver", sceGnmRequestMipStatsReportAndReset);
EXPORT("HHo1BAljZO8", "libSceGnmDriver", sceGnmDisableMipStatsReport);

// sceGnmRequestFlipAndSubmitDone(ForWorkload): imported but its arguments are not known; a call
// stops the process with its caller instead of guessing a flip.
SYSV int sceGnmRequestFlipAndSubmitDone() {
    Unsupported("libSceGnmDriver", "sceGnmRequestFlipAndSubmitDone", "gObODli-OH8", CALLER(),
                "argument layout unknown");
}
SYSV int sceGnmRequestFlipAndSubmitDoneForWorkload() {
    Unsupported("libSceGnmDriver", "sceGnmRequestFlipAndSubmitDoneForWorkload", "6YRHhh5mHCs", CALLER(),
                "argument layout unknown");
}
EXPORT("gObODli-OH8", "libSceGnmDriver", sceGnmRequestFlipAndSubmitDone);
EXPORT("6YRHhh5mHCs", "libSceGnmDriver", sceGnmRequestFlipAndSubmitDoneForWorkload);

} // namespace

u64 GnmSubmitCount() {
    return g_submits;
}
