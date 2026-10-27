// SPDX-License-Identifier: GPL-2.0-only
#include "gnm_executor_host.h"
/* Authored host ABI fixture. No PS4 manifest, NID or service return. */
static int owner_action(void* argument, void* session) {
    if (!argument || !session) return 60;
    *(uint32_t*)argument = 1;
    return 73;
}
__declspec(dllexport) int ctr_gnm_host_fixture(const CtrGnmExecutorHostV1* host, uint64_t addresses, uint64_t sizes) {
    uint64_t token = 0, count = 0, address = 0;
    const uint32_t* words = 0;
    uint32_t called = 0;
    CtrGuestCopyApi memory;
    uint32_t copied = 0;
    if (!host || host->size != sizeof(*host) || host->version != 1 || !host->capture || !host->command ||
        !host->release || !host->on_video_owner)
        return 1;
    if (host->capture(host->context, 1, addresses, sizes, 0, 0, &token)) return 2;
    if (host->command(host->context, token, 0, 0, &words, &count, &address) || count != 2 || !address || !words ||
        words[0] != 0xc0001000 || words[1] != 9) {
        host->release(host->context, token);
        return 3;
    }
    if (host->on_video_owner(host->context, owner_action, &called) != 73 || called != 1) {
        host->release(host->context, token);
        return 4;
    }
    if (!host->guest_memory || host->guest_memory(host->context, &memory) ||
        memory.read(memory.context, address + 4, &copied, 4) || copied != 9) {
        host->release(host->context, token);
        return 6;
    }
    if (host->release(host->context, token) || host->release(host->context, token) != 62) return 5;
    return 0;
}
