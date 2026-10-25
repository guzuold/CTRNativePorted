# Native texture residency and racer mesh controls

These controls execute the user's original retail 1.21 code; no new engine
function is reconstructed. Both controls are selected before guest workers start,
apply only to the pinned SHA256, and require a restart. The integration owner
provides writable bootstrap code and flushes static patch instruction caches.
All hooks must be destroyed only after guest workers are quiescent.

`mesh_override.hpp`: `verify_mesh`, `apply_mesh_highest`, `rollback_mesh`.
The six bytes at `0xeadfce` select the original authored highest-detail branch in
`0xeadf00`. The native caller `0xeadbd0` retains its multiple-camera aggregation
and sends the chosen value to `0xe21110`, which uses the authored mesh list and
original resource-switch scheduler. This affects racer/kart meshes, not arbitrary
world geometry. No fabricated LOD index or mesh-hide option is used.

`texture_hook.hpp/.cpp`: `install_texture(base, extent, sha, TextureMips)` returns
an owning `unique_ptr<TextureHook>`; `original` returns null, `half` and `quarter`
request a minimum of one/two dropped top mip levels. `uninstall()` restores the
original bytes. The register-only fragment replaces the instruction group at
`0x141be9c`, shared by both original image-size branches. It preserves every
register except original RAX/flags outputs and introduces no shared runtime data,
allocator, callback, new thread or mutation of file parameters. The original
post-load consumer `0x141be40` clamps the count and runs `0x141ccb0`, which really
reduces payload storage and dimensions before graphics resource creation, then
updates the directory's owning memory descriptor. This is not a sampler bias.

Only ordinary 2D images at least 256 by 256 receive the extra minimum. Smaller
images, cubemaps/arrays, volume textures, negative file sentinels and null
directory/file-parameter paths retain authored behavior. Single-mip images keep
their sole level through the original clamp. Larger authored mip drops are never
reduced. A mipmapped UI image meeting the size criteria is also affected: this is
a texture-quality policy, not a promise that every UI asset is classified. There
is no arbitrary VRAM budget, eviction scheduler, or live resource replacement.
Returning to original reloads the dump's unmodified image data at next startup.

Run `tools/settings/quality_controls/check.ps1` then `audit.py`. Native CPU checks
execute the full retail mesh calculator, position helper and direct handle path;
texture checks execute the full original post-load, mip drop, image configuration,
level enumeration, extraction, ownership reset and reference-count helpers. The
texture fixture explicitly supplies synthetic registry lookup, memory-pool
allocation/lookup, platform-format size/validity, and memcpy boundaries. Two
synthetic layouts represent RGBA and 4x4 blocks; no test claims validation of all
PS4 tiled formats, device uploads, GPU allocation, pixels or visual quality.

The texture oracle compares hooked calls with separate unmodified original calls
whose authored mip request already equals the requested minimum. It also checks
the independent exact payload suffix, dimensions, storage size, directory
ownership, source/destination bounds, allocation/free balance, larger requests,
exclusions, negative sentinels, duplicate/identity/signature/protection rejection,
byte restoration and behavior after rollback. Mesh tests compare the override
with original forced-high calls, restore distance-driven selection, and require
positive cases where the two original modes differ. Focused-player camera lookup,
parented transform callbacks and actual resource scheduling are not synthesized
into a claim of gameplay verification.

`world_hook.hpp/.cpp`: `install_world(base, extent, sha, WorldLod)` returns an
owning hook (`original`/`reduced`). The original encoder at `0x348350` receives
call-local copies only when an authored alternate cache exists, has positive
strictly lower primitive count, nonzero vertex/index resources, the identical
vertex-format handle and matching lower six topology/index-format bits. Morph
weights and baked-vertex offsets force original fallback. The alternate resource
publisher is original `0x3492e0`; it produces count/topology and three resource
handles. No missing alternative is generated, loaded or fabricated. Assets without
compatible alternate geometry retain original detail. This is a reduced-geometry
policy for compatible draws, not a mesh simplifier or a distance slider.

Only the count and vertex/index handles change. Original vertex format, shader
flags, bounds, culling, transforms, materials, lighting and instance references
remain. The original draw/data objects and their owning references remain alive
and unchanged through command emission; original command processing manages
resource use and submission as usual. No additional asset ownership or resource
allocation is introduced. The per-draw readability queries and small stack copies
have CPU cost; fewer emitted primitives do not by themselves prove higher FPS.

The original encoder reads only two integer arguments. Its draw/data accesses fit
within the copied 0x80/0x98 bytes. The only callees receiving the draw pointer are
morph encoding (excluded on selection), light selection (reads draw transforms and
data bounds, writes original lighting storage), and the checksum of the eight-byte
instance reference at draw+0x60. Other callees receive unchanged referenced
objects. Geometry commands contain copied numeric resource handles and count, not
stack pointers. The full source call-site audit is evidence for these lifetime
claims; tests do not pretend to exercise every material or lighting system.

The world CPU oracle executes the original alternate-cache publisher and original
command emitter, using explicitly synthetic precreated resources and independent
material/shader/animation/lighting boundaries. It compares hooked output with an
unmodified encoder supplied the native-published alternate fields, then separately
decodes VB/VF/IB handles and primitive count. It verifies all encoder return paths
(missing technique, absent stencil, reset false/true), incompatible/malformed
metadata fallback, byte/behavior rollback, eight independent concurrent draws,
and RBP/RBX/R12-R15 plus stack/canary preservation with an assembly SysV probe.
Unusable draw/data pointers pass through to the original, preserving its early
technique exit instead of faulting prematurely. There is no claim of GPU or visual
validation; incompatible assets deliberately keep their original representation.
