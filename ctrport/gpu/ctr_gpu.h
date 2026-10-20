/* SPDX-License-Identifier: GPL-2.0-or-later
 * C interface of ctr_gpu.dll: shadPS4 v0.18.0's Liverpool command processor, Vulkan
 * renderer, GnmDriver, VideoOut and kernel event queues, adapted to ctrport's loader.
 * Modelled on bbport's gpu/bbgpu.h. ctrport loads the DLL at run time and binds the
 * guest imports that the library provides; the functions use the guest SysV ABI. */
#ifndef CTR_GPU_H
#define CTR_GPU_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifdef CTR_GPU_BUILD
#define CTR_GPU_API __declspec(dllexport)
#else
#define CTR_GPU_API __declspec(dllimport)
#endif

typedef struct CtrGpuConfig {
    uint32_t version;        /* 1 */
    const char* title;       /* window title */
    const char* serial;      /* CUSA id, names the pipeline cache */
    const char* user_dir;    /* writable directory for caches and logs (runtime/user) */
    uint32_t sdk_version;    /* from the eboot's process parameters */
    uint32_t psf_attributes; /* param.sfo ATTRIBUTE */
    int32_t width, height;   /* initial window size */
} CtrGpuConfig;

/* Creates the window, Vulkan device, presenter and GPU command processor, registers the
 * kernel event queue, GnmDriver and VideoOut functions and installs the GPU page-tracking
 * fault handler. Returns 0 on success. */
CTR_GPU_API int ctr_gpu_init(const CtrGpuConfig* config);
/* GPU page-tracking faults: ctrport's exception handlers call this first with the
 * EXCEPTION_POINTERS of an access violation. Returns 1 when the fault was a tracked
 * guest page and execution can continue. */
CTR_GPU_API int ctr_gpu_handle_fault(void* exception_pointers);
/* Function for an imported NID in a library, or 0 when ctr_gpu does not provide it. */
CTR_GPU_API uintptr_t ctr_gpu_resolve(const char* nid, const char* library);
/* GPU-visible guest mappings (direct memory). Mappings reported before ctr_gpu_init are
 * kept and replayed when the rasterizer starts. */
CTR_GPU_API void ctr_gpu_map_memory(uint64_t address, uint64_t size);
CTR_GPU_API void ctr_gpu_unmap_memory(uint64_t address, uint64_t size);
/* The CPU wrote guest memory outside page tracking (e.g. a file read by the kernel). */
CTR_GPU_API void ctr_gpu_invalidate_memory(uint64_t address, uint64_t size);
/* Diagnostics. */
CTR_GPU_API unsigned ctr_gpu_symbol_count(void);
CTR_GPU_API uint64_t ctr_gpu_flip_count(void);
CTR_GPU_API uint64_t ctr_gpu_submit_count(void);
CTR_GPU_API int ctr_gpu_input_captured(void);
/* PlayStation controllers (DualShock 4, DualSense) read through SDL's HID drivers, as an
 * XINPUT_GAMEPAD-layout snapshot (buttons, triggers 0..255, sticks with Y up). Returns the
 * number of connected PlayStation controllers; their inputs are merged. */
typedef struct CtrGpuGamepad {
    uint16_t buttons;
    uint8_t left_trigger, right_trigger;
    int16_t left_x, left_y, right_x, right_y;
} CtrGpuGamepad;
CTR_GPU_API int ctr_gpu_gamepad_state(CtrGpuGamepad* out);
/* Rumble on every connected PlayStation controller (0 stops it). */
CTR_GPU_API void ctr_gpu_gamepad_rumble(uint16_t low_frequency, uint16_t high_frequency);
/* Each connected PlayStation controller on its own, for split screen: the SDL joystick id
 * (stable while connected) and its snapshot. Returns how many entries were written. */
typedef struct CtrGpuGamepadEntry {
    uint32_t id;
    CtrGpuGamepad state;
} CtrGpuGamepadEntry;
CTR_GPU_API int ctr_gpu_gamepad_list(CtrGpuGamepadEntry* out, int capacity);
/* Rumble and light bar of one PlayStation controller by id; unknown ids are ignored. */
CTR_GPU_API void ctr_gpu_gamepad_rumble_one(uint32_t id, uint16_t low_frequency, uint16_t high_frequency);
CTR_GPU_API void ctr_gpu_gamepad_light(uint32_t id, uint8_t red, uint8_t green, uint8_t blue);

#ifdef __cplusplus
}
#endif
#endif
