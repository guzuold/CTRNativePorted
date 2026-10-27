// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "session_api.h"
#include "../guest_copy_api.h"
// Owner-thread checked8B output; runtime validates guest handle/authority.
static inline int ctr_video_copy_capability(const CtrVideoSessionApiV1* api, const CtrGuestCopyApi* memory,
                                            uint64_t address) {
    uint64_t value = 0;
    int status;
    if (!api || api->size != sizeof(*api) || api->version != 1 || !api->context || !api->get_public_capability ||
        !memory || memory->size != sizeof(*memory) || memory->version != 1 || !memory->context || !memory->write)
        return 60;
    status = api->get_public_capability(api->context, &value);
    if (status) return status;
    return memory->write(memory->context, address, &value, 8) ? 112 : 0;
}
