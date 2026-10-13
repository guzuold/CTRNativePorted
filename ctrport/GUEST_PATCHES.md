# Patches to original guest code

SPDX-License-Identifier: GPL-2.0-only

ctrport changes the original game code in memory only, never on disk, and only when
the variable named below is set. Each patch is bound to an eboot SHA256 and an offset
in the mapped image, and checks the original bytes first.

| Variable | Eboot | Offset | Original | Patched | Origin |
|---|---|---|---|---|---|
| `CTR_TLS_PATCH=1` | any pinned eboot | every executed `mov rax, fs:[0]` | `64 48 8b 04 25 00 00 00 00` | `65 48 8b 04 25 <TEB slot>` | ctrport (`entry_boundary.hpp`) |
| `CTR_60FPS_PATCH=1` | v01.21, `39d716b70a299b2546bd39996c8592f9375d3f18f9bb8bf11f9139e9af1eb3f5` | `0x13a9d80` | `41 89 86 88 04 00 00` | `41 89 96 88 04 00 00` | third party, see below |
| `CTR_UNLOCK_ALL=1` | v01.00 and v01.21 | data, not code: byte `+0x81` of the current build record | `00` | `01` | ctrport (`unlock_all.hpp`), test only |

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
  `reports/reconstruction/claude/v121_and_60fps_transition.json`.

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
