// SPDX-License-Identifier: GPL-2.0-only
// Independent owned stack allocation; public stack design audited in thread_create_stack_gap.json.
#pragma once
#include <limits>
#include "kernel_time.hpp"
namespace ctrport {
class GuestStack {
    void* allocation = nullptr;
    std::size_t reserved = 0, usable = 0, guard = 0;

public:
    GuestStack(std::size_t requested, std::size_t requested_guard) {
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        const std::size_t page = info.dwPageSize;
        constexpr std::size_t allowance = 128 * 1024, maximum = 64 * 1024 * 1024;
        if (!requested || requested > maximum || requested_guard > maximum || !page)
            throw std::runtime_error("Guest stack extent unsupported");
        auto round = [page](std::size_t value) {
            if (value > std::numeric_limits<std::size_t>::max() - (page - 1))
                throw std::runtime_error("Guest stack extent overflow");
            return ((value + page - 1) / page) * page;
        };
        usable = round(requested + allowance);
        guard = round(requested_guard);
        if (usable > maximum || guard > maximum - usable)
            throw std::runtime_error("Guest stack reservation exceeds bounded domain");
        reserved = usable + guard;
        allocation = VirtualAlloc(nullptr, reserved, MEM_RESERVE, PAGE_NOACCESS);
        if (!allocation) throw std::runtime_error("Guest stack reservation failed");
        if (!VirtualAlloc(static_cast<unsigned char*>(allocation) + guard, usable, MEM_COMMIT, PAGE_READWRITE)) {
            VirtualFree(allocation, 0, MEM_RELEASE);
            allocation = nullptr;
            throw std::runtime_error("Guest stack commit failed");
        }
        try {
            publish_owned_output(this, bottom(), usable);
        } catch (...) {
            VirtualFree(allocation, 0, MEM_RELEASE);
            allocation = nullptr;
            throw;
        }
    }
    GuestStack(const GuestStack&) = delete;
    GuestStack& operator=(const GuestStack&) = delete;
    ~GuestStack() {
        revoke_owned_output(this);
        if (allocation) VirtualFree(allocation, 0, MEM_RELEASE);
    }
    void* bottom() const { return static_cast<unsigned char*>(allocation) + guard; }
    void* top() const { return static_cast<unsigned char*>(allocation) + reserved; }
    void* base() const { return allocation; }
    std::size_t size() const { return usable; }
};
extern "C" std::uint64_t ctr_guest_stack_call(void*, void*, void*, void*);
// Guest thread entry with an exit frame for scePthreadExit (abi_probe.asm).
extern "C" std::uint64_t ctr_guest_thread_call(void*, void*, void*, void*, void**);
inline thread_local void* guest_thread_exit_frame = nullptr;
// scePthreadExit: returns the calling worker's exit frame (ctr_sysv_pthread_exit jumps to it).
// A thread without a published frame (the main thread) cannot exit this way: fail loudly.
extern "C" void* ctr_thread_exit_frame(std::uint64_t value, std::uint64_t caller) {
    if (!guest_thread_exit_frame) {
        std::fprintf(
            stderr,
            "NID=3kg7rT0NQIs library=libkernel caller=0x%llx unsupported: scePthreadExit outside a runtime worker thread\n",
            static_cast<unsigned long long>(caller));
        std::fflush(stderr);
        ExitProcess(4);
    }
    std::printf("{\"scope\":\"thread_exit\",\"nid\":\"3kg7rT0NQIs\",\"caller\":%llu,\"value\":%llu}\n",
                static_cast<unsigned long long>(caller), static_cast<unsigned long long>(value));
    std::fflush(stdout);
    return guest_thread_exit_frame;
}
extern "C" void ctr_sysv_pthread_exit();
extern "C" void ctr_guest_stack_fixture();
extern "C" int ctr_guest_stack_xmm_control(void*, void*, void*);
extern "C" void ctr_guest_stack_exception_fixture();
extern "C" unsigned char ctr_guest_stack_exception_site;
struct StackExceptionControl {
    std::uint64_t low = 0, high = 0;
    unsigned seen = 0;
};
inline thread_local StackExceptionControl* stack_exception_control = nullptr;
}
