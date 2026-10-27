# CTRNativePorted

A native Windows runtime for **Crash Team Racing Nitro-Fueled** (PS4), with
**peer-to-peer online play that needs no central server**.

It loads **your own** PS4 dump and runs the game's original x86-64 code directly on the
CPU, without emulating it. It implements the PS4 system libraries for Windows and
renders with Vulkan through the shadPS4 renderer, used as a library. All game logic is
the original code from your dump. Online play replaces PSN and the Activision Demonware
servers with a small service that runs inside every player's launcher.

> This repository contains **no game files**: no executables, data, archives, audio or
> video from the game. You need your own legally obtained dump. Not affiliated with
> Activision, Beenox, Sony Interactive Entertainment or Nintendo.

**Latest release: [v0.7.9](https://github.com/guzuold/CTRNativePorted/releases/tag/v0.7.9-u2)** (US mods on the EU game, mods that do not fit no longer stop the game)

v0.7.1 makes the **Direct3D 11 renderer** the default and lays out the graphics settings like current PC games: upscaling method (FSR 1, FSR 3,
DLSS) with Quality/Balanced/Performance modes and sharpness, a single shadow quality, and the
effects pages. v0.7.0 added a **Direct3D 11 renderer** that does not use shadPS4 (launcher
Settings → Renderer); see the [changelog](CHANGELOG.md). From v0.6.1 or older, install v0.6.2
with **Check updates** first, then the latest version.

The **Nitro Cinema launcher** adds horizontal tabs, antialiased controls and animated
transitions, with background artwork read directly from your own dump. It is selected
by default; switch to the original **Classic** skin immediately with the Appearance
button in **Advanced**. Your choice is saved. Disable Nitro Cinema animations there
if preferred. Use **Check updates** with the game closed to
update from v0.6.0 or earlier; your saves, settings and mods are preserved.

This is an experimental pre-release, with PC Settings inside the game's own Options menu. The game is
playable from start to finish. Online races between two PCs over the Internet work and
complete, in private matches and in public matchmaking; see the
[limits](#online-status-and-limits).

## Contents

- [Features](#features)
- [Online play without a central server](#online-play-without-a-central-server)
- [Playing (release build)](#playing-release-build)
- [Building from source](#building-from-source)
- [How it works](#how-it-works)
- [Project status](#project-status)
- [Credits and licenses](#credits-and-licenses)

## Features

### Game
- **Supported game:** European edition CUSA14876, v01.00 and the v01.21 update. Each
  supported executable is identified by its SHA256 and other executables are refused. The
  American edition (CUSA13795) is recognized, but untested.
- **The original game, unmodified:** it can be played from start to finish, and the Pit
  Stop shop works.
- **Split screen** for 2–4 local players.
- **Game language:** English, French, Italian, German, Spanish or Arabic, chosen in the
  launcher.
- **Retail fonts and launcher background** come from your own dump. None are distributed.

### Graphics
- **PC Settings in the game's Options menu** (v01.21): native pages in the game's language
  for display, graphics, resolution and upscaling, quality, shadows and effects, and
  quality presets. Anti-aliasing (FXAA, off, SMAA), bloom, motion blur, depth of field,
  fur detail and view distance apply at once; the other settings at the next start. The
  menu is built at startup from your own game archives, which are only read; nothing from
  the game is distributed. See [`ctrport/settings/README.md`](ctrport/settings/README.md).
- **Internal 3D resolution** from 720p to native 4K (720p, 900p, 1080p, 1440p, 1800p,
  2160p), independent of the window or display resolution.
- **60 FPS** on v01.21, through the community "60 FPS Unlock" patch, applied in memory only.
- **PS4 Pro rendering mode.**
- **Upscaling:**
  - AMD FSR 1 for the 3D scene, applied before the HUD;
  - AMD FSR 3.1 (temporal), through FireBurn/FSR-Vulkan, on the same depth, motion and jitter
    as DLSS;
  - NVIDIA DLSS (experimental), which needs an NVIDIA SDK bridge that you build yourself.
  See [`ctrport/gpu/UPSCALING.md`](ctrport/gpu/UPSCALING.md).
- **Window or full screen,** with resolution presets or a custom size.

### Controllers
- **Xbox / XInput controllers** on any slot. A pad that Windows puts on slot 2–4 still
  drives player 1.
- **PlayStation controllers:** DualShock 4 and DualSense, over USB or Bluetooth, with
  vibration and no DS4Windows needed.
- **Keyboard input.**
- **Controller per player:** Settings detects the connected controllers. It shows each
  controller's type: Xbox controller on its XInput slot, DualShock 4, DualSense or another
  gamepad. Each of players 1–4 gets a menu: Auto, None, Keyboard or one of the controllers.
  Assigning a controller to players 2–4 turns split screen on. Players left on Auto join by
  holding a button, as before.
- **Colored Xbox button prompts** (optional). They are built from your own dump on the
  first launch. See [`ctrport/XBOX_PROMPTS.md`](ctrport/XBOX_PROMPTS.md).
- **Quick exit from the controller:** hold View + Menu (Share + Options) for 1.2 seconds,
  then confirm with A (Cross).
- **Quick exit from the keyboard:** hold Esc for two seconds with the game window in front;
  the game closes as with the window's close button.

### Mods and patches
- **Loose-file mods** in `runtime/mods`, with load order in `mods.json`.
- **shadPS4 / GoldHEN patch XML files** in `runtime/patches`, with choices in
  `patches.json`.
- **Same formats as SacredFire / GetLostIn CTR-NitroFueled-PC,** so the same mod folders
  and patch files work in both.
- **Game files are never modified.** Every patch is checked against the running executable
  before it is applied. See [`ctrport/MODS.md`](ctrport/MODS.md).

### Trophies
- **The game's 48 trophies unlock during play.** A notice with the trophy icon appears over
  the game, and the platinum unlocks with the last base trophy.
- **Unlocks are kept** in `runtime/user/trophies` and reported back to the game.
- **TROPHIES tab** in the launcher: every trophy icon, unlocked or locked, with the unlock
  date and the progress.
- **Names and descriptions:** with your own PS4 trophy key, typed in the tab or imported
  from shadPS4's `keys.json`, they are shown in the game language. Without the key, the
  launcher shows them in English from the
  [Crash Bandicoot Wiki](https://crashbandicoot.fandom.com/wiki/Crash_Team_Racing_Nitro-Fueled)
  (CC BY-SA), with a tip on how to unlock each one; a copy is kept in
  `runtime/user/trophies/wiki.json` for offline use.

### Wumpa Coins and Pit Stop
- **Wumpa Coins** are earned in races, Adventure included, and kept by your own local
  service.
- **Pit Stop purchases:** single items and bundles, with permanent ownership. Items you
  already own are deducted from bundle prices.
- **Catalogues** are read from your own game.
- **Optional Wumpa Coins ×100** for earnings; prices do not change.
- **Simplified earnings:** Wumpa Coins follow project rules, not every retail bonus.
- **Not implemented:** Wumpa Challenges and Grand Prix schedules.

### Launcher
- **Native Windows application:** Win32 and GDI+, with no Python and no installer.
- **Check updates:** downloads releases from GitHub after verifying their SHA-256. The
  installer runs separately, with a backup and a rollback.
- **Automatic save backups** before every launch and after every exit, including after a
  crash. Signed snapshots of the Pit Stop wallet are kept too. See
  [`ctrport/LOCAL_BACKUPS.md`](ctrport/LOCAL_BACKUPS.md).
- **Read-only game folders:** logs, saves, caches and settings stay in `runtime/user`.
- **Logs with a size limit:** per game session and stream, the first 32 MB and the last
  16 MB are kept; only the 8 most recent sessions stay in `runtime/user/logs`.

## Online play without a central server

There is no project server, no room host to run and no account to create. Each player's
launcher runs its own copy of the online service. The game talks only to that local
service; the services find each other and cooperate directly.

```
 Player A                                  Player B
 game  ──►  service (in A's launcher)  ◄─signed─►  service (in B's launcher)  ◄──  game
   └──────────────── race traffic: direct UDP between the two games ────────────────┘
```

**Identity and friends**
- Every installation has its own key pair. Its **friend code** comes from the public key.
- The account the game uses for a player comes from the friend code together with the player
  name, so players with the same name on different PCs stay distinct.
- Friends add each other by code in the launcher's friends drawer. Only mutual friends see
  each other's presence or receive invitations.
- The same friends appear in the **game's own friends list**, with live status: in game,
  launcher open, or offline. You invite them, or JOIN them, from the game itself.

**Discovery**
- Services find each other on the public **Mainline BitTorrent DHT**, so no rendezvous
  server is involved.
- The last verified address of each friend is kept, so a restarted launcher can be reached
  again without a new search.
- Every address, wherever it was found, is checked against the friend's identity before it
  is used.

**Signed service-to-service channel**
- Requests between services are signed with the sender's key. They name their recipient
  and carry a single-use nonce, valid for a 60-second window.
- Signatures authenticate the peers; they do not encrypt the HTTP transport.

**Invitations and JOIN**
- A session stays on its host's service. A friend's service forwards session and
  invitation requests to it, signed. Inviting sends at once and the recipient accepts in
  the game.
- JOIN reads the friend's published session record and connects. Both services authorize
  that verified friend for the join.

**Public matchmaking**
- *Online > Matchmaking* pairs players who are not friends.
- Services that are reachable from the Internet announce themselves on the DHT. The
  **matchmaking hub is elected automatically**: the lowest friend code among the reachable
  services.
- Players behind NAT join through their own service, which forwards their matchmaking to
  the hub.

**Gameplay traffic**
- Race traffic is the game's own UDP protocol, sent **directly between the games**.
- The runtime admits only peers that one of these vouches for:
  - the assigned matchmaking lobby;
  - a session the player has joined;
  - a verified friend's JOIN.

  The policy applies to outbound connect/send and inbound UDP/TCP accept. The configured
  project server and loopback are allowed; private/LAN, link-local and CGNAT ranges are
  also allowed unless `CTR_ONLINE_STRICT_PEERS=1`. Other unapproved addresses are blocked.
  Outbound multicast/broadcast discovery remains allowed; invalid source addresses are denied.
  Sharing the matchmaking queue alone does not authorize a connection.

  Incoming unknown peers are dropped while a single background worker queries the local
  service (at most four query starts/second, 256 cached/pending addresses). An authorized
  peer is renewed every second, ahead of unknown addresses; while it keeps sending it stays
  accepted during the renewal, so race traffic is not interrupted. A refusal takes effect at
  the service's answer; without an answer the authorization ends three seconds after the last
  confirmation. Negative answers are retried after 250 ms. The first packet from a newly
  assigned peer may be dropped until authorization completes; game retransmission is
  required. Under cache saturation additional unknown addresses fail closed.
  Relay deliveries are checked using the relay's real source before exposing its peer address;
  discovery source remapping happens after filtering. Rejections are counted, with one log
  per address for up to 4096 distinct addresses; further new-address logs are suppressed to
  bound memory. A non-blocking receive processes at most 64 rejected packets per call; a
  blocking receive waits for the next accepted packet.

  Only signed peer/party requests and identity verification are exposed remotely over HTTP.
  Game routes (including config, traffic-key, auth3 and WebAPI) require a loopback source.
  Use each PC's own launcher service; a remote/LAN `CTR_ONLINE_SERVER` deployment is no
  longer supported. The verified internal peer forwarding path remains available.
  HTTP headers and bodies each have an absolute five-second read deadline. Remote TCP
  connections are limited to 128 total and 8 per source IP; the local game (127.0.0.1) has a
  separate quota of 64, so remote connections cannot cut it off. The lobby has no idle timeout.
  Lobby TCP 3074 and diagnostic TCP/UDP listeners bind only to loopback. TCP 3074 is no
  longer requested through UPnP; existing manual/router mappings are not modified.
- The launcher asks the router for port forwarding with UPnP when the router supports it.

**What replaces the official servers**
- The local service implements the parts of PSN and Demonware the game uses:
  - authentication;
  - friends and presence;
  - session invitations;
  - matchmaking;
  - the Pit Stop economy.
- The game's Demonware traffic key is replaced in memory with the local service's key.
- **No connection is made to Activision or Sony servers.**

### Using it
1. In **Settings**, enable **Multiplayer Online** and choose your player name (3–16
   letters, digits, hyphens or underscores).
2. Open the **friends drawer** on Home, copy your code and exchange codes with your
   friends; add each other.
3. Start the game. Invite friends or JOIN them from the in-game friends list, or use
   **Online > Matchmaking** for public games.

### Online status and limits
- **Verified:**
  - **two separate PCs over the Internet:** complete races, both in a private match with
    a friend and through public matchmaking;
  - two retail instances on one PC, each connected only to its own service: JOIN from the
    friends list (Crash Cove) and public matchmaking through the elected hub (Polar Pass);
  - local protocol suites with separate services, covering friends, invitations,
    matchmaking and peer authorization.
- **Not yet verified:** races with more than two players.
- **No relay between services yet:** players behind strict NAT or carrier-grade NAT may not
  reach each other directly.
- **Public matchmaking needs at least one service reachable from the Internet,** through
  UPnP or an open port.
- **Changing party needs a game restart.** Race host migration exists, but it is
  experimental and off by default. See
  [`tools/online/HOST_MIGRATION.md`](tools/online/HOST_MIGRATION.md).
- **Full online support is not claimed.** Online and NP services that are not implemented
  yet are listed in [`ctrport/compat_stubs.json`](ctrport/compat_stubs.json), and every
  call to them is logged.

## Playing (release build)

Download `CTRNativePorted-<version>-win64.zip` from
[Releases](https://github.com/guzuold/CTRNativePorted/releases). It contains the launcher,
the runtime, the renderer and HLE DLLs, the online service and the Visual C++ runtime
DLLs. Nothing else needs to be installed.

You need:

- Windows 10/11 x64 and a GPU with Vulkan 1.3.
- An XInput controller, a PlayStation controller (DualShock 4 or DualSense) or a keyboard.
- Your own dump of CUSA14876 v01.00 (eboot SHA256 `ca9c9441…`), optionally with the
  v01.21 update (eboot SHA256 `39d716b7…`, required for 60 FPS and the Pit Stop).

Then:

1. Extract the ZIP to a folder of your choice.
2. Select your dump folders in the launcher, or place them next to it as
   `dump\CUSA14876` (base game) and `dump\CUSA14876-patch` (v01.21 update).
3. Run `launcher.exe`, choose the options and press **PLAY!**

Later versions install from the launcher with **Check updates**. `runtime/user` (saves,
identity, friends, settings) and `runtime/mods` are kept.

## Building from source

Requirements:
- Windows 10/11 x64.
- Visual Studio 2022 **Community** in its default path, with MSVC, MASM and "C++ CMake
  tools for Windows".
- Python 3.12 and Git.

`tools/bootstrap.ps1` fetches the rest:
- the shadPS4 v0.18.0 sources (tag `v.0.18.0`, commit `e3ce810f…`, checked after
  checkout), with their submodules;
- LLVM/clang-cl 19.1.1, verified by size and SHA256 (see
  [`tools/toolchains/README.md`](tools/toolchains/README.md)).

```powershell
git clone https://github.com/guzuold/CTRNativePorted.git
cd CTRNativePorted
powershell -File tools/bootstrap.ps1
powershell -File ctrport/build_release.ps1      # output in build/release-local
```

`build_release.ps1` builds everything in the release layout:
- the audio and font backends and `ctr_video.lib`;
- `ctrport.exe`;
- `ctr_gpu.dll` and `ctr_hle.dll`;
- `ctr_online_server.exe`;
- the launcher.

It also copies the Visual C++ runtime DLLs, the assets and the documents. Start
`build/release-local/launcher.exe` and choose your dump folders as in a release.

Single parts can be rebuilt on their own:
- `ctrport/build_launcher.ps1`: the launcher;
- `ctrport/gpu/build.ps1`: `ctr_gpu.dll` and `ctr_hle.dll`;
- `ctrport/online_server/build_online_server.ps1`: the online service, which also runs
  standalone.

The development scripts `ctrport/build.ps1`, `ctrport/build_profile.ps1` and
`ctrport/video/build.ps1` depend on earlier local builds and on evidence that is not in
this repository. On a fresh clone, use `build_release.ps1`.

Tests:
- `ctrport/test_mods.ps1`: mods and patch formats;
- `ctrport/online_server/server_test.cpp`: the online service;
- `tools/online/`: tests with several separate services.

## How it works

- **`ctrport/`: the runtime and the launcher.** `ctrport.exe` maps the eboot and the game's
  own PRX modules from your dump and resolves imports by NID. It implements:
  - kernel, threads and synchronization;
  - memory and files;
  - save data;
  - audio (XAudio2, ATRAC9) and pad input;
  - the user service and more.

  Every function that is not implemented **fails loudly** with its NID and caller address.
  The only success stubs are online and social services, each listed in
  [`ctrport/compat_stubs.json`](ctrport/compat_stubs.json) and logged on every call.
- **`ctrport/gpu/`: `ctr_gpu.dll`.** The shadPS4 v0.18.0 command processor, shader
  recompiler and Vulkan renderer, compiled in place with shims adapted from the Bloodborne
  Windows port. Provenance is in [`ctrport/gpu/VENDOR.md`](ctrport/gpu/VENDOR.md).
- **`ctrport/settings/`: PC Settings.** The in-game menu (generated from the retail
  menu templates and served as virtual archives), the selector bridge to the shared
  settings file, the live graphics options and the renderer controls they drive.
- **`ctrport/hle/`: `ctr_hle.dll`.** System functions written directly in the game's SysV
  ABI: NP and online, sockets, trophies, save data and dialogs.
- **`ctrport/online_server/`: the online service.** It runs inside the launcher and covers:
  - the PSN and Demonware replacement;
  - the DHT, friends and presence;
  - the signed peer channel;
  - invitations and the matchmaking hub;
  - the Pit Stop.
- **[`ctrport/GUEST_PATCHES.md`](ctrport/GUEST_PATCHES.md):** every in-memory change to the
  original code, bound to the executable's SHA256, checked against the original bytes and
  credited.

## Project status

Runtime milestones, each recorded with logs and screenshots:

| | Milestone | Status |
|---|---|---|
| R0 | Buildable project, documented toolchain | done |
| R1 | Executable and modules loaded, imports resolved by NID | done |
| R2 | Execution through `main`: threads, memory, files | done |
| R3 | Vulkan renderer, first frame | done |
| R4 | Title screen | done |
| R5 | Menus with controller and audio | done |
| R6 | Track loaded | done |
| R7 | Race playable from start to finish | done; the game can be completed |
| R8 | Online between PCs without Activision or Sony servers | two-PC races complete; in progress |

R8 is not declared complete yet. Races between two PCs over the Internet complete, in a
private match and through public matchmaking. Still missing: races with more than two
players, and real implementations for the online compatibility stubs still active.

## Credits and licenses

- Project code: **GPL-2.0** ([`LICENSE`](LICENSE)).
- [shadPS4](https://github.com/shadps4-emu/shadPS4) v0.18.0 (GPL-2.0-or-later): renderer,
  GnmDriver, VideoOut, event queues and public NID names.
- Bloodborne Windows port (bbport, revision `4fcdf770`, GPL-2.0): shims, reviewed per file.
- [FireBurn/FSR-Vulkan](https://github.com/FireBurn/FSR-Vulkan) (MIT, commit `c64f0934`):
  AMD FSR 3.1 for Vulkan, with four patches from GetLostIn/CTR-NitroFueled-PC
  ([`ctrport/gpu/patches/fsr-vulkan`](ctrport/gpu/patches/fsr-vulkan/README.md)).
- [GetLostIn/CTR-NitroFueled-PC](https://github.com/GetLostIn/CTR-NitroFueled-PC)
  (GPL-2.0-or-later): CTR scene-upscaling inputs, DLSS and frame-generation code
  (revisions in `ctrport/gpu/upscaling/SOURCES.json`).
- [jech/dht](https://github.com/jech/dht) (MIT): the Mainline DHT implementation
  ([`ctrport/dht/LICENCE`](ctrport/dht/LICENCE)).
- 60 FPS patch: "60 FPS Unlock" 2.0 by **illusion**, from the
  [GoldHEN Patch Repository](https://github.com/GoldHEN/GoldHEN_Patch_Repository).
- Xbox button icons: Julio Cacko (CC0-1.0).
- Implementation and review assisted by Claude (Anthropic), OpenAI Codex and Gemini.
- Crash Team Racing Nitro-Fueled is a trademark of Activision Publishing, Inc.

Release history and the exact verification behind each release: [`CHANGELOG.md`](CHANGELOG.md).
