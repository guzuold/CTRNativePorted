# ctrport

SPDX-License-Identifier: GPL-2.0-only

The native Windows runtime of Crash Team Racing Nitro-Fueled and its launcher. `ctrport.exe`
loads the user's own retail eboot and system modules, runs their original x86-64 code on the
CPU and provides the PS4 system services they import: kernel, threads, files, save data, pads,
audio, video output and the online services. Rendering is done by `ctr_d3d.dll` (Direct3D 11,
`d3d/`) or `ctr_gpu.dll` (Vulkan, from shadPS4, `gpu/`); `ctr_hle.dll` (`hle/`) holds the
system library functions written in the guest ABI.

## Building

`ctrport/build_release.ps1` builds everything a release contains, after `tools/bootstrap.ps1`
(see the top-level README). For quicker work on one part:

- `ctrport/build_launcher.ps1`: the launcher (`launcher.cpp`, Win32 and GDI+ only) into
  `build/CTRPort-dev`.
- `ctrport/build_profile.ps1`: `ctrport.exe` with debug information, into `build/CTRPort-dev`.
- `ctrport/d3d/build.ps1`, `ctrport/gpu/build.ps1`: the renderer libraries.
- `ctrport/online_server/build_online_server.ps1`: the project server.

## Running without the launcher

`python -B ctrport/play_gpu.py --1.21 --internal-res 720 --res 1920 1080` starts the game with
the same command line, environment and logs the launcher uses; `--help` lists the options.
`python -B ctrport/run_logged.py <runtime arguments>` copies both output streams to the console
and to `runtime/user/logs/startup.stdout.log` / `startup.stderr.log`.

The launcher keeps its settings in `runtime/user/launcher_config.json`. Writable data (saves,
logs, settings, caches) stays in `runtime/user`; the game folders are only read. The artwork the
launcher shows is read at run time from the user's dump; none is stored in the repository.

## Rules the runtime follows

- Every byte patch and binding is keyed by the SHA256 of the eboot it applies to, and checks the
  original bytes before writing (`retail_profiles.hpp`, `retail_map.hpp`, `GUEST_PATCHES.md`).
- A system function the runtime does not implement fails with its NID, library and caller
  address instead of returning success. The few declared compatibility stubs (online and social
  services not needed offline) are listed in `compat_stubs.json` and log every call.
- Implementations follow documented or observed behaviour; where they follow shadPS4 v0.18.0
  (GPL-2.0-or-later) the source file and revision are named in the code.

## Other documents

`GUEST_PATCHES.md` (patches applied in memory), `MODS.md`, `XBOX_PROMPTS.md`, `UPDATES.md`
(updater), `LOCAL_BACKUPS.md`, `settings/README.md` (PC Settings), `gpu/UPSCALING.md`,
`d3d/VENDOR.md`, `gpu/VENDOR.md`, `online_server/INVITATIONS.md`.

`ctrport/build_capture_window.ps1` builds `capture_window.exe`, which saves a picture of one
process's window (Windows Graphics Capture) without changing focus; it is used for automated
checks.
