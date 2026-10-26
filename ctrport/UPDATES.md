# Launcher updates

The launcher checks `https://api.github.com/repos/guzuold/CTRNativePorted/releases?per_page=100`
on portable startup and through **Check updates**. Checks run off the UI thread.
Network failure does not prevent offline use; a manual check displays the error.
The source checkout is never an automatic installation target.

The feed includes prereleases because the project's published builds use that
channel. The highest numeric `vMAJOR.MINOR.PATCH` newer than the compiled version
is offered. Drafts and unsupported tag formats are ignored. An incomplete newest
release is reported rather than silently selecting an older package.

The user chooses whether to download/install or postpone. Installation requires
the game to be closed and Multiplayer Online disabled. During preparation the
window is disabled and the update button reads **Downloading...**. This is an
indeterminate indicator, not a byte-progress meter. The old launcher closes only
after download, checksum verification, extraction and preparation succeed.

## Implementation and trust

`launcher_update.hpp` starts the installed Windows PowerShell 5.1 helper without
a console window. `launcher_update.ps1` provides TLS, GitHub JSON parsing and
.NET ZIP processing. There is no Python, PowerShell 7 or third-party updater
dependency. MSVC builds the launcher as before. PowerShell must be permitted by
the machine's policy; failure is reported, without changing system policy.

Only the exact release asset
`CTRNativePorted-vMAJOR.MINOR.PATCH-win64.zip` under this repository is accepted.
The asset must have a GitHub `sha256:` digest and a valid bounded size. Metadata
is fetched again before download; both size and SHA-256 are checked. Staged files
are hashed again before installation and installed files are checked afterward.

Authenticity currently relies on HTTPS and control of the GitHub repository.
There is **no independently signed update manifest or pinned signing key**.
The SHA-256 check detects mismatched/corrupt downloads, not a compromised release
publisher. No GitHub token, user credentials, saves or game data are uploaded.

ZIP validation rejects traversal, absolute paths, alternate streams, special
files, case-insensitive duplicates, unexpected program paths and oversized
archives. The installer rejects junctions/symlinks in its paths.
The closed program-file list matches the portable release layout. It excludes
`runtime/user`, dumps, mods and configuration; it does not recursively replace
the installation directory or delete unlisted/obsolete files.

## Installation and recovery

Each operation lives in `runtime/user/updates/txn-<random-id>`:

- `package.zip`, `stage/`, and `plan.json`: verified inputs.
- `apply.ps1`: a copy of the *currently installed* helper, so replacement of the
  program's helper cannot interrupt this process.
- `backup/` and `journal.json`: original files, their hashes and transaction state.

The helper waits for the original launcher process to exit, then acquires the
same exclusive `install.lock` used by the launcher. This blocks simultaneous
launchers/installers for this portable installation. Before mutation, it checks
existing files for sharing conflicts and creates all backups. Each file is copied
to a sibling temporary file, flushed, then atomically replaced. The durable
`pending.json` and journal allow rollback after interruption between files.

On success it releases the lock and starts `launcher.exe` visibly. On installation
failure it restores original files and removes newly added files; a verified
rollback allows restart. A failed rollback preserves the pending journal and
reports an error rather than launching a mixed installation.

On the next launcher startup, a pending transaction starts its saved helper and
exits. The helper rolls back an interrupted installation, or clears an already
completed journal, then starts the launcher again. Backups are retained for
inspection/manual recovery. No reboot or administrator elevation is requested.

This is file-installation recovery. Successful process creation does **not**
certify a healthy new build, title screen or playable game; there is no automatic
rollback triggered by a later runtime crash.

## Preparing a release

1. Set `ctrport/launcher_version.txt` to a new three-part version. Do not reuse a
   published version for changed binaries: clients compare versions, not dates.
2. Build with `ctrport/build_launcher.ps1`; `-Version` may override the file for
   isolated test builds. The launcher embeds the version, displays it, and the
   build emits `update-version.json`.
3. In the portable package root, include **all three**:
   `launcher.exe`, `launcher_update.ps1`, and `update-version.json`.
   Include the compatible runtime/DLLs under `build/CTRPort-gpu`, the permitted
   assets, documentation and licenses. The build emits the helper next to the
   launcher; existing external packaging scripts must copy it into the ZIP root.
   If `CTRNativePorted.exe` is included as an alias, build/copy it from the same
   version as `launcher.exe`.
4. Publish tag `v<version>` and asset
   `CTRNativePorted-v<version>-win64.zip` in `guzuold/CTRNativePorted`.
   Wait for the GitHub asset digest to be present before considering it ready.
   No publication is performed by the build or updater.

The first release containing this feature must be installed manually by users
of older launchers. Subsequent releases can use the in-app update path.
This updater is included in the refreshed v0.4.0 package (2026-10-20). Even users already on v0.4.0 must download this refreshed package manually once. Future releases must use a higher version number.

### New program files

An installed launcher applies a package with its *installed* helper, so a path that
helper's closed list lacks makes it refuse the whole ZIP. A release that adds program
files therefore goes out in two steps:

1. A release built with `build_release.ps1 -WithoutDirect3D11` (the files it would add
   are left out) whose `launcher_update.ps1` lists the new paths. Installed launchers
   accept it and install that helper.
2. The next release, built normally, with the new files. Its tag carries the helper
   generation it needs, `vX.Y.Z-uN` (asset `CTRNativePorted-vX.Y.Z-uN-win64.zip`), and
   `$UpdaterGeneration` in `launcher_update.ps1` is raised to N in the step release.
   Helpers offer the highest release they can parse whose generation they reach; helpers of
   0.6.1 and older parse only `vX.Y.Z`, so they offer the step release instead of a
   package they would refuse.

The Direct3D 11 renderer (`build/CTRPort-gpu/ctr_d3d.dll`, `ctr_shaders.exe`) is the
first such case: 0.6.1 and older helpers refuse it. v0.6.2 is its step release (generation
2) and v0.7.0 is tagged `v0.7.0-u2`. A launcher without `ctr_d3d.dll`
next to the runtime keeps the Vulkan renderer and disables the renderer choice.

## Repeatable checks

Run with Windows PowerShell 5.1:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ctrport/test_launcher_update.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ctrport/test_launcher_update_process.ps1
```

Tests create unique folders under `runtime/user`, use synthetic program files and
do not execute retail code. The filesystem suite covers numeric version ordering,
prereleases, equal/older versions, invalid metadata, ZIP restrictions, checksums,
file sharing, rollback, recovery and user-data preservation. Its junction test
needs permission to create a junction inside its own fixture folder.
The process suite uses small synthetic Windows executables to verify parent-exit
waiting, exclusive-lock release, installation, actual restart and journal cleanup.
