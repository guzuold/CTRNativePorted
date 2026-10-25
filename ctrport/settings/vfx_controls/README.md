# VFX quality override

`vfx_quality`: Original / Low / High. Startup-only retail1.21 override, SHA-pinned.
The nine-byte quality load in the original scene update is replaced with0.0 or1.0;
all downstream original instructions still publish the quality, enable flag,
reference count and render-bundle queue entry. Scene-owned parameters, particle
spawns, lifetimes and simulation are untouched. Original restores the exact nine
bytes. No reconstruction-function credit is claimed.

Low selects the original low-resolution effect composition without edge refinement;
High enables the existing edge refinement path. These are quality modes, not
particle-density reductions or promises of measured speed. The existing unmasked
UpsampleVfx shader remains present in Low; particles are not disabled by replacing
or skipping their rendering code. Runtime rendering remains original apart from
this single quality load. GPU visual validation is still user-owned.

## Original evidence and scope

- Producer function0xaf0330, site0xaf056e. Its bounded block through0xaf05f0 loads
  scene+0x4f4, compares the absolute quality to original epsilon, publishes the
  refinement enable at parent+0x268 ->+0xc and the selected bundle quality+0x18 /
  edge flag+0x1c. It increments the original bundle refcount and queues the bundle.
- Native controls execute this original block, stopping before the unrelated DoF
  call at0xaf05f0. A test-only RET marks that exact boundary; it is never installed
  in the runtime. Preallocated list path only; list growth/allocator path is outside
  this fixture. The test transport supplies original live r12/r15/rbp-local state.
- Full original publisher0x967ba0 executes against synthetic context/device/cache
  structures. Its two existing bindings0x2a62010 /0x2a62018 receive the quality and
  edge-enable values, with original dirty sequencing/list linkage. Preallocated
  constant caches only; allocation itself is not simulated or returned as success.
- CPU comparisons run the unmodified original with0.0/1.0 inputs as reference,
  then the patched original with varied inputs and finally restored originals.
  No duplicated C++ quality formula serves as the oracle. Inputs include zero,
  epsilon vicinity, fractional, positive, negative and NaN; two frame indices;
  linked/unlinked caches and all four missing-binding combinations.
- EdgeDetect original asset exposes exactly one pixel constant, `vfx_quality`,
  scalar float at constant-buffer offset0. Its native input-usage slot maps buffer0
  to s12..15; original S_BUFFER_LOAD_DWORD loads float0 into s14, consumed by the
  edge multiplication/threshold. This closes the formerly unresolved s14 binding.
- UpsampleVfx original igGraphicsShaderVariant declares exactly one boolean name,
  `vfx_edge_detect_enabled`, element0, domain size2. Its shader list maps0 to
  Default[00] (unmasked original upsample),1 to Default[10] (edge-mask refinement).
  Strict original-asset checks reject modified constant offsets, load offsets and
  variant references. Assets are read from the user's original archive, SHA-pinned,
  never written to public code or packaged.

Implementation reuses only the project's own patch conventions and original retail
observations. Public input-usage semantics checked
against [opengnm shader documentation](https://ps4-opengnm.github.io/opengnm/guides/shaders/),
and scalar-load decoding against local public shadPS4 decode.cpp. Research-only
IGZ schema cross-check used the existing read-only parent IGZ reader; executable
checks use the existing local relocation-table parser and no new parent dependency.
New source GPL-2.0-only. Original fixture spans/hash evidence remain ignored.

`tools/settings/vfx_controls/check.ps1` compiles and executes the CPU controls and
runs the original asset consumer audit. No game, window, GPU execution or screenshot
is produced. Full-function scheduling, list growth, actual graphics and quality
appearance remain outside these bounded CPU controls; parent integration verifies
patch installation and rollback in the full mapped retail image.
