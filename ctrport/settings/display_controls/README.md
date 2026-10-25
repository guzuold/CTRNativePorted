# Display and temporal controls

Public v0.5.9 and shadPS4 consumers are adapted only in a generated private GPU
source tree. Call `prepare_gpu.prepare(gpu)` after renderer_controls preparation.
No original retail instructions, game assets or shared engine files
are modified. New code GPL-2.0-only; generated upstream code retains its upstream
GPL-2.0-or-later notices. ST2084 constants are mathematical values, not copied SDK
implementation. Existing AMD/NVIDIA bridges retain their original licenses.

## Runtime interface

| Key | Values | Consumer |
|---|---|---|
| hdr_output | original/off/native | native VideoOut negotiation, title PSF bit and actual PQ 10-bit surface availability |
| display_gamma | original/0.8/0.9/1.1/1.2 | SDR PostProcessingPass gamma push constant |
| hdr_peak_nits | original/400/600/1000/1500 | native PQ decoded to absolute nits, component peak clipping, re-encoded PQ |
| hdr_exposure | original/0.75/1.25/1.5 | native PQ absolute luminance multiplier before peak clipping |

Original preserves the previous output exactly: SDR gamma remains game-controlled,
HDR is disabled as in the release; original peak/exposure preserve native PQ bits
when native HDR is selected. Peak is a clipping calibration, not a new artistic
HDR tone mapper or an assertion of monitor capability. HDR calibration applies only
to native HDR frames. GPU-side parsing also rejects native HDR paired with any non-Off CTR_UPSCALER,
including direct DLL clients and inherited process variables. The title's PSF attributes remain untouched and are forwarded
from the runtime. No SDR-to-HDR conversion or invented title HDR support is added.

`policy.hpp::environment(doc,extended)` validates and returns four process variables
before GPU DLL load; empty means remove stale overrides. Verify the actual export
`ctr_gpu_display_abi()==1`. Runtime and catalog must reject native HDR with any scene
upscaler other than Off: the existing scene temporal/FSR1 inputs are SDR contracts.
HDR metadata and monitor calibration are not inferred from a selection. Native HDR
is advertised to the title only for the correct surface format and title PSF flag;
an unsupported explicit PQ request returns the real invalid-value error. The GPU
logs requested/surface-format/available at swapchain construction. A user's HDR
selection is permission for negotiation, not proof that HDR became active.

`ctr_gpu_temporal_capabilities()` is an atomic mask: bit0 means a real FSR3 context
was successfully created for actual scene dimensions, bit1 means NVIDIA NGX device
initialization succeeded. Zero at DLL load or before scene input is expected.
Failure/destruction clears the respective bit. This is separate from output use:
`ctr_gpu_temporal_state()` is 0 unobserved,1 successful temporal output,2 actual
FSR1 fallback,3 neither path produced a suitable scene. State changes are logged.
FSR3/DLSS selection may request initialization, but the menu must not present backend
presence or ABI as hardware readiness. Before first valid scene no FSR3 availability
claim is possible. Actual upscaling failure falls through the existing FSR1 pass,
with its original dimensions/aspect checks, synchronization and source layout
transition. Fallback restores the output layout to shader-read (temporal paths use
General). Unsupported dimensions retain original rendering. No frame-generation
support is claimed or changed; the existing backend rejects 10-bit HDR input.

## Evidence and limits

`check.ps1` generates isolated sources; compiles/executes actual generated videoout
functions against synthetic capability combinations, exact PSF forwarding and actual
UpscaleScene against call-recording Vulkan/resource fixtures. Color tests compile
the scalar functions extracted from the generated fragment shader and compare
independent ST2084 checkpoints plus bounded sweeps and exact Original rollback.
The actual full fragment shader compiles to SPIR-V through the installed upstream
glslang libraries; a deliberately invalid shader must fail. These controls do not
start the game or create a display surface. Full GPU DLL integration/build and real
monitor/scene verification are separate parent/user checks. No visual quality,
performance, successful HDR presentation or NVIDIA/AMD execution is claimed by
CPU fixtures. No new reconstructed retail functions are counted.

Resources: calibration extends push constants from8 to16bytes within Vulkan's
minimum supported range, adds no frame allocations, preserves alpha, and keeps
native pass creation/synchronization. Startup environment is read once by the
presentation pass. Capability/state atomics avoid races with runtime readers.
Rollback is selecting Original/Off then restarting, with the same pinned sources.

Primary technical references checked 2026-10-24:
- [Khronos color-space contract](https://docs.vulkan.org/refpages/latest/refpages/source/VkColorSpaceKHR.html): HDR10 surface expects BT2020 components encoded with PQ; alpha remains linear.
- [Khronos Data Format, section13.6](https://registry.khronos.org/DataFormat/specs/1.3/dataformat.1.3.html): PQ EOTF/inverse constants and absolute luminance units used by the calibration shader.
