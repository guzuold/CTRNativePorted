# Retail graphics controls

Bootstrap-only overrides for retail 1.21, bound to the executable SHA256 in
`overrides.hpp`. These five controls expose **Original / Off**, requiring a new
process. Original applies no patch. This is native execution of the user's
original code with narrow modifications, not reconstructed engine functions.

| Key | Retail producer/publisher | Off behavior |
|---|---|---|
| `screen_space_reflections` | `0x36d430` | Publishes false to `ig_screen_space_reflections_enabled`; preserves both parameter vectors. |
| `bloom` | `0x963ce0` | Zeros only the published bloom-strength lane, preserving luminance scale and bias. |
| `motion_blur` | `0xaf23d0` | Always follows the original camera-cut zero-exposure path; preserves matrices, history, reference accounting and bundle rotation. |
| `vignette` | `0xaf1f90` | Uses zero strength when producing vignette constants; preserves other color controls and scene-owned input data. |
| `depth_of_field` | `0xaf8da0` | Uses zero fade before the original pass-enable comparison and fade publication, for both ordinary scene and camera override paths. Preserves focus-plane calculations. |

`descriptors` is the single ordered list. `Options` has one boolean per descriptor:
true means Off, false means Original. `apply` and `rollback` preflight every
selected site's signature and extent before changing any site. A mismatched
version, signature or already-applied selected patch throws; no partial batch is
written. `Readback` reports controls and changed bytes. Call only before guest
workers execute, while the loader owns the writable image. This API deliberately
does not change page protections or implement live patching. The parent startup
adapter owns instruction-cache synchronization and application logging.

## Verification and limits

Run `tools/settings/retail_graphics/check.ps1`. It reads the pinned user dump,
extracts a small private fixture, builds the C++ controls using MSVC and the
existing ABI bridge, and executes original retail instructions on the CPU.
No game window or GPU is launched.

The oracle is original retail execution with neutral input, compared byte-for-byte
against patched retail execution with non-neutral input and then restored
execution. Tests cover registered/unregistered constants, existing cache values,
linked/unlinked dirty tracking, repeated publication, NaN and signed-zero lanes,
camera cut/default/explicit exposure, singular/identity matrices, camera DoF
override, negative/zero/positive fade, and two viewport sizes. Original viewport,
reference-count, singleton, matrix-adjoint, vector and boolean helpers execute
where needed. All objects, lists, globals and preallocated caches are synthetic.
Two isolated child controls reach the exact original allocation boundary without
executing an allocator. Allocation growth, lazy singleton initialization, and
handle resolution are not certified by these fixtures.

Retail evidence includes the executable's own class registrations, parameter
registration strings, relocated vtables and instructions. Fixture provenance and
original bytes stay in the local, ignored build area. Public names are descriptive or occur in retail metadata. None of these
checks proves GPU output, GPU cost reduction, or real-menu construction. Bloom,
SSR and vignette neutralization do not claim to skip passes or reclaim targets.
The user owns visual verification, including Original / Off / Original checks.

AO, shadow quality, geometry LOD, VFX budgets, artistic color ranges and additional
techniques must not be inferred as supported from these five descriptors. Exact
remaining research boundaries are recorded privately and are not selectable
placeholders in this adapter.
