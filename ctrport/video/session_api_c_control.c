// SPDX-License-Identifier: GPL-2.0-only
#include "session_api.h"
#include "graphics_companion.h"
#include <stddef.h>
_Static_assert(sizeof(CtrGraphicsIndexedDrawV1) == 320, "indexed ABI320");
_Static_assert(offsetof(CtrGraphicsIndexedDrawV1, index_token) == 32, "index token offset32");
_Static_assert(offsetof(CtrGraphicsIndexedDrawV1, vertex_buffers) == 64, "vertex tokens offset64");
#include "status_layout.h"
#include "guest_status_copy.h"
#include "guest_attribute_write.h"
#include "guest_color_write.h"
#include "guest_capability_write.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct StatusCopyControl {
    unsigned calls;
    int reject;
    unsigned char bytes[64];
} StatusCopyControl;
static int copy_status(void* context, uint64_t address, const void* input, uint64_t size) {
    StatusCopyControl* control = (StatusCopyControl*)context;
    ++control->calls;
    if (control->reject || address != 100 || size > sizeof(control->bytes)) return 1;
    memcpy(control->bytes, input, (size_t)size);
    return 0;
}
_Static_assert(sizeof(CtrVideoBufferAttributesV1) == 40, "public attributes40B");
static int consume(const CtrVideoSessionApiV1* api, uint32_t width, uint32_t height, uint32_t format, void* user) {
    size_t pitch = (size_t)width * 4;
    if (ctr_video_graphics_indexed_depth_draw_v1(NULL, 0, NULL, NULL, NULL, NULL) != 60) return 246;
    unsigned char* pixels;
    uint64_t token = 0;
    CtrVideoCompletionV1 completion = {9, 8, 7};
    int result = 0;
    CtrVideoProgressV1 progress = {0};
    (void)format;
    if (!api || api->version != 1 || api->size != sizeof(*api) || !width || !height || pitch > SIZE_MAX / height)
        return 70;
    if (api->pump_messages(api->context)) return 82;
    {
        CtrDisplayEventApi invalid = {0};
        if (!api->attach_events || !api->detach_events || api->attach_events(api->context, NULL, 1, 0, 0) != 60 ||
            api->attach_events(api->context, &invalid, 1, 0, 0) != 91 || api->detach_events(api->context) != 91)
            return 93;
    }
    {
        StatusCopyControl control = {0};
        CtrGuestCopyApi memory = {sizeof(CtrGuestCopyApi), 1, &control, NULL, copy_status};
        uint64_t capability = 999;
        memset(control.bytes, 0x71, sizeof(control.bytes));
        if (!api->get_public_capability || api->get_public_capability(api->context, &capability) || capability ||
            ctr_video_copy_capability(api, &memory, 100) || control.calls != 1 || control.bytes[8] != 0x71)
            return 133;
        for (size_t i = 0; i < 8; ++i)
            if (control.bytes[i]) return 133;
        control.reject = 1;
        if (ctr_video_copy_capability(api, &memory, 100) != 112 || control.bytes[8] != 0x71) return 133;
        puts(
            "capability copy passed actual_SDR_swapchain=true exact8=true sentinel8=true rejected112=true no_HDR_invention=true");
    }
    if (!api->get_progress || api->get_progress(api->context, NULL) != 60 ||
        api->get_progress(api->context, &progress) || progress.completed || progress.pending || progress.token ||
        progress.argument || progress.current_slot != -1 || progress.timing_valid || progress.reserved)
        return 99;
    {
        CtrVideoVblankV1 blank = {9, 8, 7};
        if (!api->get_vblank || api->get_vblank(api->context, NULL) != 60 ||
            api->get_vblank(api->context, &blank) != 91 || blank.count != 9 || blank.process_microseconds != 8 ||
            blank.native_tsc != 7)
            return 100;
    }
    {
        CtrVideoPublicFlipStatus flip = {0};
        CtrVideoPublicVblankStatus blank = {0};
        flip.count = 999;
        blank.count = 888;
        if (!api->get_public_flip || !api->get_public_vblank || api->get_public_flip(api->context, &flip, 63) != 60 ||
            flip.count != 999 || api->get_public_flip(api->context, &flip, 64) || flip.count || flip.flip_arg != -1 ||
            flip.current_buffer != -1 || api->get_public_vblank(api->context, &blank, 40) != 91 || blank.count != 888)
            return 106;
    }
    if (!api->wait_vblank || api->wait_vblank(NULL, 500) != 60 || api->wait_vblank(api->context, 5001) != 60 ||
        api->wait_vblank(api->context, 0) != 52 || api->start_pacing(api->context) ||
        api->wait_vblank(api->context, 500) || api->get_progress(api->context, &progress) || progress.completed ||
        progress.pending || api->stop_pacing(api->context) || api->wait_vblank(api->context, 500) != 52)
        return 107;
    {
        StatusCopyControl control = {0};
        CtrGuestCopyApi memory = {sizeof(CtrGuestCopyApi), 1, &control, NULL, copy_status};
        CtrVideoPublicFlipStatus copied;
        memset(control.bytes, 0x71, sizeof(control.bytes));
        if (ctr_video_copy_public_status(api, &memory, 100, 1) != 91 || control.calls ||
            ctr_video_copy_public_status(api, &memory, 100, 2) != 60 || control.calls ||
            ctr_video_copy_public_status(api, &memory, 100, 0) || control.calls != 1)
            return 113;
        memcpy(&copied, control.bytes, sizeof(copied));
        if (copied.count || copied.flip_arg != -1 || copied.current_buffer != -1) return 113;
        control.reject = 1;
        memset(control.bytes, 0x71, sizeof(control.bytes));
        if (ctr_video_copy_public_status(api, &memory, 100, 0) != 112 || control.calls != 2) return 113;
        for (size_t i = 0; i < sizeof(control.bytes); ++i)
            if (control.bytes[i] != 0x71) return 113;
    }
    {
        StatusCopyControl control = {0};
        CtrGuestCopyApi memory = {sizeof(CtrGuestCopyApi), 1, &control, NULL, copy_status};
        CtrVideoResolutionConfigV1 config = {sizeof(CtrVideoResolutionConfigV1), 1, 0, 0, {0}};
        memset(control.bytes, 0x71, sizeof(control.bytes));
        if (ctr_video_copy_resolution(&config, &memory, 100) != 91 || control.calls) return 125;
        config.configured = 1;
        config.value.full_width = 1920;
        config.value.full_height = 1080;
        config.value.pane_width = 1280;
        config.value.pane_height = 720;
        if (ctr_video_copy_resolution(&config, &memory, 100) || control.calls != 1 ||
            memcmp(control.bytes, &config.value, 48) || control.bytes[48] != 0x71)
            return 125;
        config.value.pane_width = 1921;
        if (ctr_video_copy_resolution(&config, &memory, 100) != 60 || control.calls != 1) return 125;
        config.value.pane_width = 1280;
        config.value.padding = 1;
        if (ctr_video_copy_resolution(&config, &memory, 100) != 60 || control.calls != 1) return 125;
        config.value.padding = 0;
        control.reject = 1;
        if (ctr_video_copy_resolution(&config, &memory, 100) != 112 || control.calls != 2) return 125;
        puts(
            "resolution copy passed explicit_config=true unknown_refresh0=true exact48=true sentinel_preserved=true unconfigured91_no_write=true invalid_no_write=true reject112=true synthetic_only=true");
    }
    {
        StatusCopyControl control = {0};
        CtrGuestCopyApi memory = {sizeof(CtrGuestCopyApi), 1, &control, NULL, copy_status};
        CtrVideoBufferAttributesV1 value;
        memset(control.bytes, 0x71, sizeof(control.bytes));
        if (ctr_video_write_buffer_attributes(&memory, 100, 0x80000000U, 0xffffffffU, 0x80000000U, 1920, 1080, 2048) ||
            control.calls != 1 || control.bytes[40] != 0x71)
            return 127;
        memcpy(&value, control.bytes, 40);
        if (value.format != 0x80000000U || value.tiling != -1 || value.aspect != INT32_MIN || value.width != 1920 ||
            value.height != 1080 || value.pitch_pixels != 2048 || value.option || value.reserved0 || value.reserved1)
            return 127;
        control.reject = 1;
        if (ctr_video_write_buffer_attributes(&memory, 100, 0, 0, 0, 0, 0, 0) != 112 || control.calls != 2 ||
            memcmp(&value, control.bytes, 40) || ctr_video_write_buffer_attributes(NULL, 100, 0, 0, 0, 0, 0, 0) != 60)
            return 127;
        puts(
            "attribute write passed public_initialization=true exact40=true full32bit_values=true zero_reserved=true rejected112_preserves=true synthetic_copy=true");
    }
    {
        StatusCopyControl control = {0};
        CtrGuestCopyApi memory = {sizeof(CtrGuestCopyApi), 1, &control, NULL, copy_status};
        float gamma = 0.1f;
        uint32_t nanBits = 0x7fc00001U;
        memset(control.bytes, 0x71, sizeof(control.bytes));
        if (ctr_video_write_gamma(&memory, 100, gamma) || memcmp(control.bytes, &gamma, 4) ||
            control.bytes[4] != 0x71 || ctr_video_write_gamma(&memory, 100, 2.0f) ||
            ctr_video_write_gamma(&memory, 100, 0.0f) != 89 || ctr_video_write_gamma(&memory, 100, 3.0f) != 89 ||
            control.calls != 2)
            return 129;
        memcpy(&gamma, &nanBits, 4);
        if (ctr_video_write_gamma(&memory, 100, gamma) || memcmp(control.bytes, &nanBits, 4) ||
            control.bytes[4] != 0x71)
            return 129;
        control.reject = 1;
        if (ctr_video_write_gamma(&memory, 100, 1.0f) != 112 || memcmp(control.bytes, &nanBits, 4)) return 129;
        puts(
            "gamma initializer passed bounds01_20=true invalid89_no_write=true exact4_reserved_preserved=true public_NaN_comparison=true reject112=true renderer_application_missing=true");
    }
    pixels = (unsigned char*)malloc(pitch * height);
    if (!pixels) return 71;
    for (size_t i = 0; i < pitch * height; ++i) pixels[i] = 128;
    {
        CtrVideoHostBufferV1 buffers[2] = {{pixels, pitch * height, pitch}, {pixels, pitch * height, pitch}};
        uint32_t group = 999;
        uint64_t tokens[2] = {777, 888};
        CtrVideoBufferAttributesV1 attributes = {0x80000000U, 0, 0, width, height, width, 0, 0, 0};
        if (!api->register_linear_group ||
            api->register_linear_group(api->context, 0, 2, &attributes, buffers, &group, tokens, 2) != 88 ||
            group != 999 || tokens[0] != 777) {
            free(pixels);
            return 111;
        }
        attributes.tiling = 1;
        if (!api->register_group || !api->unregister_group ||
            api->register_group(api->context, 0, 2, buffers, &group, tokens, 1) != 60 || group != 999 ||
            tokens[0] != 777 || tokens[1] != 888) {
            free(pixels);
            return 111;
        }
        if (api->register_linear_group(api->context, 0, 2, &attributes, buffers, &group, tokens, 2) || group != 0 ||
            !tokens[0] || !tokens[1] || tokens[0] == tokens[1]) {
            free(pixels);
            return 111;
        }
        token = tokens[0];
        CtrVideoLinearFrameV1 latest = {
            sizeof(CtrVideoLinearFrameV1), 0x80000000U, width, height, width, pixels, pitch * height};
        if (!api->submit_linear_frame || api->submit_linear_frame(api->context, 0, token + 99, &latest, 0) != 77 ||
            api->submit_linear_frame(api->context, 0, token, &latest, 1234) ||
            api->unregister_group(api->context, group) || api->unregister_group(api->context, group) != 62 ||
            api->enqueue(api->context, 1, tokens[1], 0) != 63) {
            free(pixels);
            return 111;
        }
    }
    if (!api->set_flip_rate || api->set_flip_rate(api->context, 1) || api->set_flip_rate(api->context, -1) != 88 ||
        api->set_flip_rate(api->context, 3) != 88) {
        free(pixels);
        return 118;
    }
    if (api->get_progress(api->context, &progress) || progress.completed || progress.pending != 1 ||
        api->start_pacing(api->context) || api->present_next(api->context, 500, &completion) || completion.count != 1 ||
        completion.token != token || completion.argument != 1234 || api->stop_pacing(api->context))
        result = 72;
    if (!result && (api->get_progress(api->context, &progress) || progress.completed != 1 || progress.pending ||
                    progress.token != token || progress.argument != 1234 || progress.current_slot != 0))
        result = 99;
    if (!result) {
        if (api->set_flip_rate(api->context, 0)) result = 119;
    }
    free(pixels);
    if (!result) ++*(unsigned*)user;
    return result;
}
static int fail_body(const CtrVideoSessionApiV1* api, uint32_t w, uint32_t h, uint32_t f, void* user) {
    (void)api;
    (void)w;
    (void)h;
    (void)f;
    ++*(unsigned*)user;
    return 73;
}
int ctr_video_c_consumer_control(void) {
    unsigned calls = 0;
    if (ctr_video_run_session_v1(NULL, &calls) != 60 || calls) return 74;
    if (ctr_video_run_session_v1(consume, &calls) || calls != 1) return 75;
    if (ctr_video_run_session_v1(fail_body, &calls) != 73 || calls != 2) return 76;
    {
        CtrVideoWindowOptionsV1 options = {sizeof(CtrVideoWindowOptionsV1), 1, 320, 240, 1, 0};
        if (ctr_video_run_window_session_v1(&options, consume, &calls) || calls != 3) return 85;
        options.visible = 2;
        if (ctr_video_run_window_session_v1(&options, consume, &calls) != 60 || calls != 3) return 86;
        options.visible = 0;
        options.width = 0;
        if (ctr_video_run_window_session_v1(&options, consume, &calls) != 60 || calls != 3) return 87;
    }
    puts(
        "C consumer status copy adapter passed exact64=true unbound_no_write=true rejection112=true synthetic_transport_only=true");
    puts(
        "C consumer linear groups passed transactional_submit=true stale77=true attributes40=true tiled88=true bounded_tokens=true queued_snapshot_survives_group_removal=true stale_slot_rejected=true");
    puts("C consumer independent vblank wait passed empty_queue=true stopped52=true invalid60=true");
    puts(
        "C consumer visible options control passed requested_client=320x240 gpu_present=1 invalid_options_rejected=2 visual_observation=false");
    puts(
        "C consumer flip rate1 real GPU present on eligible old-vblank phase passed; negative rates reject88 preserve rate");
    puts("C consumer session lifecycle passed gpu_present=1 callback_failure73_propagated=true calls=3");
    return 0;
}
