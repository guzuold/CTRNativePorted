# Cascade shadow quality

Two controls at original retail consumers:

- Resolution: Original,50%,75% of authored cascade viewport dimensions.
- Cascade limit: Original,maximum1,2,3. Always takes the smaller of the authored
  count, existing atlas capacity and selected limit; it never adds cascades.

`Options{Resolution::half,2}` and
`install(image,extent,retail_sha256,options)` return a `unique_ptr<Hook>`.
Keep it alive for execution. `uninstall()` requires quiescence, checks both
installed patches before any restoration and restores exact original bytes.
Original/0 is a no-op. Invalid enums/counts, wrong executable, changed signatures,
short images and unwritable startup images fail before mutation. The parent
must compile `hook.cpp`, install before original execution and retain the owner.

The configure trampoline replaces37 relocation-free bytes at0x357794 inside
original0x357540; the companion20-byte publisher trampoline starts0x357072
inside0x357020. Every original instruction is retained. Resolution reduces the
original computed pixel size at pass+0x1e8 while leaving atlas width+0x1f0 and
slot count+0x1f4 unchanged. The companion applies the identical multiplier to
shadow-map scale before the original sample matrices/constants are built.
Reducing viewport size without this companion would break shadow coordinates.

The guard computes `(atlas_width*authored_scale)*factor` on both sides. Values
below1 or nonfinite retain original scale and original size. Thus a tiny original
viewport cannot become zero because of this setting. The original conversion
still rounds to integer; existing fractional-texel rounding remains. Values
above the authored viewport are never introduced. Cascade cap modifies the
original minimum before its final store; original split generation, camera
enable/disable, samplers and render command loops use that resulting count.

Original viewport block0x356f88 centers the effective viewport within its atlas
slot. Original split code0x3557b0 and publisher0x357020 retain their allocations,
projection logic and reference ownership. No allocation/reallocation/free is
added, no rendering pass is skipped, and no atlas memory savings are claimed.
Quality changes require restart. The code pages and literals are immutable
while workers run; guard scratch YMM1 is saved/restored on each call's stack.
Only registers already produced by original blocks are changed. The cascade-cap
comparison is enclosed in PUSHFQ/POPFQ to preserve original flags exactly. Original flag
comparison at the publisher runs after the guard, preserving its subsequent
branch. No C++ callback, shared mutable object, runtime allocation or locking
occurs inside the detour. Installation/removal is not concurrent-safe and is
restricted to startup/quiescent teardown.

Validation runs real extracted original configure, sampler-scale and viewport
instruction blocks with synthetic object storage. Each patched result is
compared against the original block with equivalent authored settings, followed
by byte-exact restore. The matrix covers widths1..4096 (including odd widths),
authored scales0..1, capacities1/2/4, counts1..4 and all11 non-default combinations.
Negative/nonfinite/tiny scales, changed signatures, unknown versions, short
images, readonly memory, duplicate install and guarded rollback are covered.
Eight concurrent workers exercise immutable detours with independent objects.

Reproduce:
`powershell -NoProfile -ExecutionPolicy Bypass -File tools/settings/shadow_quality/check.ps1`.
Current evidence:65,971 assertions and33,536 original block calls. This is not
whole-pass execution, real GPU output, framerate measurement or visual acceptance.
The probe deliberately ends each extracted block before unrelated services.
Original/quality/Original visual review remains separate.

Retail provenance only: SHA256
39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5. Extracted input bytes remain ignored/private;
public code contains only required patch signatures, authored detours and tests.

Local/hero/projective shadows are a different subsystem; the two cascade controls
make no claim to resize their resources. The separate hero control below disables
hero contributions without changing allocation. Retail research found
HeroShadows/ScreenSpaceShadows shader families, fullscreen boolean publisher
0x950760 and local influence binding0x9790b0. These findings led to the paired native/shader hero contract below. General
projective/local-light shadow allocation remains outside this control.

Four passes completed: implemented paired original-consumer detours; reviewed
coordinate and count invariants; fixed guard edge cases and proved atomic
preflight, exact restore, readonly handling and concurrent calls; kept API and
scope explicit. Parent verifies integration separately.

## Hero shadows: paired Original/Off consumers

`hero_patch.hpp` exposes `verify(image,extent,sha,patched=false)`, `apply(...)`
and `rollback(...)`. Both native sites are preflighted before either changes.
This bootstrap-only patch covers the full context publisher0x950760 (load at
0x950781) AND its command-buffer serializer0x9504a0 (load at0x950503). Both
publish original `hero_shadow_fullscreen_state=false`, retaining all other
parameters, allocations/cache updates and command records. Neutral input and
patched active input are compared using the full original functions, with
preallocated synthetic caches/command buffers; allocation growth is not tested.

`hero_shader.hpp` exposes `hero_shadows::override_code(hash,span,bool off)` like
the AO adapter. It recognizes hash0x1d8043ad,161 words, SHA256
`db6b98beb4971c0af89b4eb70fa8a81bd0dfc1f41b42e32ee99b6b306ede6090`.
The original HeroShadows pixel suffix produces `1+fade*(visibility-1)`, then
packs the same result into all MRT0 channels. Off replaces that final8-byte
MAD with `V_MOV_B32 v0,1.0; S_NOP0`, preserving original packing, EXEC restoration,
export and all preceding side effects. Forcing the literal avoids0*NaN.
This matches the original finite neutral-fade path and deliberately defines
neutral output for exceptional computed values as well. It is a shadow
contribution override; no pass-cost reduction or resource release is claimed.

Both native and GPU components must be enabled together for the menu's
`hero_shadows=off`. Use `CTR_GPU_HERO_SHADOWS` at restart, require the supporting
GPU ABI, feed the owned replacement span to TranslateProgram, and prevent an
external SPIR-V override from bypassing this matched replacement. Guest shader
bytes remain unchanged; Original at next process start uses the exact original
shader and native instructions. The shader helper reuses the AO SHA256 routine.

`check_hero.ps1` verifies475 assertions,102 full original CPU function calls
and one original shader variant. It checks dirty/clean cache cases, true/false/
255 state, eight command-buffer alignments, unaffected serialized fields,
byte-exact rollback, shader-per-word corruption, positive non-neutral original
output and NaN/infinity neutrality. These are CPU and shader-semantic checks,
not GPU visual execution. Parent integration and user visual review are separate.

Hero source provenance: original retail HeroShadows.igz SHA256
`edba0438ddbc81c2483fccc7eccd7adf192b0e59279b289db980293af4fde1bc`;
patch1.21 has no shader archive replacement. Native binding names are present
in retail registration0x975b60. The final review explicitly
added the command serializer to prevent a route that could bypass the direct
context publisher, and replaced zero multiplication with a literal neutral value.
