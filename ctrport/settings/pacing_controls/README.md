# Presentation pacing and display modes

All changes take effect after restart. GPU export `ctr_gpu_pacing_abi()==1`
identifies this implementation; it does not prove monitor mode availability.

| Key | Domain | Actual consumer |
|---|---|---|
| presentation_limit | original/30/60/90/120/144/165/240/unlimited | Direct3D 11: game pacing and presentation; Vulkan: normal and generated-frame presentation boundaries, before Vulkan submit mutex |
| window_mode | original/windowed/borderless/exclusive | SDL window construction and fullscreen mode transition |
| display_refresh | original/60/90/120/144/165/240 | enumerated SDL exclusive fullscreen mode, matching requested output dimensions |

Original presentation timing does no additional wait and keeps the console's
vblank pacing (30, or 60 with the 60 FPS patch).

Direct3D 11: any other value removes the console pacing in `d3d/videoout.cpp`
(`PaceFrame`), so the game runs as fast as the PC allows and the value caps it;
`unlimited` sets no cap and V-Sync On caps at the display refresh. The game keeps
real time: with no limit a race ran at about 150 FPS and its timer counted 20.09 s
in 20.08 s of wall clock. Measured on an RTX 4070 Ti SUPER in the menus: Console
60, 144 → 144, Unlimited about 370, Unlimited with V-Sync On 120 on a 120 Hz display.

Vulkan: limits do not increase simulation FPS or generate frames. They cap host
presentation attempts (including existing generated frames); shadPS4 still presents
at most one flip per vblank, so values above 60 change nothing there.

The steady-clock deadline sequence uses absolute integer-nanosecond targets,
preserves phase under small scheduling jitter and rebases after a full missed
period instead of catching up in bursts. No sleep holds Scheduler::submit_mutex;
its own deadline reservation mutex is also released before sleep. Presentation
backpressure can reduce throughput naturally; no guest physics/timer/vblank value
is patched. Operating-system scheduler precision and Vulkan present mode can still
make actual pacing slower or uneven; CPU tests do not measure live frame timing.

Original window mode retains the previous `fullscreen` boolean. Explicit window
modes override that boolean, so the old control need not be deleted. Windowed is
normal resizable; borderless uses the desktop mode. Exclusive requests a genuine
SDL fullscreen mode and synchronizes the transition before Vulkan creates its
surface. Refresh selection requires exclusive mode: there is no fake refresh
setting for borderless or windowed mode. Requested dimensions must match an
enumerated fullscreen mode exactly; nominal Hz permits0.1Hz tolerance (e.g59.94Hz
is a60Hz mode). Actual dimensions/rate and fullscreen state are checked after
SDL_SyncWindow; failure is reported instead of silently choosing a nearest mode.
With Original refresh, exclusive uses the current desktop refresh at requested
resolution, still requiring an enumerated match. No monitor capability is inferred
from CPU fixtures. Applied mode and observed refresh are logged at startup.

`policy.hpp::environment(doc,extended)` returns canonical startup variables and
performs strict merged-document validation, clearing stale Original overrides.
Parent should allow partial selector editing but validate the final snapshot;
`display_refresh != original` requires `window_mode == exclusive`.

## Integration and tests

Apply `tools/settings/pacing_controls/prepare_gpu.py::prepare(gpu)` after renderer,
display and AO adapters, only to a private generated GPU source copy. The adapter
appends to generated presenter construction rather than replacing other overlays.
It resets its two shim inputs (`window.cpp`, `ctr_gpu.cpp`) from the pinned release
on each composed preparation, avoiding accumulated changes. No other current
adapter modifies these files; future overlays must preserve that composition rule.
The window thread catches configuration rejection, notifies initialization failure
and returns GPU-init error-3; failed construction destroys the window so it cannot
leave an exclusive display mode active. This avoids detached-thread termination or
an initializer stuck waiting for a window that failed to be created.

`check.ps1` executes the actual generated constructor with a call-recording SDL
fixture, including Original legacy settings, all modes, successful refresh changes,
unavailable resolution/refresh, denied calls, transition timeout, acknowledgement
failure and constructor cleanup. Fake-clock tests check1000 frames per supported
limit and long stalls. Source checks bind both real presentation call sites to the
pacer before locking. Repeated preparation hashes must be identical. No real window,
GPU, display-mode change, game or timing measurement occurs in these tests.
The parent full GPU build compiles against the actual pinned SDL3 headers/library.

New adapters GPL-2.0-only. Generated shims retain the release's GPL-2.0-or-later
provenance. APIs and contracts were checked in installed SDL_video.h; primary
references: [fullscreen modes](https://wiki.libsdl.org/SDL3/SDL_SetWindowFullscreenMode),
[mode enumeration](https://wiki.libsdl.org/SDL3/SDL_GetFullscreenDisplayModes),
[synchronous transition](https://wiki.libsdl.org/SDL3/SDL_SyncWindow).
