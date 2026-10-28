# ctrport/d3d: engine-state D3D11 backend (no shadPS4)

`ctr_d3d.dll` exports the same C interface as `ctr_gpu.dll` (`ctrport/gpu/ctr_gpu.h`). It is selected
with `CTR_GPU_BACKEND=d3d11` (`play_gpu.py --d3d11`). It owns libSceGnmDriver, libSceVideoOut and the
libkernel event queues. It renders from the engine's own render state (`igPS4GraphicsDeviceContext`,
layout in `engine.h`) with shaders translated ahead of time from the user's dump.

## Provenance

- FrankHUN88453/CTR-NitroFueled-PC, revision `96b4aa5474b1d42b0d9f383349dc091ea9a2d385`, MIT
  (`LICENSE.FrankHUN88453`). Each file header states its changes.
  - Unchanged: `formats.h`, `formats.cpp`, `texture.h`.
  - Adapted: `texture.cpp`, `renderer.cpp`, `gfx.h`, `videoout.cpp`, `gnm.cpp`.
- Written for ctrport (GPL-2.0-or-later): `common.h`, `guest.*`, `engine.h`, `equeue.*`,
  `videoout.h`, `ctr_d3d.cpp`, `CMakeLists.txt`, `build.ps1`.
  - Behaviour of a few calls follows shadPS4's documented semantics: Gnm query return values,
    event data encoding, VideoOut mode structures. No shadPS4 code is compiled or linked.
- Shader translator: `shaders/gcn_decode.hpp`, `shaders/gcn_hlsl.hpp`, `shaders/shader_export.hpp` are C++
  ports (MIT) of FrankHUN88453's `tools/gcn.py`, `gcn2hlsl.py`, `igz.py`, `export_shaders.py`,
  `import_runtime_shaders.py`. Their output is byte-identical to the Python translator: same HLSL and
  same DXBC for all 3202 shaders it handles. They also read one more shader, the Fur GS of
  `update2.pak`, which the Python IGA reader could not read. `shaders/ctr_shaders.cpp` (GPL-2.0-or-later) is the tool.
  - ctrport change: a texture read only through cube-face coordinates (`v_cubeid` and the rest) is declared
    `TextureCube`, and the face coordinates are turned back into a direction; the renderer binds a cube view.
    The 6-slice array read before chose a far too small mip level and did not filter across face edges, which
    drew a seam on reflective surfaces such as the trophies (110 of 3203 shaders).
- FSR 1: `fsr/ffx_a.h`, `fsr/ffx_fsr1.h` (AMD FidelityFX, MIT), copied unchanged from shadPS4 v0.18.0
  `src/video_core/host_shaders/fsr` (SHA256 `94d8d663…`, `4dc2022e…`). `fsr/fsr1.hlsl` is the D3D11 form
  of shadPS4's `fsr.comp` (32-bit float path); CMake embeds the three into the DLL.
- Scene post-processing and ambient occlusion (`post.cpp`, `post/post.hlsl`, `post/gtao.hlsl`, GPL-2.0-or-later),
  embedded by CMake and compiled by the renderer at startup:
  - `smaa/SMAA.hlsl`, `smaa/AreaTex.h`, `smaa/SearchTex.h`: SMAA by Jimenez et al. (MIT, `smaa/LICENSE.SMAA`),
    iryoku/smaa revision `71c806a838bdd7d517df19192a20f0c61b3ca29d`, unchanged. Its pixel shader functions are
    called from compute shaders through its `SMAA_CUSTOM_SL` porting macros.
  - `cas/ffx_cas.h`, `cas/ffx_a.h`: AMD FidelityFX Contrast Adaptive Sharpening (MIT,
    `cas/LICENSE.AMD-FidelityFX-CAS`), GPUOpen-Effects/FidelityFX-CAS revision
    `9fabcc9a2c45f958aff55ddfda337e74ef894b7f`, unchanged (its `ffx_a.h` is an older version than FSR 1's, so it is
    kept apart).
  - `gtao/XeGTAO.h`, `gtao/XeGTAO.hlsli`: Intel XeGTAO (MIT, `gtao/LICENSE.XeGTAO`), GameTechDev/XeGTAO revision
    `a5b1686c7ea37788eeb3576b5be47f7c03db532c`. Changed for this renderer: `XE_GTAO_LINEAR_DEPTH_INPUT` (the source
    is the game's linear depth, scaled by `DepthUnpackConsts.x`), and integer/float literal swizzles such as `2.xx`
    written as `((uint)(2)).xx` so fxc (Shader Model 5) compiles them. The Hilbert noise follows Intel's sample
    `vaGTAO.hlsl`.
- `pads.cpp` (GPL-2.0-only): PlayStation controllers through SDL 3 (zlib licence, linked statically from
  the GPU library's pinned `engine/shadPS4/externals/sdl3`), the same reading and exports as
  `ctrport/gpu/shim/controller_exit.cpp`, polled on a thread of its own.
- `shaders/variants.cpp` (GPL-2.0-only) reuses the GPU library's recognised-shader contracts
  (`ctrport/gpu/ao_controls`, `ctrport/gpu/shadow_quality`).

## PC settings

| Setting | Direct3D 11 |
|---|---|
| present mode, presentation limit, window mode, exclusive refresh, anisotropy, display gamma | as before (first settings group) |
| ambient occlusion off, hero shadows off | `ctr_shaders` translates the patched versions of the four AO shaders and the HeroShadows pixel shader next to the originals (index records `P <setting> <name>`); the renderer loads them at restart. Hero shadows also keep the runtime's CPU patch. |
| native HDR, HDR peak, HDR exposure | HDR10 is offered to the game (capability BT.2020 PQ) when the setting asks for it and the window's display uses the HDR10 color space; the swap chain then switches to R10G10B10A2 with `DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020`, and the present shader applies peak / exposure as `color_math.inc` does |
| scene upscaling FSR 1, FSR 1 sharpness | the finished scene (color target before the first draw into a flip buffer, after a four-target G-buffer pass) is upscaled with EASU + RCAS to the display target and sampled by the display pass in its place, before the HUD; sharpness is read every frame (live) |
| scene upscaling FSR 3, DLSS | not available: logged, the game's own scaling shows the scene; the launcher asks for Off or FSR 1 |
| anti-aliasing SMAA / SMAA Ultra / Off (`CTR_D3D_PostAA` 1 / 2 / 3) | the game's FXAA pass draws a plain copy instead (it must still draw: skipping it leaves the scene black), and with SMAA, SMAA 1x runs on the finished scene before the HUD (`post.cpp`) |
| saturation, contrast, vibrance, colour temperature (`CTR_D3D_Saturation`, `_Contrast`, `_Vibrance`, `_Warmth`) | one compute pass on the finished scene before the HUD |
| sharpness without an upscaler (`CTR_D3D_Sharpen`, live from the game menu) | AMD CAS on the finished scene before the HUD; with FSR 1 its RCAS sharpens instead |
| ambient occlusion XeGTAO / XeGTAO Ultra (`CTR_D3D_AO` 1 / 2) | after the game's AO pass, XeGTAO (3 or 9 slices, one denoise pass) on the game's full-resolution linear depth with the camera of the AO pass's constants, averaged to the pass size and written into its AO channel; the game's blur and lighting use it. `CTR_D3D_AODebug` 1–3 shows the term before denoising, the depth XeGTAO reads, the game's depth |
| supersampling 150% / 200% | the upscaling mode sets the game's scene height above the output (at most 2160); the presentation filters it down |

## Engine addresses (per eboot SHA256, `engine.h`)

| | 1.00 `ca9c9441…` | 1.21 `39d716b7…` |
|---|---|---|
| device global | 0x26e7920 | 0x2aebce0 |
| `igPS4GraphicsDeviceContext::clear` | 0x1306ba0 | 0x13c9e40 |
| `igPS4GraphicsDeviceContext::copyTexture` | 0x1307f20 | 0x13cb1c0 |

- **Device globals:** found by an engine-state probe on live objects.
- **clear / copyTexture:** the callees at the same offsets of `igWorkflowGpuPicking::getPixel` (+0x109)
  and of the command decoder in both versions.
- **Hooks:** placed only when the first 16 code bytes match the recorded prologue.

## Build and data

- `powershell -File ctrport/d3d/build.ps1`
  builds `build/CTRPort-d3d/ctr_d3d.dll`, `ctr_shaders.exe` and `ctr_hle.dll`.
- `ctr_shaders.exe --base <CUSA14876> [--patch <patch>] [--user runtime/user/gpu_d3d]` builds
  `runtime/user/gpu_d3d/shaders.bin` from the dump, with no Python. It includes the eboot-embedded
  shaders that the backend saved into `missing_shaders` when it met them. Compiled bytecode is cached
  in `dxbc_cache`: the first build takes about 6 minutes on 20 threads (fxc), a rebuild about 10 seconds.
  The launcher runs it when Direct3D 11 is selected and `shaders.bin` is missing, older than
  `ctr_shaders.exe`, or older than a newly dumped shader. `--hlsl <dir>` writes the translated sources.
- Writable data stays in `runtime/user/gpu_d3d`; nothing of the game is in the repository.

## Not implemented (reported, never silent)

- **Compute dispatches:** logged once per program, and the program is saved as
  `missing_shaders/Cs_*`. In 1.21 there are six `sceGnmDispatchDirect` call sites; one is inside
  `copyTexture`, which is replaced. None ran in the title, menus, adventure hub or a Crash Cove race.
- **Indirect, multi-instanced and opaque draws, `DispatchIndirect`, compute queues:** imported but
  with no call site in either eboot. A call is counted and logged with its caller.
- **`sceGnmRequestFlipAndSubmitDone*`:** imported with no call site and an unknown argument layout.
  A call stops the process.
- **2D macro-tiled textures:** only met as render targets sampled before their first draw. They read
  as transparent black.
- **Not available on this backend:** the temporal upscalers
  (FSR 3, DLSS); supersampling is available with `CTR_D3D_RenderScale`.
