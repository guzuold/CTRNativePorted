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

## Engine addresses (per eboot SHA256, `engine.h`)

| | 1.00 `ca9c9441…` | 1.21 `39d716b7…` |
|---|---|---|
| device global | 0x26e7920 | 0x2aebce0 |
| `igPS4GraphicsDeviceContext::clear` | 0x1306ba0 | 0x13c9e40 |
| `igPS4GraphicsDeviceContext::copyTexture` | 0x1307f20 | 0x13cb1c0 |

- **Device globals:** found by the engine-state probe ((development notes)).
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
- **Not available on this backend:** PlayStation controllers through SDL, the FSR/DLSS presenter and
  the gamma adjustment; supersampling is available with `CTR_D3D_RenderScale`.
