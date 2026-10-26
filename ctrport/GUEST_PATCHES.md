# Patches to original guest code

SPDX-License-Identifier: GPL-2.0-only

ctrport changes the original game code in memory only, never on disk, and only when
the variable named below is set. Each patch is bound to an eboot SHA256 and an offset
in the mapped image, and checks the original bytes first.

| Variable | Eboot | Offset | Original | Patched | Origin |
|---|---|---|---|---|---|
| `CTR_TLS_PATCH=1` | any pinned eboot | every executed `mov rax, fs:[0]` | `64 48 8b 04 25 00 00 00 00` | `65 48 8b 04 25 <TEB slot>` | ctrport (`entry_boundary.hpp`) |
| `CTR_60FPS_PATCH=1` | v01.21, `39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5` | `0x13a9d80` | `41 89 86 88 04 00 00` | `41 89 96 88 04 00 00` | third party, see below |
| `CTR_NATIVE_4K=1` | v01.00 (`ca9c9441...`) and v01.21 (`39d716b7...`) | v01.00 `0x12e74c2`, `0x1aacfcb`; v01.21 `0x13aa652`, `0x1bbb918` | `41 8b 44 24 48` and `c7 45 c8 00 00 00 3c` | `b8 00 00 00 40` and `c7 45 c8 00 00 00 78` | ctrport (`entry_boundary.hpp`, `direct_memory.hpp`) |
| `CTR_UNLOCK_ALL=1` | v01.00 and v01.21 | data, not code: byte `+0x81` of the current build record | `00` | `01` | ctrport (`unlock_all.hpp`), test only |
| `CTR_ONLINE=1` | v01.00 (`ca9c9441...`) and v01.21 (`39d716b7...`) | data: v01.00 `0x1ade440`, v01.21 `0x1becf40`, 0x126 bytes | Demonware auth3 traffic key (SHA-256 `356c4bc3...`) | project server public key (`CTR_ONLINE_TRAFFIC_KEY`) | ctrport (`online_traffic_key.hpp`), see below |

The offsets of every supported eboot are in `retail_profiles.hpp`. The US CUSA13795 eboots
(v01.00 `23ddf390...`, v01.21 `e3b7a068...`) have the same patches at shifted offsets (60 FPS
v01.21 `0x13a9d90`; native 4K v01.00 `0x12e74b2`, `0x1aacfbb`, v01.21 `0x13aa662`, `0x1bbb938`;
online key v01.21 `0x1becf80`, v01.00 as EU), derived from the user's dump by comparing the
original bytes; not yet run in this runtime.

## CTR_TLS_PATCH

Written for ctrport. After the exception handler has emulated a guest `mov rax, fs:[0]`,
it rewrites that instruction to read the guest thread pointer from a Windows TEB TLS
slot (`tls_storage.hpp`), so later executions no longer fault. Same length, same result.

## CTR_60FPS_PATCH

- Source: GoldHEN_Patch_Repository (https://github.com/GoldHEN/GoldHEN_Patch_Repository),
  `patches/xml/CrashTeamRacingEU-Orbis.xml` and `CrashTeamRacingUS-Orbis.xml`, entry
  "60 FPS Unlock", author illusion, PatchVer 2.0, AppVer 01.21, AppElf eboot.bin,
  note "Version 2.0 now removes double buffer vsync". Consulted 2026-10-13.
- The repository address is `0x017a9d80`. GoldHEN addresses include a 0x400000 base,
  so the image offset is `0x13a9d80`. The original bytes there were checked in the
  v01.21 eboot.
- License: the repository declares none (GitHub reports no license). The patch is
  seven bytes and is not copied code, but it is the author's work: it stays credited
  here, in `entry_boundary.hpp` and in the launcher, and is disabled unless requested.
- What it changes (llvm-mc disassembly of the v01.21 eboot):
  `mov eax, [r12+0x18]` loads a configuration value, then the original
  `mov [r14+0x488], eax` stores it. The patch stores `edx` instead. No instruction
  in this part of the function sets `edx`: it holds what the preceding call left. The
  effect (60 flips per second instead of 30) is observed, not derived.
- No v01.00 equivalent exists, so 60 FPS is available only with the v01.21 update.
- The runtime does not change `sceVideoOutSetFlipRate`; the patch alone gives 60 FPS,
  as on the console.
- Measured with `tools/profiler/speed_probe.py`, without input: 60.0 flips/s, and the
  intro and title sequence changes scene at the same wall-clock times as at 30 FPS
  (same draws per frame, twice the frames), so the game runs in real time. Race
  physics at 60 FPS were not measured. Details in
  (development notes).

## CTR_NATIVE_4K

### Configurable scene resolution: CTR_SCENE_HEIGHT

`CTR_SCENE_HEIGHT=720|900|1080|1260|1440|1800|2160` generalizes the same verified
Neo scale site below. The launcher exposes it as **Internal 3D resolution**;
`play_gpu.py --internal-res 720 --res 1920 1080` selects a 1280x720 scene in a
1920x1080 host window. Changes apply on the next game launch. Invalid values
fail, rather than silently falling back. Explicit scene height takes precedence
over the legacy Pro/4K switches; absent the new option, their behavior is preserved.

The explicit option enables Neo and keeps the guest VideoOut/UI canvas at
3840x2160 so the original game's Neo scale path runs independently of the host
window/fullscreen size. The patched float is `height / 1080.0f`, giving 16:9
scene targets from 1280x720 through 3840x2160. This does not reduce every UI,
shadow or post-processing pass to the scene resolution. Above 1440p, the existing
1920 MiB Garlic pool and expanded direct-memory budget are used; lower presets
retain the 960 MiB Neo pool. The SHA-selected addresses and original-byte checks
below still apply. No retail file on disk is changed.

`CTR_SCENE_TRACE=1` enables bounded, deduplicated diagnostics in the Vulkan
rasterizer: `CTR_SCENE_TARGET color=WxH depth=WxH render=WxH` records the actual
attachments of depth-tested draws. `tools/profiler/scene_resolution_probe.py`
compares these with each preset while measuring the host window's client size,
captures the game, and writes hashes/logs under `runtime/user`. Tests run offline
in isolated save directories. A configured scale or screenshot size alone is
not accepted as evidence that scene resolution changed.

Written for ctrport. Raises the internal 3D scene rendering resolution in PS4 Pro mode
(`CTR_NEO_MODE=1`) from `2560x1440` (scale `1.3333334f` relative to `1920x1080`) to native
4K `3840x2160` (scale `2.0f` relative to `1920x1080`), matching the `3840x2160` VideoOut
buffer and 2D/UI buffer. Use it from `play_gpu.py --native-4k` or the launcher toggle
(`3D 4K nativo`).

- Why two sites are patched (retail disassembly of both eboots):
  1. **Neo 3D scene scale factor (`0x12e74c2` in v01.00, `0x13aa652` in v01.21):**
     When `sceKernelIsNeoMode()` returns 1 and `sceVideoOutGetResolutionStatus` reports
     `paneHeight >= 1081` (`3840x2160`), the display setup function sets the 2D/UI target
     dimensions (`[r14+0x490..0x49c]`) to `3840x2160` (`0xf00 x 0x870`) and loads the Neo
     3D scene scale factor from `[r12+0x48]` (`0x3faaaaab` = `1.3333334f`) into `[r14+0x548]`,
     which sizes the 3D scene render targets to `2560x1440`. Replacing the 5-byte load
     `41 8b 44 24 48` (`mov eax, [r12+0x48]`) with `b8 00 00 00 40` (`mov eax, 0x40000000`
     = `2.0f`) after verifying the 12-byte instruction pair `41 8b 44 24 48 41 89 86 48 05 00 00`
     makes the 3D scene render targets `3840x2160`.
  2. **Neo `Gfx/@vramBSize` / `[1]{7,Garlic}` pool size (`0x1aacfcb` in v01.00, `0x1bbb918` in v01.21):**
     At `3840x2160`, the 3D scene color, depth, shadow, and post-processing targets use
     2.25x the VRAM of `2560x1440`, exhausting the retail Neo `[1]{7,Garlic}` pool (`960 MiB`
     = `0x3c000000`) when the 3D title scene loads (`Out of memory allocating from memory pool
     "[1]{7,Garlic}"`, probe `20261013_033457_native4k_v121`). In the startup memory
     configuration function (`0x1aac7df` in v01.00, `0x1bbb123` in v01.21), when
     `sceKernelIsNeoMode()` is non-zero and no XML override is present for `"Gfx/@vramBSize"`,
     execution branches to `0x1aacfcb` (v01.00) / `0x1bbb918` (v01.21) and executes
     `c7 45 c8 00 00 00 3c` (`mov dword ptr [rbp-0x38], 0x3c000000`), which is then
     sign-extended via `movsxd rdx, dword ptr [rbp-0x38]` and passed to the direct-memory
     allocator (`0x1437c20` in v01.00, `0x1500320` in v01.21) for type 3 (`WC_GARLIC`).
     Patching the immediate to `0x78000000` (`1920 MiB` = `2,013,265,920` bytes, bit 31 = 0
     so `movsxd` sign-extends cleanly to `+0x78000000`) after verifying 8 original bytes
     `c7 45 c8 00 00 00 3c e9`, together with expanding `DirectMemoryBacking` by `+960 MiB`
     (`6336 MiB` total when `CTR_NATIVE_4K=1`), doubles the `[1]{7,Garlic}` pool to `1920 MiB`.
- Observed on v01.21 (`20261013_035006_native4k_v121`): `guest_native_4k_patch_applied`
  logged (`scale_target: 20620882`, `vram_b_target: 29079832`, `scene_scale: 2.0`,
  `garlic_bytes: 2013265920`), type-3 direct memory allocated and mapped at `2,013,265,920`
  bytes, and the 3D title scene rendered at a steady 30.0 flips/s (~4650 draws/s) with zero
  pool OOM errors.

## CTR_UNLOCK_ALL

Test-only switch, written for ctrport: every unlockable reports "unlocked" while the run
lasts. It changes no code and no file; it sets one byte of game data in memory. Use it from
`play_gpu.py --unlock-all` or the launcher checkbox. Both first copy
`runtime/user/savedata` to `runtime/user/savedata_backups/before_unlock_all_<time>`,
because the game keeps saving during the run.

- What the retail code does (capstone disassembly of both eboots): every unlockable check
  calls the same lazy singleton getter, follows `[singleton+0x10]` (an engine handle) to
  `[handle+0x28]` (the record) and returns "unlocked" when the byte at `record+0x81` is
  non-zero, ORed with the item's own condition. The callers are the `isUnlocked`
  overrides of the `COctaneUnlockable` subclasses (vtable slot `0xb0`) and of the
  `CProgressionChallenge` subclasses. No caller uses the byte for anything else.

  | Eboot | Getter | Singleton pointer | Readers of `+0x81` |
  |---|---|---|---|
  | v01.00 `ca9c9441...` | `0x9de850` | `0x26b1320` | 10 |
  | v01.21 `39d716b7...` | `0x9f0cf0` | `0x2ab1bd0` | 25 |

- Names: retail reflection strings list `CBuildsSetupData` (`_currentBuild`, `_builds`)
  and `CBuildData` with `... _aiPreset, _saveLoadEnabled, _unlockAll,
  _restrictedCustomization ...`; the record is a `CBuildData` and `+0x81` follows
  `_saveLoadEnabled` at `+0x80`, so it is read as `_unlockAll`. The name is inferred from
  field order; the behaviour above is what the switch relies on.
- Binding: before starting, `unlock_all.hpp` checks the getter bytes
  (`48 8b 05 <rel32> 48 85 c0 74 01 c3`, displacement resolving to the singleton pointer)
  and one reader (`80 bb 81 00 00 00 00`, v01.00 `0xf3cba8`, v01.21 `0xe6b342`) and stops
  the runtime if they differ.
- Runtime: a host thread polls every 250 ms with `ReadProcessMemory`, follows only direct
  handles (redirected/aliased handles are logged, not followed) and writes `01` when the
  byte is `00`. Log lines: `guest_unlock_all_armed`, `guest_unlock_all_set` (record
  address, the `+0x80` byte, write count), `guest_unlock_all_handle_indirect`.
- Observed on v01.21 (`play_20261013_013241`): record found, byte `+0x80` = 1, one write;
  the user confirmed on screen that every unlockable was available. In that run the game
  also unlocked trophy 46 in `runtime/user/trophies/unlocks.jsonl` and rewrote the global
  save. v01.00 is bound statically but has not been run with the switch.

## CTR_ONLINE (auth3 traffic key)

Written for ctrport (R8, (development notes)). The game's Demonware client
accepts an auth3 login reply only if its `X-Signature` header verifies (RSA-PSS, SHA-256,
salt length 0) against an RSA-2048 public key stored in the eboot, the only
SubjectPublicKeyInfo in the image. The project server cannot sign with that key. With
`CTR_ONLINE=1` the runtime replaces those 0x126 bytes in memory with the project server's
public key (downloaded by `play_gpu.py --online` from `/ctr/v1/traffic-key` into
`runtime/user/online/`), after checking the original bytes by SHA-256 so that no game data
is stored in the repository. The verification code itself is unchanged. Without
`CTR_ONLINE` nothing is patched.
