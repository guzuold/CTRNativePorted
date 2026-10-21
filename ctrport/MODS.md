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
(launcher) and runs 49 checks on synthetic files. No game files are used.
