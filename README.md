# CTRNativePorted

A native Windows runtime for **Crash Team Racing Nitro-Fueled** (PS4), in the style of
the Bloodborne PC port: it loads **your own** PS4 dump, runs the game's original x86-64
code directly on the CPU, implements the PS4 system libraries for Windows, and renders
with Vulkan through the shadPS4 renderer used as a library. Alongside the runtime, the
game engine is being **reconstructed in C++**, one function at a time, each verified
against the execution of the original code.

> This repository contains **no game files**: no executables, data, archives, audio or
> video from the game. You need your own legally obtained dump. Not affiliated with
> Activision, Beenox, Sony Interactive Entertainment or Nintendo.

## Status

Two progress measures are tracked separately.

**Runtime milestones**

| Milestone | v01.00 | v01.21 (final update) |
|---|---|---|
| Boot, intro, title screen | ✅ | ✅ (window captures) |
| Menus with controller and audio | ✅ (user observation) | ✅ reached |
| Race playable | ✅ (user observation) | reached; formal capture pending |
| 60 FPS | — | ✅ opt-in patch (`--60fps`), game runs in real time |
| PS4 Pro mode (1440p 3D / 4K UI) | opt-in (`--pro`) | ✅ 2560×1440 3D, 3840×2160 video buffers |
| Native 4K 3D mode (2160p 3D / 4K UI) | opt-in (`--native-4k`) | ✅ 3840×2160 3D + 1920 MiB Garlic VRAM |
| Local split-screen | planned | planned |
| Online | not available (planned: Demonware-compatible server) | — |

**Reconstruction:** original functions replaced at run time by verified C++: **0** so
far. The reconstructed modules in [`reconstruction/`](reconstruction/) are compiled and
validated against original execution (thousands of fixtures, zero differences), but none
is switched on in the runtime yet.

## How it works

- `ctrport/` — the runtime (`ctrport.exe`, MSVC) and native Win32/GDI+ launcher
  (`ctrport/launcher.cpp` → `CTRNativePorted.exe`, no Python required). The runtime maps
  the eboot and the game's own PRX modules, resolves imports by NID, and implements
  kernel, threads, memory, files, save data, audio (XAudio2, ATRAC9), pad (XInput), user
  service and more. Every function that is not implemented fails loudly with its NID and
  caller; the only success stubs are online/social services listed in
  [`ctrport/compat_stubs.json`](ctrport/compat_stubs.json).
- `ctrport/gpu/` — `ctr_gpu.dll` (clang-cl): shadPS4 v0.18.0's command processor,
  shader recompiler and Vulkan renderer compiled in place, with shims adapted from the
  Bloodborne Windows port. Provenance in [`ctrport/gpu/VENDOR.md`](ctrport/gpu/VENDOR.md).
- `ctrport/hle/` — `ctr_hle.dll`: system functions written directly in the guest SysV
  ABI (trophies, save data).
- `ctrport/GUEST_PATCHES.md` — every in-memory change to original game code, bound to the
  eboot SHA256 and checked against the original bytes, with its origin.
- `reconstruction/` — reconstructed engine C++ code (`include/` and `src/`).

## Requirements

- Windows 10/11 x64, a Vulkan 1.3 GPU, an XInput controller.
- Your own dump of CUSA14876 v01.00 (eboot SHA256 `ca9c9441…`), optionally with the
  v01.21 update folder (eboot SHA256 `39d716b7…`). Other versions are rejected.
- Visual Studio 2022 (MSVC, MASM, CMake, Ninja), Python 3.12 (for build scripts),
  LLVM/clang-cl 19.1.1 in `tools/toolchains/llvm-19.1.1` (see
  [`tools/toolchains/README.md`](tools/toolchains/README.md)).
- shadPS4 v0.18.0 sources (tag `v.0.18.0`, commit `e3ce810f`) with submodules in
  `engine/shadPS4`.

## Build and run

From the repository root:

```powershell
powershell -File ctrport/build_audio_backend.ps1
powershell -File ctrport/build_font_backend.ps1
powershell -File ctrport/video/build.ps1        # ctr_video.lib
powershell -File ctrport/build.ps1              # build/CTRPort-infra
powershell -File ctrport/build_profile.ps1      # build/CTRPort-dev/ctrport.exe
powershell -File ctrport/build_launcher.ps1     # build/CTRPort-dev/CTRNativePorted.exe
powershell -File ctrport/gpu/build.ps1          # build/CTRPort-gpu/ctr_gpu.dll, ctr_hle.dll
```

Start the native launcher (`build/CTRPort-dev/CTRNativePorted.exe`), or run from the
command line:

```powershell
python -B ctrport/play_gpu.py --base-dir <CUSA14876 folder> [--1.21 --patch-dir <update folder>] [--60fps] [--pro] [--native-4k] [--1080p | --fullscreen]
```

Logs, saves and caches go to `runtime/user/`. The game folders are only read.

## Credits and licenses

- Project code: **GPL-2.0** ([`LICENSE`](LICENSE)).
- [shadPS4](https://github.com/shadps4-emu/shadPS4) v0.18.0 (GPL-2.0-or-later): renderer,
  GnmDriver, VideoOut, event queues, public NID names.
- Bloodborne Windows port (bbport, revision `4fcdf770`) shims (GPL-2.0), reviewed per file.
- 60 FPS patch: "60 FPS Unlock" 2.0 by **illusion**, from the
  [GoldHEN Patch Repository](https://github.com/GoldHEN/GoldHEN_Patch_Repository).
- Crash Team Racing Nitro-Fueled is a trademark of Activision Publishing, Inc.
