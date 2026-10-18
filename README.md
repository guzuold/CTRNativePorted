# CTRNativePorted

A native Windows runtime for **Crash Team Racing Nitro-Fueled** (PS4), in the style of
the Bloodborne PC port: it loads **your own** PS4 dump, runs the game's original x86-64
code directly on the CPU, implements the PS4 system libraries for Windows, and renders
with Vulkan through the shadPS4 renderer used as a library. All game logic runs as the
original code from your dump.

> This repository contains **no game files**: no executables, data, archives, audio or
> video from the game. You need your own legally obtained dump. Not affiliated with
> Activision, Beenox, Sony Interactive Entertainment or Nintendo.

## Status

Latest release: **v0.3.2** (experimental).

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
powershell -File ctrport/video/build.ps1     # ctr_video.lib
powershell -File ctrport/build.ps1              # build/CTRPort-infra
powershell -File ctrport/build_profile.ps1      # build/CTRPort-dev/ctrport.exe
powershell -File ctrport/build_launcher.ps1     # build/CTRPort-dev/launcher.exe
powershell -File ctrport/gpu/build.ps1  # ctr_gpu.dll, ctr_hle.dll
```

`ctrport/video/build.ps1` and the scripts it calls refuse to run without the build lock,
hence `tools/lane_lock.py`.

> **Known limitation:** `ctrport/video/build.ps1` still reads compile and control results
> from (development notes), which is local material not published in this
> repository. Until that step is separated from the local evidence, `ctr_video.lib`
> cannot be built from a fresh clone.

Start the native launcher (`build/CTRPort-dev/launcher.exe`), or run from the
command line:

```powershell
python -B ctrport/play_gpu.py --base-dir <CUSA14876 folder> [--1.21 --patch-dir <update folder>] [--60fps] [--pro] [--native-4k] [--1080p | --fullscreen]
```

Logs, saves and caches go to `runtime/user/`. The game folders are only read.

## Multiplayer (friends and parties, experimental)

Online uses the project's own server in place of PSN and Demonware.

1. In **Settings**, enable **Online** and choose your visible player name (3-16
   letters, digits, hyphens or underscores). The local service starts automatically.
2. Open the friends drawer on **Home**. Copy your friend code and exchange codes
   with your friends; add each other in the friends dialog.
3. Check an online friend's status, then **invite them** or **join their lobby**.
   Received invitations can be accepted in the dialog. A party holds at most
   **8 people, including its owner**; each member needs a different player name.
4. Form the party **before PLAY**. Everyone launches the game and selects
   **ONLINE > Matchmaking > Go to Lobby**, using compatible matchmaking modes.
   The launcher supplies the same network selector to the party's members; the
   server waits for the whole group before creating the matchmaking lobby.

No manual server address, host toggle or shared password is needed in the launcher.
Friend codes identify signed services; discovery uses Mainline BitTorrent DHT.
Own party/invitation state refreshes periodically; friend status is checked on demand.
The complete drawer, including its opening tab, disappears outside Home.

**Automatic relay fallback:** traffic tries the direct connection first and falls
back to the project's relay when the peer probe times out. This does not make an
unreachable host reachable: restrictive NAT/CGNAT can still prevent discovery or
hosting. The launcher requests UPnP forwarding when supported by the router.

**Current limits:** changing parties requires closing the game. There is no host
migration; an owner with guests cannot abandon the party from the dialog. Local
service/protocol tests pass, but an online retail race across remote PCs has not
been verified. This is not a claim of complete online support. Signed requests
authenticate peers; they do not encrypt the HTTP transport.

## Wumpa Coins and Pit Stop (v01.21, experimental)

Enable **Online** in Settings to start the project service, including for single-player
Adventure; no friend or party is required. Valid end-of-race reports earn persistent
Wumpa Coins from the local track/rank table. Unknown tracks use 32 coins and unranked
activities use first-place base rewards. These simplified project rules do not
replicate all retail bonuses. **Wumpa Coins x100** is optional in Settings and off
by default; choose it before PLAY. It changes earnings, not prices.

Pit Stop supports single-item and bundle purchases, persistent ownership and
balance, and exclusion of already-owned bundle contents from the price. Catalogues
are read locally from your own game; none are distributed. Adventure earnings,
Real Velo purchase/persistence and a four-item Baby Crash bundle were verified
in the original game. Partial ownership and x100 have synthetic test coverage.

**Limits:** Wumpa Challenges progression and payouts are not implemented. Economy
with Online disabled, wallet migration across remote services, unsupported catalogue
variants and multi-page inventory remain deferred. Keep the same player name and
local service to retain access to that account's wallet. See CHANGELOG for scope.

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

## Invitation delivery update (v0.3.1)

Invitations are held on the existing lobby/relay host; recipients retrieve them through outgoing connections. Add the actual host code to the recipient address book. No public listener is required on the recipient, but the host must remain reachable. See [the invitation guide](ctrport/online_server/INVITATIONS.md) for setup, expiry and discovery limits. Game transport is unchanged.

## Graphics update (v0.3.1)

Internal scene resolution (720p–4K) is independent of the display. FSR 1 is included. DLSS is experimental, requires a separately built NVIDIA SDK bridge/runtime, has unresolved object motion and measured slower in the current race test. See [upscaling details](ctrport/gpu/UPSCALING.md) and CHANGELOG for exact test limits.

## Local backups (v0.3.1 update)

Game saves are copied before launch and after exit. Updated hosts also provide signed local snapshots of your Pit Stop wallet. Update both launchers and preserve `runtime/user` when replacing program files. Snapshots retain history but do not automatically synchronize wallets between hosts or restore missing past purchases. See [local backup details](ctrport/LOCAL_BACKUPS.md).

## v0.3.2

The right-side drawer is the room lobby. Retail fonts are loaded from your own dump. Host migration is experimental and disabled by default; see [status and limitations](tools/online/HOST_MIGRATION.md).
