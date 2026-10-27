// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "session_api.h"
#include "status_layout.h"
#include "../guest_copy_api.h"
// Owner-thread host adapter. Runtime owns guest mapping/serialization and error translation.
// No guest pointer dereference; callback owns checked destination copy and its lifetime.
static inline int ctr_video_copy_public_status(const CtrVideoSessionApiV1* session, const CtrGuestCopyApi* memory,
                                               uint64_t guest_address, int vblank) {
    if (!session || session->size != sizeof(*session) || session->version != 1 || !session->context || !memory ||
        memory->size != sizeof(*memory) || memory->version != 1 || !memory->context || !memory->write ||
        (vblank != 0 && vblank != 1))
        return 60;
    if (vblank) {
        CtrVideoPublicVblankStatus value = {0};
        if (!session->get_public_vblank) return 60;
        const int status = session->get_public_vblank(session->context, &value, sizeof(value));
        if (status) return status;
        return memory->write(memory->context, guest_address, &value, sizeof(value)) ? 112 : 0;
    } else {
        CtrVideoPublicFlipStatus value = {0};
        if (!session->get_public_flip) return 60;
        const int status = session->get_public_flip(session->context, &value, sizeof(value));
        if (status) return status;
        return memory->write(memory->context, guest_address, &value, sizeof(value)) ? 112 : 0;
    }
}

#include "../owner_dispatch_api.h"
typedef struct CtrVideoStatusRequest {
    const CtrVideoSessionApiV1* session;
    const CtrGuestCopyApi* memory;
    uint64_t address;
    int vblank;
} CtrVideoStatusRequest;
static inline int ctr_video_status_owner_action(void* raw) {
    const CtrVideoStatusRequest* request = (const CtrVideoStatusRequest*)raw;
    return ctr_video_copy_public_status(request->session, request->memory, request->address, request->vblank);
}
// Caller holds no loader/kernel/session locks. Tables, mapping and callback code
// remain live until invoke returns; owner must pump.115 is transport rejection.
static inline int ctr_video_dispatch_public_status(const CtrOwnerDispatchApi* owner,
                                                   const CtrVideoSessionApiV1* session, const CtrGuestCopyApi* memory,
                                                   uint64_t address, int vblank) {
    if (!owner || owner->size != sizeof(*owner) || owner->version != 1 || !owner->context || !owner->invoke) return 60;
    CtrVideoStatusRequest request = {session, memory, address, vblank};
    int result = 90;
    return owner->invoke(owner->context, ctr_video_status_owner_action, &request, &result) ? 115 : result;
}

// Explicit host display configuration snapshot, never inferred from swapchain size.
// Runtime owns provenance; configured=0 fails without a destination write.
typedef struct CtrVideoResolutionConfigV1 {
    uint32_t size, version, configured, reserved;
    CtrVideoPublicResolutionStatus value;
} CtrVideoResolutionConfigV1;
static inline int ctr_video_copy_resolution(const CtrVideoResolutionConfigV1* config, const CtrGuestCopyApi* memory,
                                            uint64_t address) {
    if (!config || config->size != sizeof(*config) || config->version != 1 || config->reserved || !memory ||
        memory->size != sizeof(*memory) || memory->version != 1 || !memory->context || !memory->write)
        return 60;
    if (config->configured != 1) return 91;
    const CtrVideoPublicResolutionStatus* v = &config->value;
    if (v->full_width <= 0 || v->full_height <= 0 || v->pane_width <= 0 || v->pane_height <= 0 ||
        v->pane_width > v->full_width || v->pane_height > v->full_height || !(v->screen_size_in_inch >= 0) ||
        v->screen_size_in_inch > 3.402823466e+38F || v->reserved0 || v->reserved1[0] || v->reserved1[1] ||
        v->reserved1[2] || v->padding)
        return 60;
    // Refresh is an opaque public enum (0 means unknown), not numeric Hertz.
    return memory->write(memory->context, address, v, sizeof(*v)) ? 112 : 0;
}

typedef struct CtrVideoResolutionRequest {
    const CtrVideoResolutionConfigV1* config;
    const CtrGuestCopyApi* memory;
    uint64_t address;
} CtrVideoResolutionRequest;
static inline int ctr_video_resolution_owner_action(void* raw) {
    const CtrVideoResolutionRequest* r = (const CtrVideoResolutionRequest*)raw;
    return ctr_video_copy_resolution(r->config, r->memory, r->address);
}
// Configuration immutable and mapping/code live until synchronous return; no locks held.
static inline int ctr_video_dispatch_resolution(const CtrOwnerDispatchApi* owner,
                                                const CtrVideoResolutionConfigV1* config, const CtrGuestCopyApi* memory,
                                                uint64_t address) {
    if (!owner || owner->size != sizeof(*owner) || owner->version != 1 || !owner->context || !owner->invoke) return 60;
    CtrVideoResolutionRequest request = {config, memory, address};
    int result = 90;
    return owner->invoke(owner->context, ctr_video_resolution_owner_action, &request, &result) ? 115 : result;
}
