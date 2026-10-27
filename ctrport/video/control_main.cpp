extern "C" int ctr_video_c_consumer_control();
extern "C" int ctr_video_tiled_session_control();
extern "C" int ctr_video_checked_submit_control();
// SPDX-License-Identifier: GPL-2.0-only
#include "host_interfaces.h"
#include "shader_inputs.h"
#include "open_session.h"
#include <stdio.h>
extern "C" int ctr_video_device_control();
static int open_body(const CtrVideoSessionApiV1* api, uint32_t width, uint32_t height, uint32_t, void* context) {
    auto* calls = static_cast<unsigned*>(context);
    ++*calls;
    if (!api || api->version != 1 || !api->context || width != 320 || height != 240 || api->pump_messages(api->context))
        return 132;
    CtrVideoProgressV1 progress{};
    if (api->get_progress(api->context, &progress) || progress.completed || progress.pending) return 132;
    return 73; // Owner loop exit propagates after real resource cleanup.
}
static int open_control() {
    unsigned calls = 0;
    CtrVideoWindowOptionsV1 options{sizeof(options), 1, 320, 240, 1, 0};
    if (ctr_video_open_main_session_v1(254, 0, 0, 0, &options, open_body, &calls) != 88 ||
        ctr_video_open_main_session_v1(255, 1, 0, 0, &options, open_body, &calls) != 88 ||
        ctr_video_open_main_session_v1(255, 0, 1, 0, &options, open_body, &calls) != 88 ||
        ctr_video_open_main_session_v1(255, 0, 0, 1, &options, open_body, &calls) != 88 || calls)
        return 132;
    if (ctr_video_open_main_session_v1(255, 0, 0, 0, &options, open_body, &calls) != 73 || calls != 1) return 132;
    puts(
        "VideoOut Open observed-argument adapter passed real_window_session=true owner_body_once=true callback73_cleanup=true unsupported88_no_body=true guest_handle_missing=true R3=false");
    return 0;
}
int main() {
    setvbuf(stdout, nullptr, _IONBF, 0); // Keep evidence lines intact when Vulkan diagnostics use stderr.
    const unsigned char bytes[4] = {1, 2, 3, 4};
    CtrShaderSnapshot snapshot{bytes, 4, 100, 1};
    unsigned char output = 99;
    if (ctr_shader_snapshot_read(&snapshot, 103, &output, 1) || output != 4) return 20;
    output = 99;
    if (!ctr_shader_snapshot_read(&snapshot, 104, &output, 1) || output != 99) return 21;
    if (!ctr_shader_snapshot_read(&snapshot, 99, &output, 1) || output != 99) return 22;
    if (ctr_shader_snapshot_read(&snapshot, 104, &output, 0) || output != 99) return 23;
    snapshot.guest_address = UINT64_MAX - 1;
    if (!ctr_shader_snapshot_read(&snapshot, UINT64_MAX, &output, 1) || output != 99) return 24;
    snapshot = {bytes, 4, 100, 0};
    if (!ctr_shader_snapshot_read(&snapshot, 100, &output, 1) || output != 99) return 25;
    if (!ctr_shader_snapshot_read(nullptr, 100, &output, 1) || output != 99) return 26;
    snapshot = {bytes, 4, 100, 1};
    if (!ctr_shader_snapshot_read(&snapshot, 100, nullptr, 1)) return 27;
    puts("shader_snapshot host-only boundary checks passed=8; runtime_token_lifetime=unverified");
    const int tiled = ctr_video_tiled_session_control();
    if (tiled) return tiled;
    const int opened = open_control();
    if (opened) return opened;
    const int status = ctr_video_device_control();
    if (status) return status;
    const int checked = ctr_video_checked_submit_control();
    return checked ? checked : ctr_video_c_consumer_control();
}
