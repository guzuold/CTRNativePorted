# Retail geometry controls

The runtime executes original retail CPU code. This module adds two bootstrap
controls for the pinned 1.21 executable; it does not reconstruct these functions.

`distance_hook.hpp/.cpp` exposes `install(base, extent, sha, DistanceScale)`.
`original` returns a null handle; `half` and `three_quarters` create an instance
whose lifetime must cover every guest call. Install before guest workers start;
`uninstall()` and destruction require all guest workers to be quiescent. Startup
must supply writable executable bytes. The hook rejects wrong identity, prologue,
read-only memory, invalid values and an already installed hook before mutation.

The verified 17-byte setter prologue has no PC-relative operands. Its trampoline
runs the original instructions then resumes at RVA `0x34f211`. A per-instance
SysV-to-Win64 thunk passes a call-local argument array to the callback, with Win64
shadow space and registered unwind data. The original-call ABI bridge preserves
Win64 nonvolatile SIMD registers. No new assembler file or global preferences are
needed. The callback first checks the original enabled/matching-scene predicates;
no-match, empty and disabled branches do not dereference the distance arrays.
It then copies the five authored distances and fades, scales both, and calls the
original setter. Original zero/sentinel/negative/nonfinite or too-small inputs
pass through unchanged. The original distance-culling flags remain enabled.
This controls render distance/fade classes; it is not mesh LOD quality.

`shadow_override.hpp` supplies `apply_shadow_off`, `shadow_verify` and
`rollback_shadow`. At bootstrap, one branch byte at `0x3576a1` follows the original
scene-disabled path in function `0x357540`: all four cascade camera flags are
cleared, the active count becomes -1, and the original cleanup runs. It does not
change the authored enabled flag, resize/free the atlas, or affect every local or
hero shadow. The integration layer owns code protection and instruction-cache
flush as for the other static graphics patches.

Run `tools/settings/geometry_controls/check.ps1` from the repository root, then
`python -X utf8 tools/settings/geometry_controls/audit.py`. The extractor verifies
the retail SHA256 and reads only the selected original CPU functions/constants
into a local, ignored build area.

The distance oracle executes the original full setter with separately scaled
inputs and compares it to hooked execution on unchanged authored inputs. Checks
include both scales, all five pairs, match/miss/disabled/empty branches, sentinel
and nonfinite passthrough, input preservation, rollback, invalid version/signature
and read-only protection, duplicate hook rejection, and eight concurrent fixtures
with distinct scenes. This does not certify concurrently mutating one game scene.

Shadow tests execute original setup/refcount code against synthetic handles and
four camera records, including a null camera. The patched enabled scene must match
an unmodified disabled scene. A separate positive control reaches the original
active split-computation boundary and exits 42; GPU allocation and split math are
outside this fixture. Tests never launch the game or claim visual correctness.

`support.json` explicitly distinguishes these implemented controls from missing
contracts: mesh LOD, shadow atlas quality/recreation, local/hero quality, and VFX
quality. The VFX classifier and its virtual wrappers were traced, but their
lifecycle/critical-gameplay classifications are not verified. Hiding every effect
or repurposing a per-model shadow flag would not implement those controls.
