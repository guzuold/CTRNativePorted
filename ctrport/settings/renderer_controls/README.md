# Renderer controls

These adapters use the public v0.5.9 GPU runtime and shadPS4 Vulkan boundaries.
They do not patch game data or reconstruct retail functions. All changes apply
at process startup; the same renderer must be restarted to restore Original.

| Key | Values | Actual consumer |
|---|---|---|
| present_mode | original, fifo, mailbox, immediate | GPU shim GetPresentMode -> Swapchain::FindPresentMode; unsupported surface modes fall back to FIFO with renderer log |
| anisotropy | original, 2, 4, 8, 16 | settings GPU sampler constructor -> VkSamplerCreateInfo.maxAnisotropy, bounded by device maximum; only game-designated anisotropic samplers |
| fsr1_sharpen | original, off, low, medium, high | settings GPU scene FSR1 pass -> use_rcas/rcas_attenuation -> FsrRcasCon; no effect with scene FSR1 inactive |

Original clears our override. Scene RCAS Original is the previous 0.25-stop
attenuation; low/medium/high are 2/1/0 stops. Off bypasses RCAS. These labels
describe sharpening strength, not measured quality or performance presets.
We preserve nearest/non-anisotropic samplers and all minification/mipmap/LOD/
comparison/border/addressing properties. Anisotropy cannot increase texture
resolution or alter streaming budgets. Maximum 16 remains clamped to hardware.

## Parent integration

Include `renderer_controls/policy.hpp` from the store, and call
`renderer::validate(document)` inside the existing validator. Before loading the
GPU DLL, call `renderer::environment(document, extended_gpu)`, apply each returned
environment value (empty means remove), and log the requested settings. The old
published GPU permits present_mode; it must reject non-original anisotropy or
sharpness. extended_gpu is a package/build identity fact, never a user-supplied
environment flag. The new DLL exports `ctr_gpu_settings_abi()` returning 1.

`tools/settings/renderer_controls/prepare_gpu.py --gpu-source <isolated private GPU>`
derives modified sources from the pinned release. It must run after baseline
copying and before CMake configuration. Generated sampler.cpp replaces, rather
than duplicates, upstream's sampler translation unit. The source include path
finds renderer_controls/gpu_policy.hpp. No shared engine/source input is edited.

## Evidence boundaries and remaining categories

CPU controls compile the actual baseline emulator settings shim and verify four
requested present modes, malformed input rejection, missing extended capability,
100 sampler capability/selection combinations, sharpness boundaries and bad device
limits. Compiling a Vulkan DLL proves integration, not appearance or frame timing.
The user owns graphics inspection; no game is launched by these checks.

* HDR: baseline `gpu/shim/core/emulator_settings.h::IsHdrAllowed` returns false.
  The upstream swapchain uses Rec.2020 PQ, but exposing HDR also needs correct
  retail color encoding, compatible post-processing, display capability and
  frame-generation handling. No fake HDR switch is exposed.
* Texture resolution/streaming: sampler.cpp controls sampling, not resource
  residency. No verified retail streaming allocator/budget contract is supplied
  by this renderer; arbitrary mip clamps would misrepresent texture quality.
* Frame pacing: present-mode selection is implemented. `GetVblankFrequency` is
  also consumed by guest timing; changing it is not a safe arbitrary FPS cap.
  Existing verified 30/60 mode remains the supported simulation-rate control.
* FSR3/DLSS/frame generation: source code exists, but presence is not positive
  device/runtime acknowledgement. Retain the parent's capability gate and do
  not advertise availability from successful compilation alone.
* Color/gamma/HDR calibration and newer antialiasing: no actual validated
  display transform or temporal camera/depth/motion contract is added here.
* Presets: no GPU measurements were made; no performance ranking is claimed.

Provenance: public release commit 461610adefa1e400df59f08c2270cced9ff29209;
GPU VENDOR.md and upscaling/SOURCES.json retained; shadPS4 source and public
runtime adapters GPL-2.0-or-later, new project policy GPL-2.0-only. Reused FSR libraries are private local build
inputs whose hashes must be recorded with the resulting package.
