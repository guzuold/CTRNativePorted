# Local changes to engine/fsr-vulkan

engine/fsr-vulkan is FireBurn/FSR-Vulkan at `c64f093` (MIT), the latest upstream commit.
`ctrport/gpu/build.ps1` applies these patches to its working tree when they are not applied
yet. They come from GetLostIn/CTR-NitroFueled-PC (`gpu/patches/fsr-vulkan`, revision
0cf8def7f676192de4f37a26e08a92ea0cd4674a, GPL-2.0-or-later), unchanged:

- `0003`: the 3.1.6 frame-generation wrapper tags the bridge frame on Prepare and retires its
  transient descriptor sets and constant buffers in RetireFrame. Without it every dispatch
  leaks them until the device runs out of memory.
- `0004`: the 3.1.5 bridge (used by 3.1.6 frame generation) stages its per-pass constant
  buffers in persistent 256 KiB mapped slabs instead of one allocation each.
- `0005`: the FFX 1.1.4 Vulkan backend (the FSR 3 upscaler) falls back to plain host memory
  for its constant buffers when the device-local host-visible heap (no Resizable BAR) is full.
- `0006`: `PrepareInfo.colorHudless`, an optional HUD-less copy of the frame: optical flow
  and interpolation run on it and the HUD is taken from the presented frame.

SHA256:

| Patch | SHA256 |
| --- | --- |
| 0003-framegen-retire-bridge-transients.patch | efb51f6f5041f95e75af3f12e90eb88e4a2b0145ae4c689c02b1c6fdfadb6526 |
| 0004-bridge-constant-buffer-slabs.patch | e6d0cf810d8630a270257861abe449bce643803f6fd64d3caf3549cf91e9c6cb |
| 0005-ffx_vk-uniform-buffer-host-fallback.patch | dfb6f320019661a35008d2621839cac5393d4d728cbdf63c04cccce476a686f3 |
| 0006-framegen-hudless-source.patch | 821e58b1b3aba668fc620a2622f90f5c35b94873e1934e0123bff5a5596338f1 |
