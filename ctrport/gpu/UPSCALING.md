# Scene upscaling

Launcher Settings → Scene upscaling: **Off** (default), **AMD FSR 1**, **AMD FSR 3.1**
or **DLSS (experimental)**. CLI: `python ctrport/play_gpu.py --1.21 --internal-res 1080
--upscaler fsr3 --res 1920 1080`. Environment: `CTR_UPSCALER=off|fsr1|fsr3|dlss`.
Changing the setting requires restarting the game.

FSR 1 is the spatial EASU + RCAS implementation already in pinned shadPS4 v0.18.0.
FSR 3.1 is AMD's temporal upscaler through FireBurn/FSR-Vulkan (MIT), pinned at
`c64f093404125e960813f95f1996c14780f0dd69` by `tools/bootstrap.ps1` and patched by
`build.ps1` (`patches/fsr-vulkan/README.md`). It receives exactly the inputs DLSS gets
(scene color, R32F depth, RG16F motion, Halton jitter, the retail projection's near/far
planes and field of view) and shares DLSS's object-motion limits below.
`CTR_FSR3_SHARPNESS` (0–1, default 0.3) sets its sharpening.
DLSS Super Resolution requires an RTX GPU, its driver, the bridge, and NVIDIA's DLL.

## Actual rendering path

The internal-resolution selector controls the guest scene attachments. The game
uses its Neo display canvas (3840×2160), independently of the host window. A scene
at 1080p is therefore upscaled to that canvas, then the completed frame is fitted
to the window. Selecting a 1080p window does not itself change the canvas to 1080p.
This additional work means there is no promised FPS gain on CPU-bound scenes.
FSR bypasses equal-size or larger inputs. DLSS at equal sizes requests DLAA.

Successful video-out registrations identify display targets by address. At the
start of a display pass, the previous color attachment identifies the finished
scene. Its sampled descriptor is replaced only when the frame had a four-target
G-buffer pass and dimensions/aspect match. The game's subsequent HUD draws are
unchanged. Image ids retained across passes are checked against their unique ids.
The old final-frame FSR option is disabled when a scene upscaler is selected.

DLSS receives the real scene color, sample 0 of scene depth converted to R32F,
RG16F motion, and subpixel Halton viewport jitter. The shaders handle separate
depth/velocity MSAA counts. Vulkan layouts, device extensions, initialization,
feature creation, and successful evaluation are handled through NGX.

### Camera evidence and remaining limits

Retail v01.21 title draws use vertex buffers of 400/2864 or 464/2928 bytes, not the
528-byte buffer assumed by the reference integration. Captures from original
execution show WVP at floats 0–15, world at 16–31, projection at 32–47 or 48–63.
For three draws with different object transforms, `inverse(P) * WVP * inverse(W)`
produces the same camera (translation agreement within 0.002 in the sampled frame).
The runtime validates perspective/affine structure and invertibility, derives the
current view, and retains the preceding frame's view. Missing camera data causes
a logged skip, never silent substitution of an unvalidated matrix.

Object-motion decoding is still experimental: the reference velocity target marks
moving objects, whose motion defaults to zero. This can produce trails on moving
objects. Camera reprojection alone does not establish correct animation motion or
race-wide temporal quality. This limitation is why DLSS is labeled experimental.
No reconstructed retail functions or new playable-race milestone are claimed.

## Build and dependencies

The GPL renderer sources and MIT bridge originate from the public repository and
revision recorded in `upscaling/SOURCES.json`, with hashes of every imported source.
The pinned shadPS4 tree is unchanged; `make_patched.py` generates overlays.

NVIDIA SDK used locally: https://github.com/NVIDIA/DLSS at
`9981c6350dc58c0ce21f024fd65e31e851c31689`. It stays in ignored `build/dlss-sdk`.
Build with `pwsh -NoProfile -File ctrport/gpu/upscaling/bridge/build.ps1` after
placing that SDK there. This produces `build/CTRPort-dev/ctr_dlss.dll`, copies
`nvngx_dlss.dll` and its NVIDIA license next to `ctrport.exe`. NVIDIA binaries have
their own license and are not added to the GPL source tree. This task does not
publish or package them for distribution.

## Repeatable checks

`tools/profiler/scene_resolution_probe.py --upscaler fsr1|dlss` launches original
retail code offline with isolated save copies. It checks actual depth/color
attachments, physical window dimensions, real dispatch logs and nonblank finite
GPU output pixels. `CTR_UPSCALE_CAPTURE_DIR` enables fenced GPU readback on the
30th processed 3D scene, including DLSS depth and motion. Raw captures remain in
ignored `runtime/user`; do not commit game-derived images or buffers.
`CTR_SCENE_TRACE` additionally records shader-buffer layout diagnostics.

Review input/output captures together: successful dispatches alone are insufficient.
The initial FSR capture hit a black startup frame; the initial DLSS capture had no
camera data. These are development failures, superseded only by later validated
runs, not retroactively relabeled as working tests.

## Verified 2026-10-16

On RTX 4070 Ti SUPER (driver 610.88), isolated original retail runs passed:

| Retail | Mode | Actual scene → output | Physical window |
|---|---|---|---|
| 1.21 | DLSS | 1920×1080 → 3840×2160 | 1920×1080 |
| 1.00 | DLSS | 1280×720 → 3840×2160 | 1920×1080 |
| 1.21 | FSR 1 | 1280×720 → 3840×2160 | 1920×1080 |
| 1.00 | FSR 1 | 1920×1080 → 3840×2160 | 1920×1080 |

Both DLSS runs logged validated camera data and successful NGX evaluations. Depth
and motion readbacks were finite. All four output images were nonblank, contained
the same scene as their input, and differed from bilinear enlargement of that same
input (mean absolute difference 0.00091–0.00197 on a 0–1 scale). These differences
verify processing, not a quality ranking. GPU captures show the animated title
trophy/Crash before HUD; no race verification is claimed by these tests.

Off control on retail 1.21 passed at internal 1080p with no upscaler dispatch.
Launcher tests exercised all three options, checked saved `upscaler=dlss`, and
verified that display/internal-resolution selections were preserved.

## Race check, 2026-10-16 (incomplete temporal validation)

Retail 1.21 reached an actual single race on Crash Cove with Off, FSR1 and DLSS.
The physical controller was detected through SDL; the runtime's existing test
input overlay operated only the isolated test process. Acceleration and camera
movement were visually verified for FSR1 and DLSS. No full lap was completed.

At a 3840x2160 canvas and 1920x1080 host window, 30-second samples of newly
presented guest frames gave:

| Mode | Internal scene | Mean FPS | p95 frame time |
|---|---|---:|---:|
| Off | 3840x2160 | 46.35 | 33.97 ms |
| Off | 1920x1080 | 49.49 | 33.84 ms |
| FSR1 | 1920x1080 | 50.15 | 34.06 ms |
| DLSS, default preset | 1920x1080 | 29.88 | 34.27 ms |

These are preliminary single samples, with the player stationary at the starting
grid after green and AI racing. AI rosters and elapsed race times differ; they
are not deterministic replays or a full driving benchmark. The 60 FPS patch was
enabled, detailed runtime logging was identical, and screenshots/input were
outside measurement. Reused host refreshes are excluded. FSR's observed +8.2%
against native 4K is not a guaranteed uplift; its +1.3% against Off at 1080p is
too small to separate from run variability. DLSS regressed in this configuration.

**Object motion is not corrected.** The reference assumption that G-buffer target
3 channel 2 is a moving-object mask fails visual inspection: it selects parts of
Crash but excludes the kart. Flagged pixels receive zero motion, while the actual
objects animate/move. Linear and squared decoding trials did not establish a
reliable mapping. Do not call these validated object motion vectors or present
DLSS as race-quality certified. The camera reprojection does work on the sampled
nonflagged region: consecutive-frame photometric error drops from 14.58 to 5.68
on the 0–255 scale during acceleration. This metric also includes lighting,
disocclusion and the game's own motion blur; it is not ground-truth flow.

`tools/profiler/race_upscale_session.py` provides isolated run/pad/shot/measure/stop
commands. `tools/profiler/upscale_motion_check.py <capture-directory> --frame 1`
reproduces the diagnostic from consecutive retail GPU captures and logged jitter.
Each run had 21 active compatibility stubs, of which 10 distinct ones were called.
All raw game-derived captures remain under ignored `runtime/user/instances`.

## Frame generation (set aside)

`upscaling/ctr_framegen.{h,cpp}` adapts GetLostIn's FSR 3.1.6 frame generation: it fills
only the gap between the real frame rate and the target (the game's 30/60, or
`CTR_FRAMEGEN_TARGET`), uses the upscaler's depth and motion when FSR 3 or DLSS ran, and
interpolates a HUD-less copy of the frame captured between the scene stretch and the first
HUD draw. It is off unless `CTR_FRAMEGEN=1` and has no launcher setting: in a race on
2026-10-21 (FSR 3, 720p scene, 1920x1080 window) it reached about twice the real rate with
game motion and the HUD kept out, but moving images still left visible trails.

## Portable release distribution

The v0.3.1 graphics package includes FSR1 but excludes the SDK-linked bridge and NVIDIA binaries. For optional local DLSS testing, obtain the official NVIDIA/DLSS SDK at the pinned revision above, run ctrport/gpu/upscaling/bridge/build.ps1 from a source checkout, and copy its ctr_dlss.dll and nvngx_dlss.dll beside the portable build/CTRPort-gpu/ctrport.exe. The SDK has its own license. DLSS remains experimental with unresolved object motion.
