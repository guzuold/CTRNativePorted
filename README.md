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

**Reconstruction:** original functions replaced at run time by verified C++: **0** so
far. The reconstructed modules in [`reconstruction/`](reconstruction/) are compiled and
validated against original execution (thousands of fixtures, zero differences), but none
is switched on in the runtime yet.

## How it works

- `ctrport/` — the runtime (`ctrport.exe`, MSVC) and native Win32/GDI+ launcher
  (`ctrport/launcher.cpp` → `launcher.exe`, no Python required). The runtime maps
  the eboot and the game's own PRX modules, resolves imports by NID, and implements
  kernel, threads, memory, files, save data, audio (XAudio2, ATRAC9), pad (XInput and PlayStation controllers), user
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

## Playing (release build)

Download `CTRNativePorted-<version>-win64.zip` from
[Releases](https://github.com/guzuold/CTRNativePorted/releases). It already contains the
launcher, the runtime, the renderer and HLE DLLs and the Visual C++ runtime DLLs. Nothing
else needs to be installed.

You need:

- Windows 10/11 x64, a GPU with Vulkan 1.3, an XInput controller or a PlayStation controller
  (DualShock 4 or DualSense, USB or Bluetooth; no DS4Windows needed).
- Your own dump of CUSA14876 v01.00 (eboot SHA256 `ca9c9441…`), optionally with the
  v01.21 update (eboot SHA256 `39d716b7…`, required for 60 FPS). Other versions are
  rejected.

Then:

1. Extract the zip to a folder of your choice.
2. Either select your dump folders in the launcher, or place them next to it as
   `dump\CUSA14876` (base game) and `dump\CUSA14876-patch` (v01.21 update), where they
   are found automatically.
3. Run `launcher.exe`, choose the options and start the game.

Logs, saves and caches go to `runtime\user\`. The game folders are only read.

## Building from source

Everything in [Playing](#playing-release-build) applies, plus:

- Visual Studio 2022 **Community** in its default path (MSVC, MASM, "C++ CMake tools
  for Windows"); the build scripts currently use that fixed path. Python 3.12. Git.
- LLVM/clang-cl 19.1.1 in `tools/toolchains/llvm-19.1.1`: downloaded and verified
  (size and SHA256) by `tools/bootstrap.ps1`, see
  [`tools/toolchains/README.md`](tools/toolchains/README.md).
- shadPS4 v0.18.0 sources in `engine/shadPS4`: cloned by `tools/bootstrap.ps1` at tag
  `v.0.18.0` (commit `e3ce810f3a653f43ac64ebab63023de281a4103a`, checked after
  checkout) together with its own submodules.

### Build and run

Clone and prepare the dependencies (once):

```powershell
git clone https://github.com/guzuold/CTRNativePorted.git
cd CTRNativePorted
powershell -File tools/bootstrap.ps1
```

Then, from the repository root:

```powershell
powershell -File ctrport/build_audio_backend.ps1
powershell -File ctrport/build_font_backend.ps1
python -B tools/lane_lock.py --name build-data --lane data -- powershell -File ctrport/video/build.ps1     # ctr_video.lib
powershell -File ctrport/build.ps1              # build/CTRPort-infra
powershell -File ctrport/build_profile.ps1      # build/CTRPort-claude/ctrport.exe
powershell -File ctrport/build_launcher.ps1     # build/CTRPort-claude/launcher.exe
python -B tools/lane_lock.py --name build-claude --lane claude -- powershell -File ctrport/gpu/build.ps1  # ctr_gpu.dll, ctr_hle.dll
```

`ctrport/video/build.ps1` and the scripts it calls refuse to run without the build lock,
hence `tools/lane_lock.py`.

> **Known limitation:** `ctrport/video/build.ps1` still reads compile and control results
> from `reports/reconstruction/data/`, which is local material not published in this
> repository. Until that step is separated from the local evidence, `ctr_video.lib`
> cannot be built from a fresh clone.

Start the native launcher (`build/CTRPort-claude/launcher.exe`), or run from the
command line:

```powershell
python -B ctrport/play_gpu.py --base-dir <CUSA14876 folder> [--1.21 --patch-dir <update folder>] [--60fps] [--pro] [--native-4k] [--1080p | --fullscreen]
```

Logs, saves and caches go to `runtime/user/`. The game folders are only read.

## Multiplayer (Matchmaking with password)

Online play uses the project's own server in place of PSN and Demonware; Sony and Activision
servers are never contacted. Only the player who hosts runs the server: it is built into the
launcher (C++, no Python needed).

1. **Host**: in the launcher `SETTINGS` tab turn on **Multiplayer Online** and **Ospita server
   qui** (host server here). The launcher starts the server, asks the router to open the ports
   with UPnP and shows the address to give the other players (also copied to the clipboard). If
   the router has UPnP port forwarding disabled, forward TCP 8780, TCP 3074, UDP 3078-3082 and
   UDP 3174 (the host's own game) to the host PC by hand and type the public IP in the field
   above the switch.
2. **Other players**: turn on **Multiplayer Online** and type the host's address as server (a
   name such as a dynamic DNS host works too). They do not run any server.
3. Everyone types the same **Password partita** (room password), launches the game and selects
   **ONLINE > Matchmaking > Go to Lobby**. Same password: same lobby and race; different
   passwords never match.

**Relay (NAT / CGNAT)** (experimental): players whose connection cannot receive traffic from
the internet (mobile hotspots, carrier-grade NAT, strict routers) can turn this switch on; the
race traffic then goes through the server instead of directly between players (about 2 KB/s
per player and direction). It helps only when the server itself is reachable from the internet:
a host behind CGNAT cannot host the server for players outside its network, relay or not.
Tested so far on one PC with the direct path blocked on purpose, not yet between real networks.

**Pit Stop and Wumpa Challenges** open online with v01.21: the server keeps each player's
Wumpa Coin balance and challenge events (catalogue, prices and challenges come from the game's
own data). Buying items and earning coins from races are not implemented yet, so the balance
starts at 0; for testing, `ctr_online_server.exe --set-balance <online ID> 10 <amount>` sets an
account's Wumpa Coins (currency 10), picked up at the game's next login.

The server also exists as a standalone program, `ctr_online_server.exe --auto-public --upnp`
(built by `ctrport/online_server/build_online_server.ps1`), and the original Python reference,
`tools/online/ctr_online_server.py`, is kept for comparison.

## Additional features

- **Xbox Controller Prompts**: check *Icone Xbox colorate* in launcher Settings to use colored A/B/X/Y prompts (CC0-1.0 by Julio Cacko, see [`ctrport/XBOX_PROMPTS.md`](ctrport/XBOX_PROMPTS.md)). The launcher builds the prompt archives from your own dump on the first launch (about 10 s, no Python); the release folder must contain `assets/xbox-default/`.
- **Any XInput slot**: every connected XInput controller drives player 1, so a pad that Windows
  puts on slot 2–4 (for example next to a virtual pad from Steam or a controller emulator) works.
- **PlayStation controllers**: DualShock 4 and DualSense work directly (read through SDL's
  HID drivers in `ctr_gpu.dll`), with vibration; the touch pad click and Share act as the
  touch pad button. Motion sensors and touch position are not passed to the game.
- **Quick Controller Exit**: hold **View + Menu** (PlayStation: **Share + Options**) for 1.2 seconds on your controller, then press **A** (Cross) to confirm exit or **B** (Circle) to cancel.
- **Rendering Modes**: choose between standard 1080p, PS4 Pro 1440p, or **Native 4K** internal resolution buffers, with 60 FPS unlocked on v01.21.


## Credits and licenses

- Project code: **GPL-2.0** ([`LICENSE`](LICENSE)).
- [shadPS4](https://github.com/shadps4-emu/shadPS4) v0.18.0 (GPL-2.0-or-later): renderer,
  GnmDriver, VideoOut, event queues, public NID names.
- Bloodborne Windows port (bbport, revision `4fcdf770`) shims (GPL-2.0), reviewed per file.
- 60 FPS patch: "60 FPS Unlock" 2.0 by **illusion**, from the
  [GoldHEN Patch Repository](https://github.com/GoldHEN/GoldHEN_Patch_Repository).
- Crash Team Racing Nitro-Fueled is a trademark of Activision Publishing, Inc.
