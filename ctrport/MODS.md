# Mods and patch files

SPDX-License-Identifier: GPL-2.0-only

The MODS tab of the launcher manages two kinds of mods, in the same formats as SacredFire and
GetLostIn CTR-NitroFueled-PC, so that the same folders and files can be used in both.
Changes apply at the next launch. "Use mods and patches" turns both off at once.

## File mods: `runtime/mods`

- One folder per mod. Inside it are the game's own folders (`archives/`, `icons/`, ...). They can
  also be inside `app0/`, `CUSA14876/` or `CUSA13795/`, or inside one extra wrapper folder (for
  example a ZIP extracted into a folder named after it).
- A mod file replaces the game file with the same path, case-insensitively. A file the game does
  not have is added. Readme files next to the game folders are ignored.
- Load order and switches are kept in `runtime/user/mods.json`:
  `{"order": ["First", "Second"], "disabled": ["First"]}`. When a mod's name is not listed, the
  mod is enabled and appended alphabetically. A mod lower in the list wins over one above it.
- Lookup order of the runtime for `/app0` files: enabled mods, then the update folder, then the
  base game. Game and mod files are never modified.
- The following are refused, both by the launcher (with a dialog) and by the runtime (at
  startup):
  - links or junctions inside a mod;
  - `eboot.bin`, `sce_module` or `sce_sys` in a mod.

  Code changes belong in patch files.
- When a mod replaces `permanent.pak`, `languagestartup.pak` or `update.pak`, the mod's archive is
  used as it is: the colored Xbox icons apply only to the retail archives.
- At startup the runtime log (`.stderr.log`) lists:
  - each mod with its file count;
  - each file overridden between mods (`Mods: path: A -> B`);
  - a total in the form `Mods: N game files replaced, M added`.

## octr-mod mods (`mod.json`)

Most mods for the PC ports on GameBanana are folders with a `mod.json` manifest (schemaVersion 2).
A folder in `runtime/mods` with `mod.json` at its top is applied by the runtime
(`octr_mods.hpp`) while the mod is enabled, in the load order of the other mods:

- `archive_asset` operations change one entry inside a game archive (`archives/<archive>`):
  the entry becomes the mod's `payload` file, or the original entry with the `runs` byte
  changes applied. The entry the game would read must have the manifest's `original_sha256`,
  and the result its `patched_sha256`; otherwise the launch stops and the log names the mod and
  the asset. Mods made for another region or version of the game stop here instead of
  damaging it (for example, assets that differ between CUSA14876 and CUSA13795).
- The archive is not copied: the runtime serves the original file with the replaced entries
  appended after its end, stored uncompressed, and only their 16-byte descriptors redirected
  (the same view the PC Settings menu uses for `update.pak` and `update2.pak`, which it continues).
- The `patches` listed in the manifest are applied while the mod is enabled (all patches in
  those files, with the TitleID and AppVer rules of patch files below).
- The `files` hashes are checked at startup.
- Game folders inside the mod (`archives/`, ...) are loose files as in any other mod; its
  payloads and manifest are not.
- Any other capability (shaders, settings, variants of the `nitro-mod.json` format) or
  operation kind is refused by name, by the launcher when installing and by the runtime.

- A mod is applied whole or not at all. One that does not fit the game is skipped, the game
  starts without it, and `runtime/user/logs/mods_status.json` tells the launcher why; the MODS
  tab shows it in orange.

### Mods made for the other region (US CUSA13795 / EU CUSA14876, same version)

Some mods can be moved onto the other region's game, only with proof:

- **UI assets that differ only in their language tables.** The EU and US games list different
  `.lng` files in an IGZ file's TDEP section (ar/de/es/it against mx/pt). The mod's file gets
  this game's set when (1) this game's asset with the mod's set is byte for byte the mod's
  `original_sha256`, and (2) the port with the mod's set given back is byte for byte its
  payload (`patched_sha256`).
- **Patch files whose every line has `ExpectedSHA256`** (as the OCTR tools write them): each
  write must find its own original bytes, at the same address or once within 64 KB through
  its `Signature`. A moved line must be a single call or jump, whose displacement is
  rewritten. Calls to a moved line's original callee follow it. Any other call must reach a
  function this eboot's own code calls, and any RIP-relative global must be used at least 4
  times by this eboot's code. Without such evidence the whole mod is skipped.

Adapted mods are marked in the MODS tab. Checked on the EU 1.21 dump with the US "Reserves + CNK
Drift Gauge" 1.0.6: 5 race HUDs ported, 7 patch writes found by their original bytes (2 moved by
-0x10, 2 calls retargeted, matching a Ghidra disassembly of the injected code), and a race with
the gauge, the speedometer and the reserves bar on screen.

`ctrport/octr_mods_control.cpp` applies the mods of a folder to the user's own game folders,
read-only, reads every replaced entry back through the view, and plans or ports each mod
patch on the update folder's eboot, listing where every call lands.

## Installing mods from the launcher

The FILE MODS view is one scrolling list:

- a drop zone: `.zip`, `.7z` and `.rar` archives (extracted with the Windows `tar.exe`) and
  folders dropped anywhere on the launcher are installed;
- the installed mods;
- the GameBanana catalog of the PC ports (game 26138; `gamebanana.hpp`), with a page button and
  INSTALL / REMOVE. Downloads are checked against their MD5 on GameBanana.

What a download or a dropped item installs: a `mod.json` folder whole; otherwise its game
folders, or loose `.pak` files into `archives/`, as one mod; patch XML files into
`runtime/patches` (`gb<id>-` prefix for downloads). Links, `eboot.bin`, `sce_module`, `sce_sys`
and manifests the runtime cannot apply are refused before anything is written. What each
download installed is kept in `runtime/user/gamebanana.json`, so REMOVE deletes only that.

## Patch files: `runtime/patches`

- These are XML files in the shadPS4 / GoldHEN format (`<Patch>`, `<TitleID><ID>`,
  `<Metadata Name AppVer AppElf isEnabled>`, `<PatchList><Line Type Address Value/>`).
- The choices are kept in `runtime/user/patches.json` as
  `{"enabled": ["file.xml/Name"], "disabled": ["file.xml/Name"]}`. When a patch is not listed,
  its `isEnabled` attribute decides.
- A selected patch is applied only when all of these hold:
  - its TitleID lists the running game (CUSA14876 or CUSA13795);
  - AppElf is `eboot.bin`;
  - AppVer matches the eboot (`01.00` or `01.21`).

  Patches made only of `mask` lines apply to any AppVer, as in shadPS4. Other patches are
  skipped and logged.
- The values follow the shadPS4 v0.18.0 encodings:
  - `bytes` and `byte`;
  - `bytes16`, `bytes32` and `bytes64`, written little-endian;
  - `float32` and `float64`;
  - `utf8` and `utf16`, written with a terminator;
  - `mask`, a byte pattern where `??` matches any byte, written at the match plus `Offset`.

  `mask_jump32` is not supported.
- An Address includes the 0x400000 eboot base. Every write must lie inside one mapped segment of
  the eboot.
- All selected patches are checked before the first byte is written. If one is invalid, or its
  file cannot be read, the launch stops with the reason.
- The patches are applied after the built-in ones (`GUEST_PATCHES.md`), so a patch file wins where
  both write.

## Checks

`powershell -File ctrport/test_mods.ps1` builds `mods_test.cpp` as C++17 (runtime) and C++20
(launcher) and runs 52 checks on synthetic files, then builds `gamebanana_test.cpp` (launcher) and runs
23 checks of the catalog parsing and the installer. No game files and no network are used.
