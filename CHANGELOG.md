## v0.7.0 — 2026-10-26

A second renderer, Direct3D 11, that does not use shadPS4; upscaling quality presets; sharper shadow maps.

- **Direct3D 11 renderer:** choose it in launcher **Settings → Renderer**. Vulkan (shadPS4) stays the default. It draws from the game engine's own render state with shaders translated from your own dump; no game file is included. The first time you play with Direct3D 11 the launcher offers to translate the shaders (`ctr_shaders.exe`): about six minutes once on a 20-thread CPU, a few seconds afterwards. Tested on 1.00 and 1.21: title, menus, adventure hub and races, at 60 FPS with PS4 Pro mode.
- **PC Settings with Direct3D 11:** vertical sync, frame rate limit, window modes, exclusive fullscreen refresh rate, texture filtering, display gamma, ambient occlusion off, hero shadows off, AMD FSR 1 (sharpness changes at once) and native HDR10 with peak and exposure. HDR10 needs a display in Windows HDR mode; it has not been tested on one yet. AMD FSR 3 and DLSS stay Vulkan only: with Direct3D 11 the launcher asks for Off or FSR 1.
- **Upscaling quality presets** (both renderers): the render resolution choices carry the usual names: Ultra performance 720p, Performance 1080p, Balanced 1260p (new), Quality 1440p, Native 2160p; 900p and 1800p remain. With a window smaller than 4K, Native renders more pixels than the window and acts as supersampling.
- **Shadow map detail** (Direct3D 11, Shadows and effects page): High creates the shadow maps at twice the PS4 size.
- **Not yet with Direct3D 11:** PlayStation controllers through SDL (Xbox/XInput controllers work); split screen and the game's compute passes are untested. Use Vulkan for them.
- **How to update:** from v0.6.1 or older, install v0.6.2 first with **Check updates**, then check again for v0.7.0. The 0.6.1 updater cannot install the new renderer files directly. Saves, settings and mods are preserved.

## v0.6.2 — 2026-10-26

Step release before v0.7.0: the updater learns the program files of the new Direct3D 11 renderer.

- **Why this release exists:** an installed launcher installs updates with its own update helper, which accepts only a closed list of program files. The 0.6.1 helper does not know `ctr_d3d.dll` and `ctr_shaders.exe`, so it would refuse the whole v0.7.0 package. v0.6.2 installs the helper that knows them; **Check updates** then offers v0.7.0.
- Releases that need this helper are tagged `vX.Y.Z-uN` (v0.7.0 is `v0.7.0-u2`). Older helpers skip such tags, so they offer v0.6.2 first instead of a package they would refuse.
- Nothing else changes: the runtime, the GPU library and the launcher features are those of v0.6.1. Saves, settings and mods are preserved.

## v0.6.1 — 2026-10-25

Two launcher skins, with Nitro Cinema selected by default, retain the v0.6.0 runtime and in-game PC Settings.

- Horizontal navigation, antialiased text and controls, and the original artwork read from your own dump with a bottom-centered crop that hides its embedded logo.
- **Switch skins in Advanced:** click the Appearance button to switch immediately between Nitro Cinema and the original Classic launcher. Your choice is saved, and both skins share the same settings, saves, mods and launcher actions.
- Nitro Cinema has animated tab transitions and a Friends panel, with cached rendering to reduce click delay and prevent controls flashing during transitions. Animations can be disabled in Advanced; tab transitions also respect Windows' client-area animation setting.
- Corrected circular arrows beside Updates. Existing launch, mods, trophies, options and update actions are retained.
- Update through Check updates with the game closed. The package preserves saves and settings under `runtime/user` and mods under `runtime/mods`. No game files, background artwork, trophy keys or user data are included.
- Runtime binaries are unchanged from v0.6.0; this release adds no runtime or reconstruction milestone.

## v0.6.0 — 2026-10-25

PC Settings inside the game's own Options menu, a launcher that no longer crashes on close, less CPU spent on logging, and logs with a size limit.

- **PC Settings in the game menu:** Options has a new PC Settings entry with six native pages (Display, Graphics, Resolution and upscaling, Quality, Shadows and effects, Quality presets), in the game's language. The Graphics page (anti-aliasing FXAA/off/SMAA, bloom, motion blur, depth of field, fur detail, view distance) applies at once; settings marked "(restart)" apply at the next start. Saving runs in the background and does not slow the menu. See PC_SETTINGS.md, which also gives the command that restores the graphics settings if the game no longer starts. (The first v0.6.0 package also contained `Reset-PC-Graphics.cmd`, which the 0.5.9 updater refuses as an unmanaged path; the package was replaced without it.)
- **Built from your own game:** the menu is generated in memory at startup from your retail 01.21 archives and served to the game with them; no game data is distributed and no archive is copied or changed. With another game version, or a mod that replaces those archives, the menu is left out and the game runs unchanged. `CTR_PC_MENU=0` turns it off.
- **Launcher crash on close fixed:** closing the launcher crashed (access violation) because the trophy icons were released after the graphics library had shut down. They are released first now.
- **Less CPU on logging:** per-call lines of the busiest services (mutexes, clock, waits, pad and file polling) are written only with `CTR_TRACE_HOT=1`. On the title screen the game used about 12% less CPU; errors and all other logs are unchanged.
- **Logs with a size limit:** the launcher now reads the game's output itself and keeps, per stream, the first 32 MB and the last 16 MB of a session with a line saying how much was left out. Only the 8 most recent game sessions are kept in `runtime/user/logs`; older `play_*` logs are deleted at the next start. Before, single logs could reach several GB.

## v0.5.9 — 2026-10-21

Leaving the matchmaking screen frees the player, and a new host takes over the lobby.

- **Leaving the matchmaking screen:** the game's leave-lobby (145/10) and cancel-search (145/8) tasks were answered "not served", and from a PC using a hub they were not even forwarded. A player who left stayed in the lobby for the hub; when it was the host, the other player promoted itself in game and waited forever on "Preparazione lobby in corso" for the hub to confirm. Now the hub removes the player from the lobby and cancels its search. When the host leaves, the next connected member becomes host and is sent the updated lobby. Players who left get no more updates of that lobby and can search again at once.

## v0.5.8 — 2026-10-21

The two games of an online match can connect again: answers to the game's own datagrams are no longer dropped.

- **Answers to the game's own datagrams are accepted at once.** Since v0.5.5 the incoming filter dropped the first datagram from an address until the service confirmed it, and an authorization without traffic lasted three seconds. A connection handshake answers once and retries only seconds later, so in the v0.5.7 two-PC test the answers kept arriving to an expired or pending check and the two games never connected: the host's log shows the guest's handshake datagram blocked, then accepted and answered, and the guest never continuing. Now when the game sends to an address the service authorizes, that address is authorized for incoming datagrams too, as a firewall accepts replies to connections it opened. Addresses the game has not written to are still checked first.

## v0.5.7 — 2026-10-21

A player who can be reached hosts public lobbies; hold Esc to quit.

- **Public matchmaking host:** when the hub forms a lobby it picks as host the first searcher whose PC accepts connections from the Internet. The hub's own players count as such; each other service now says so with the tasks it forwards. Only when nobody can be reached does the first searcher host, as before. In the v0.5.6 two-PC test the PC on a phone hotspot searched first and hosted, advertising an address of its operator's network (10.x) that the other game could not reach.
- **Esc to quit:** holding Esc for two seconds with the game window in front closes the game, as the window's close button does.
- Services of earlier versions do not report whether they can be reached, so their players never host a lobby formed by a newer hub.

## v0.5.6 — 2026-10-21

Public matchmaking from PCs that cannot be reached: no dead local queue, and the hub election in the log.

- A PC that cannot be reached from the Internet (behind the operator's NAT, a phone hotspot) and has not found a matchmaking hub refuses the public search instead of queueing it on its own service, where no other player could ever join. The game receives an error for the search; the next attempt starts over and uses the hub if one has been found. The refusal is logged as `hub_none` with reason `search_refused`. How the game shows the refusal still has to be checked in a real session: on 19 October 2026 the game survived errors on these tasks, gave up the search after 60 seconds and went on.
- Every hub election round writes a `hub_round` line to the service log: addresses returned by the DHT, identities that answered, live candidates, whether this PC can be reached, and the elected hub. A PC that never finds a hub now shows it directly.

## v0.5.5 — 2026-10-21

Security of the online service and more reliable public matchmaking.

- Filter incoming game UDP and TCP accepts using bounded asynchronous peer authorization; preserve relay delivery and discovery mapping.
- Authorized peers are renewed every second ahead of unknown sources and, while they keep sending, stay accepted during the renewal: no race datagram is lost to renewals (an earlier build of this filter lost 1–10% per peer, more with several opponents). A refusal takes effect at the service's answer; with the service unreachable an authorization ends three seconds after the last confirmation. A blocking receive skips refused datagrams instead of failing with EWOULDBLOCK.
- Bound JSON nesting to 64 containers and decode BLOB/STRUCT lengths without recursion.
- Restrict local game HTTP routes to loopback while preserving signed peer/party and identity flows.
- Apply absolute five-second HTTP header/body deadlines and 128 global and 8 per-IP TCP connection limits for remote sources. The local game (127.0.0.1) has its own quota of 64, so remote connections cannot cut it off from its service. The loopback-only lobby has no idle timeout.
- Stop requesting TCP 3074 through UPnP; bind the lobby and diagnostic TCP 3075 / UDP 3075, 3478, 3479 to loopback.
- Compatibility: signed service and game wire formats are unchanged, so mixed v0.5.4/current per-PC launcher peers remain protocol-compatible (two-PC play still needs manual verification). Manually pointing a game at a LAN/remote CTR_ONLINE_SERVER is intentionally no longer supported. Unusually deep payloads, slow HTTP clients and connections over the limits are now rejected.
- Fix invitation ownership validation to use the registered session member name with the service-scoped account, preserving rejection of spoofed senders.
- Keep directly verified public matchmaking hubs across intermittent DHT searches and launcher restarts; reverify saved identities before use, expire after three failed rounds or five minutes, and elect the lowest verified code.
- Start election immediately, deduplicate DHT candidates by IP, and wake election for an unreachable player's public task with a one-second bounded wait. Log `hub_elected`, `hub_lost` and `hub_none` in the service log.
- A public matchmaking sequence stays on the side that answered its advertise: a hub elected halfway through no longer receives a search for a player it never registered (which it refused). A new advertise chooses again.
- Document external reachability confirmation as a proposal; mixed v0.5.4/current signed matchmaking tasks are covered by local protocol tests. Real two-PC/CGNAT play still needs the manual check in docs/security/hub-election.md.

## v0.5.4 — 2026-10-21

AMD FSR 3.1 upscaling, and friends keep their names while offline.

- **AMD FSR 3.1** in Settings → Scene upscaling ("AMD FSR 3.1"; `CTR_UPSCALER=fsr3`).
  - Temporal upscaling through FireBurn/FSR-Vulkan (MIT), fed with the same scene depth, motion and sub-pixel jitter as DLSS, before the HUD. It runs on any Vulkan GPU with the needed features (no RTX needed).
  - Sharpening 0.3 by default; `CTR_FSR3_SHARPNESS` (0–1) changes it.
  - The device enables `shaderStorageImageWriteWithoutFormat`, which FSR 3 needs.
  - FSR-Vulkan is pinned by `tools/bootstrap.ps1` and built by `ctrport/gpu/build.ps1`. Four patches from GetLostIn's port are applied to it; one stops the FSR 3 upscaler from failing with "out of memory" when the memory visible to both CPU and GPU is full (no Resizable BAR).
- **Friend list:** a friend who is offline keeps the last known name instead of becoming a generic "Friend"; the launcher reads it from `runtime/user/online/friend_names.json`. A default nickname is replaced by the real name as soon as it is known.
- Launcher friend status texts: "Verified service" and "Never contacted".
- AMD FSR 3.1.6 frame generation is in the renderer but set aside: it has no setting in this release, because moving images left visible trails in testing. `CTR_FRAMEGEN=1` turns it on for development only.

Validation:
- FSR 3.1 on retail 1.21: 1280×720 → 3840×2160 at the title with valid camera planes; the user confirmed no flicker after the dispatch fix.
- The build was made from a fresh clone of this version, FSR-Vulkan included.

Not tested: FSR 3.1 in a full race, and the friend-name fix on two PCs.

Update with Check updates. `runtime/user` and `runtime/mods` are kept.

## v0.5.3 — 2026-10-21

Trophies, and a controller for each player.

- **Trophies:** the game's 48 trophies now unlock.
  - Earlier versions reported no trophies to the game, which then repeated the same unlock hundreds of times.
  - Unlocks are kept in `runtime/user/trophies/<NP communication ID>.txt` and reported back to the game. Unlocks recorded by earlier versions are imported.
  - Each unlock shows a notice over the game for five seconds, with the trophy icon. The platinum unlocks with the last base trophy.
- **TROPHIES tab** in the launcher:
  - every trophy icon, in color when unlocked and gray when locked, with the progress;
  - name, grade and unlock date on hover;
  - an optional field for your own PS4 trophy key, or IMPORT FROM shadPS4 (`keys.json`). With the key, names and descriptions are shown in the game language; without it, trophies are shown by number.
- **Controllers in Settings:**
  - the launcher detects the connected controllers and their type: Xbox controller on its XInput slot, DualShock 4, DualSense or another gamepad;
  - each of players 1–4 has a menu: Auto, None, Keyboard or a controller;
  - a controller fixed to a player is used only by that player and logs in when connected;
  - assigning a controller to players 2–4 turns split screen on;
  - players on Auto join by holding a button, as before;
  - the list refreshes when a device is connected or removed.
- `ctr_gpu.dll` gives the runtime the device path of each PlayStation controller (`ctr_gpu_gamepad_path`), so that the launcher's choice finds the same controller.
- Launcher: the launcher version moved below Check updates, and a German Settings label was shortened.

Validation:
- The split screen control passes with new cases for fixed players, "None" and late connection. The mods and invitation tests pass.
- The trophy file of the dump is read (48 trophies, icons), and a wrong key is rejected.
- Launcher screens were checked by capture in Italian, German and French.

Not tested in game: the trophy notice over the game, names with a real trophy key, and the controller assignment with real controllers in a race.

Update with Check updates. `runtime/user` and `runtime/mods` are kept.

## v0.5.2 — 2026-10-21

Players with the same name no longer collide online.

- Until v0.5.1 the account the game uses for a player was computed from the name alone, so two players with the same name (for example the default "CTRPlayer") had the same account on every PC. In public matchmaking the second one was refused by the hub; an invitation or JOIN between two players with the same name was refused as "Player on this PC"; in a private match one member replaced the other.
- The account now comes from the player's name together with the friend code of the launcher that hosts the player. Each launcher passes it to its game (`CTR_ONLINE_ACCOUNT`); friends, invitations, sessions, the matchmaking hub and the in-game friend list use the same account.
- The Pit Stop wallet of the old name-only account moves to the new account the first time the game connects, unless the new account already has a wallet.
- The last Italian message of the matchmaking hub ("Hub full") is in English.

All players should update together: a v0.5.1 player and a v0.5.2 player see different accounts for each other, so friends' status, invitations and matchmaking between the two versions can fail.

Validation: online service suite with 0 failures, including the signed wallet backup with the new accounts. Not tested in game: the same-name case and the wallet move are to be checked on the PCs.

Update with Check updates. `runtime/user` and `runtime/mods` are kept.

## v0.5.1 — 2026-10-21

The launcher in English that follows the game language, and online races between two PCs confirmed.

- **Online:** races between two separate PCs over the Internet complete, both in a private match with a friend and through public matchmaking (tested by the project owner on v0.5.0). Races with more than two players are not tested yet.
- **Launcher language:** every text is in English and follows the game language chosen in Settings (French, Italian, German or Spanish) at once, without a restart. Arabic shows English. The language list shows each language in its own name.
- **Fewer notes:** explanations and instructions were removed from Settings, the friends drawer and the MODS tab; the remaining messages are shorter.
- **In-game invitation windows** (picker, prompts and errors) use the game language.
- **English messages** from the online service and the updater, which reach the launcher's notices and the logs.
- **README:** a complete list of the features and of the serverless peer-to-peer online design.

Validation: online service suite with 0 failures; invitation window test with 0 failures; mods test unchanged. Launcher, online service and `ctr_hle.dll` are built from a fresh clone of this version. Translated layouts were not checked on screen: a long translation may not fit a button. `ctrport.exe` and `ctr_gpu.dll` are unchanged from v0.5.0.

Update with Check updates. `runtime/user` and `runtime/mods` are kept.

## v0.5.0 — 2026-10-21

Mods and patch files in the MODS tab, and a build from the public sources alone.

- **File mods:** one folder per mod in `runtime/mods`, holding the game's own folders (`archives/`, `icons/`, ...), optionally inside `app0/`, `CUSA14876/`, `CUSA13795/` or one extra wrapper folder.
  - Load order and switches are kept in `runtime/user/mods.json` (`{"order": [...], "disabled": [...]}`); a mod lower in the list wins.
  - The runtime reads `/app0` files from the enabled mods first, then from the update and the base game. Game files are never modified.
  - Links, and `eboot.bin`, `sce_module` or `sce_sys` inside a mod, are refused with a message.
- **Patch files:** shadPS4 / GoldHEN XML files in `runtime/patches`, with choices in `runtime/user/patches.json` (`"file.xml/Name"`).
  - A patch applies only when its TitleID, AppElf and AppVer match the running eboot.
  - Values and the `mask` pattern search follow shadPS4 v0.18.0. Every write must lie inside the eboot's mapped segments; `mask_jump32` is not supported.
  - All selected patches are checked before any byte is written. A problem stops the launch with the reason, and skipped patches are logged.
- **Same formats as SacredFire / GetLostIn CTR-NitroFueled-PC**, so the same mod folders and patch files work in both.
- **MODS tab:** two views, File mods and Patches. File mods have switches and up/down order; the Patches view marks patches that do not match your game version. A "Use mods and patches" switch turns everything off at once.
- **Public build:** `ctrport/build_release.ps1` builds the whole program in the release layout from a fresh clone after `tools/bootstrap.ps1`. `ctrport/original_call_session.hpp`, already compiled into `ctrport.exe`, is now in the repository. The audio and font backend scripts create their output folder.

Validation:
- 49 checks of the mod and patch formats, built both as C++17 (runtime) and as C++20 (launcher), on synthetic files.
- A retail run with a local test mod and patch file: the log reports 2 game files replaced, the 01.21 patch applied and the 01.00 patch skipped, and the game read the mod's archives up to the start screen. The changed graphics were not checked on screen.
- A fresh clone was built with `build_release.ps1`.

Update with Check updates. `runtime/mods` and `runtime/user` are kept.

## v0.4.4 — 2026-10-20

Fix JOIN from the in-game friends list across separate launchers.

- JOIN reads the friend's Delta record (138/14) and contacts the friend immediately, before any session request names the friend's session. v0.4.3 therefore refused those packets (`blocked_by_policy`, peer-allowed 403 right after 138/14 in the two-PC log), and the join ended with an error.
- The joining player's service now remembers, for two minutes, the friends whose Delta record its game received, and asks their service as a joining friend. The host's service admits that verified friend for one minute and answers with its own players' addresses, so both directions are authorized.
- Only mutual, verified friends who are in game are admitted this way. Strangers and unknown addresses stay blocked; public matchmaking authorization is unchanged.

Validation: two separate services with encrypted synthetic game clients, 16 checks; v0.4.3 fails the new Delta check, v0.4.4 passes. Server suite 237, friend network 34, public hub, invitation and room tests pass. These are local protocol tests: a retail race over the Internet, NAT/UDP reachability and relay remain to be verified on the two PCs.

Update both PCs, close the game before installation, then try JOIN from the friends list in both directions. Keep `runtime/user` and the existing identities. See FIRST_UPDATE.md.

## v0.4.3 — 2026-10-20

Fix gameplay packets rejected as `blocked_by_policy` when joining across separate launchers.

- Authorize actual assigned matchmaking lobby peers, including players who are not friends. Sharing the public matchmaking queue alone does not authorize a destination.
- A member service queries its selected matchmaking hub over the signed peer channel. The hub checks the caller's identity and uses observed source addresses, including address changes.
- For invitations/session joins, authorize the connection before retail posts session membership. A readable, unlocked session grants a 30-second provisional connection; membership keeps it valid. Leaving or closing the session revokes it, and cancelling an uncompleted join no longer reports an invalid-member error.
- Query the session's owning service for cross-PC session authorization. Friendship alone does not grant gameplay access. Failed authority queries stay denied.
- Expire the runtime's positive authorization cache after one second (previously ten minutes), and its negative cache after 250 ms. Matchmaking departure/reassignment does not retain the previous lobby's authorization.

Validation: 15 session and signed multi-service matchmaking checks, 4 runtime policy/cache checks, 34 existing peer/invitation checks, and 10 checks using two separate services with encrypted synthetic game clients. The last scenario reproduces the blocked connection on v0.4.2 and passes on v0.4.3. These are local protocol tests, not a retail Internet race or proof of NAT/UDP reachability. Relay behavior is unchanged.

Update both PCs, including `ctr_hle.dll`, close the game before installation, then test both invitation directions and public matchmaking. Keep `runtime/user` and the existing identities. See FIRST_UPDATE.md.

## v0.4.2 — 2026-10-20

Automatic friend discovery and presence recovery:
- Retain DHT searches and late replies by key after a caller times out, including while other keys are being searched. The cache is bounded and expires after two minutes.
- Preserve announcements when looking up the same matchmaking key; renew announcements every two minutes. A lookup must not change publication into lookup-only mode.
- Poll discovery again promptly and verify every candidate's identity; an unrelated candidate does not hide a later matching one.
- Learn a return-address hint from an authenticated friend's incoming presence request. The hint uses the observed source IP and signed contact port; it is saved only after a fresh signed presence response from the expected friend. It does not bypass authentication or require either game to be running.
- Isolate discovery failures to the affected friend instead of aborting the entire presence round.
- Serialize and atomically save address-book changes, reloading current entries before mutation so a stale lookup cannot resurrect a removed friend or overwrite a newer update.
- Log friend_presence_failed and friend_presence_changed for diagnosis.

Upgrade both PCs through Check updates, or follow FIRST_UPDATE.md. Manual IP entry/restoration is not part of the normal recovery flow. Keep runtime/user and existing friend codes. Public DHT bootstrap may take time; the launcher continues collecting results rather than dropping them at the first timeout.

Validation: separate local DHT processes test late results after timeout and publication through a third node; real local peer services test asymmetric discovery, reverse identity verification and remove/re-add recovery. Existing peer signatures, invitation/session and restart checks are also rerun. These checks do not constitute a retail Internet race or prove every NAT/UDP configuration works; UDP gameplay relay is unchanged.
## v0.4.1 — 2026-10-20

Friend presence and invitation recovery fixes, including the launcher updater.

- Keep the last verified friend address during a temporary outage so a restarted launcher can be contacted again without waiting for DHT rediscovery. Failed lookups no longer advance the last-seen timestamp.
- Do not consume the one-minute discovery retry interval before DHT is ready at launcher startup.
- Repeated delivery of an invitation preserves its accepted state. Reused invitation IDs with conflicting details are rejected.
- Do not automatically prompt again for accepted invitations or repeatedly reopen a failed invitation. Acceptance errors now show HTTP/transport details with explicit Retry/Cancel controls. Cancel stops automatic prompts for that pending invitation; it does not claim that joining succeeded.

Validation: 8 restart checks, 34 peer/session checks using real loopback services, and 36 native invitation-dialog checks with mocked HTTP transport. Launcher and HLE library compile. These checks do not establish successful party joining or a playable race across the Internet. The first presence update coinciding with game startup remains an observation to verify on two PCs.

Upgrade both PCs. The refreshed v0.4.0 with **Check updates** can install v0.4.1 from the launcher. Close the game and disable Multiplayer Online before installation. Older launchers without the updater need one manual ZIP extraction into the existing installation; preserve `runtime/user`, your dump and mods. See FIRST_UPDATE.md.

For the two-PC test: first open both launchers with Multiplayer Online enabled and both games closed; check friend presence. Restart one launcher, then check recovery. Start both games and send one invite. Record whether acceptance succeeds or the exact error shown. Logs are under `runtime/user/logs` (including `online_server_*.jsonl` and game stdout/stderr logs).

No game files, identities, saves or user configuration are included. No new retail runtime milestone or verified reconstruction is claimed. Changes prepared with OpenAI Codex; existing project and third-party license notices remain in place.
## v0.4.0 updater refresh — 2026-10-20

Automatic and manual release checks, SHA-256 verified downloads, separate installer, backup/rollback and launcher restart. The first installation is manual, even from v0.4.0. No runtime binary changes. See FIRST_UPDATE.md.

# v0.4.0 — Friends and public matchmaking without a shared server

Each player's game now talks only to the service running inside their own launcher. The services find each other and cooperate directly; nobody has to host a room or run a server for the others.

- **Friends list (AMICI).** The launcher's right-side drawer is a friends list, in the style of game clients: copy your code, add a friend by code, remove a friend. Friends must add each other. The same friends appear in the game's own friend list, with live status (in game, launcher open, offline). Offline friends stay listed.
- **Invite and join from the game.** Invite a friend from the in-game friends list, or JOIN a friend whose game is joinable. The session stays on the host's service; the friend's service relays the requests to it, signed. Inviting sends at once; the recipient accepts in game.
- **Public matchmaking.** Online > Matchmaking pairs every player using the client. Services that can be reached from the internet announce themselves on the DHT; the hub is elected automatically (lowest friend code among the reachable services). Players behind NAT join through their own service, which forwards the matchmaking to the hub.
- **Split screen up to four players and keyboard input**, with a launcher toggle.
- The launcher's room lobby and the public-server field are gone: friends and matchmaking replace them.
- Fixed a crash in the DHT bootstrap at shutdown (Winsock torn down under a running name lookup).

## Verified

Two retail EU 1.21 instances on one PC, each connected only to its own service (different loopback addresses): friends shown JOINABLE both ways, JOIN into the same lobby and a Crash Cove race; public matchmaking through the hub into the same lobby and a Polar Pass race. Server suites: 237 checks, friend network and cross-service invitations 33 checks, cross-service matchmaking at protocol level, C++/Python comparison 57 steps with no differences, invitation dialog 34 checks.

## Limits

- Not yet tested between two separate PCs or over the internet.
- No relay between services yet: players behind strict NAT may not reach each other directly (planned next).
- Public matchmaking needs at least one service reachable from the internet (UPnP or a manual public address); two PCs on a LAN without UPnP do not find each other for public matchmaking.
- Full online support (R8) is not claimed. Reconstruction: zero new verified replacements; compatibility registry unchanged.

## Installation

Close the game and launcher, extract the ZIP and run launcher.exe. Preserve your existing runtime/user folder when upgrading. All players should update: v0.4.0 services do not interoperate with the v0.3.x room lobby. Supply your own base dump and v01.21 update; game data, saves and credentials are not included.

## Credits

GPL-2.0 project, shadPS4 and bbport references, jech/dht, Julio Cacko Xbox icons (CC0-1.0), user-supplied branding. Implementation and review assisted by Claude (Anthropic) and OpenAI Codex.

---

# v0.3.2 — Client lobby and experimental host migration

- The right-side drawer is now the room lobby, with complete invitations, participant management and a visible scrollbar for the eight-player list. Common room actions stay inside the client.
- Load the game's fonts from your own dump, alongside the background. Preserve the top-left logo with an embedded branding fallback. No retail fonts or backgrounds are distributed.
- Experimental EU1.21 race host migration preserves the live session and surviving players' object ownership. Local three-client Dingo Canyon and four-client Polar Pass tests survived loss of the host and backend; Polar Pass also survived a second host loss during the same race.
- Signed launcher-room checkpoints support successor recovery, same-room membership and fresh invitations. Three-server HTTP recovery tests passed, including retry and former-host exclusion.

## Experimental limits

Host migration is **disabled by default**. Testing requires `CTR_EXPERIMENTAL_HOST_MIGRATION=1` for launcher/server and runtime. Only the pinned CUSA14876 v01.21 executable is supported. Local tests do not establish Internet/NAT or relay failover. Game backend retargeting is unfinished: after the race, returning to the lobby can display "Unable to connect to servers". Combined launcher UI and retail recovery is not yet validated end to end. Wallet migration, network partitions and unreachable-successor recovery remain unfinished. See HOST_MIGRATION.md.

The existing graphics, economy and local backup features and limitations remain. Full online support (R8) is not claimed. Runtime: local race continuity observed. Reconstruction: zero new verified replacements; compatibility registry unchanged.

## Installation

Close the game and launcher, extract the ZIP and run launcher.exe. Preserve your existing runtime/user folder when upgrading. All participants should update. Supply your own base dump and v01.21 update; game data, saves and credentials are not included.

## Verification and credits

Launcher/server builds and route-parser checks passed. Signed HTTP room recovery reports zero failures; real retail evidence remains local. GPL-2.0 project, shadPS4 and bbport references, jech/dht, Julio Cacko Xbox icons (CC0-1.0), user-supplied branding. Implementation and review assisted by Claude, Gemini and OpenAI Codex. See README and included licenses.

---

## v0.3.1 local backup update — 2026-10-16

- Automatic game-save copies before every launch and after game exit, including crash exits, in `runtime/user/savedata_backups/`. A failed pre-launch copy cancels launch.
- The local server archives the previous wallet before replacing it; a failed backup prevents the transaction from overwriting the wallet.
- Signed local copies of your Pit Stop inventory, coin balances and reward receipts on joining a lobby, every successful 15-second heartbeat, and after game exit. Snapshots are separated by host/account; unchanged snapshots are deduplicated and an empty inventory never overwrites an older copy.
- **Update both your launcher and your friend's/host's launcher.** Older hosts cannot supply wallet backups. Interrupted connections can leave changes since the last successful copy unbacked up.
- These are recovery copies, not automatic wallet synchronization between hosts. Missing past purchases still require the previous host's data or a prior backup. No balances are automatically merged or restored.
- Validation: 197 server checks and 2 CLI backup tests passed, including signed HTTP host/guest transfer, account isolation, empty-inventory history retention, backup-write failure rollback and Windows long paths. No new remote two-PC gameplay claim.
- See `LOCAL_BACKUPS.md` in the download. Earlier economy, invitation and graphics features and their limitations remain unchanged.

Upgrade: close the game and launcher, replace program files from the ZIP, and **preserve your existing `runtime/user` folder**. Both players should download this updated v0.3.1 package.

## v0.3.1 graphics update — 2026-10-16

- Independent internal scene resolution: 720p, 900p, 1080p, 1440p, 1800p and 2160p, selected in launcher Settings. Display/window resolution remains separate.
- AMD FSR 1 spatial upscaling of the 3D scene before the HUD, selectable from the launcher. This is not FSR 3 or frame generation.
- Experimental NVIDIA DLSS Super Resolution integration is available in source. The portable GPL package does not bundle the NVIDIA SDK/runtime or SDK-linked bridge. Build the optional bridge against NVIDIA's SDK separately, then put ctr_dlss.dll and nvngx_dlss.dll beside build/CTRPort-gpu/ctrport.exe. See UPSCALING.md and the bridge build script. Without them, selecting DLSS cannot enable NGX; use Off or FSR 1.
- DLSS object motion is NOT corrected: the inherited target3 heuristic is not a validated moving-object mask. Camera reprojection works in the captured case, but temporal race quality is not certified.
- Retail v01.21 Crash Cove, RTX 4070 Ti SUPER: single 30-second stationary-grid samples with AI racing measured Off native4K 46.35 FPS, Off1080 49.49, FSR1 1080->4K 50.15, DLSS1080->4K 29.88. Canvas4K/window1080, 60 FPS patch enabled. FSR's observed +8.2% versus native4K is preliminary; rosters/time differ, no deterministic replay or full-lap benchmark. DLSS was slower.
- Both retail versions were checked at title-scene resolutions; race input/acceleration was observed with FSR1 and DLSS on v01.21. New tools capture actual GPU buffers, measure guest presentations, and check consecutive-frame reprojection.
- No game files, saves, diagnostic captures or NVIDIA binaries are distributed. Reconstruction remains zero active verified replacements; all prior economy/invitation limitations remain.

Upgrade: close the game/launcher, replace program files, and preserve runtime/user.

## v0.3.1 invitation update — 2026-10-16

- Invitations are now stored on the existing lobby/relay host and retrieved by
  the recipient through outbound authenticated HTTP requests. Sending no longer
  requires discovering or connecting to the recipient's IP or public TCP 8780.
  No extra server or account is required. Game datagrams, direct/relay selection
  and the game room selector are unchanged.
- Add the actual lobby host's friend code to the recipient's address book (and
  the inviter's code if different). The host must remain reachable and running.
  This does not solve an unreachable host behind CGNAT or discovery of an unknown
  host. Update both launchers. Pending invites expire after 15 minutes or host
  restart/lobby reset. Choose **Inviti ricevuti** to check known hosts; the main
  launcher also polls one known host every 15 seconds.
- Cached addresses are identity-verified before DHT lookup. Manual friend lookup
  waits up to 20 seconds; diagnostics distinguish local DHT startup failure,
  missing nodes and a missing friend announcement.
- Validation: 189 existing server checks, 11 invitation-pull checks, 5 isolated
  DHT checks, and game Auto Direct/Relay fallback controls pass. Invitation tests
  include an absent recipient listener, identity isolation, expiry, duplicate
  delivery and accepting into the unchanged game room. No real two-PC CGNAT
  invitation or retail online race is claimed by these tests.
- See `INVITATIONS.md` in the download for operation and limitations. All earlier
  v0.3.1 economy features and limitations remain applicable.

# CTRNativePorted v0.3.1 — Wumpa Coins and Pit Stop purchases

## New since v0.3.0

- The launcher's visible player name is passed to the game in both Online and
  offline sessions, fixing the unwanted CTRPlayer fallback for valid names.
- Wumpa Coins are credited from valid end-of-race activity reports, including
  single-player Adventure. Rewards use the local retail track/rank table; unknown
  tracks use a project fallback of 32 coins, and unranked activities use first-place
  base rewards. These are simplified project rules, not complete retail bonus parity.
- Optional **Wumpa Coins x100** switch in launcher Settings, off by default and
  saved per player. The multiplier applies to race earnings, not purchase prices.
- Pit Stop single-item and bundle purchases, with persistent inventory and balance.
  Already-owned bundle contents are excluded from the charge. Balance and all
  granted items save together; repeated requests do not debit twice.
- Shop and reward catalogues are read locally from the user's v01.21 runtime.
  No game data or catalogues are included in this download or source repository.

## Install and use

Extract `CTRNativePorted-v0.3.1-win64.zip` and run `launcher.exe`. When upgrading,
close the launcher and game, replace program files, and keep your existing
`runtime/user` directory to retain saves, settings, friends and local wallet.

Supply your own CUSA14876 base dump and v01.21 update for these economy features.
Enable **Online** in Settings to start the project service, even when playing
single-player; no friend or lobby is required to earn coins in Adventure.
Choose **Wumpa Coins x100** before launching if you want faster unlocks.
The existing friends, parties (up to 8 players) and relay features remain included.

## Verification

- Runtime, launcher and server builds pass; 189 server checks pass.
- Original retail Adventure Crash Cove: 32 coins awarded and visible in Pit Stop.
- Real Velo purchased for 1500, selectable, retained after a cold restart.
- Back N. Time: Baby Crash bundle purchased for 3375: all four contents available,
  balance reduced from 8500 to 5125 and inventory persisted.
- Synthetic tests cover insufficient funds, write-failure rollback, duplicate
  requests after restart, partial ownership and per-player x1/x100 isolation.

## Known limits

Wumpa Challenges progression/rewards are not implemented; opening their screen
and recording events does not mean completing them pays coins. Individual
activity modes and x100 have not all been verified through the original game UI.
Economy currently requires Online enabled. Wallets belong to the service host;
wallet migration across remote lobbies is not provided. Catalogue variants with
unsupported price accessors/handle aliases and multi-page inventories are deferred.
Partial-ownership discounts have synthetic coverage, not an original-client test.

No remote-PC retail online race or full online support (R8) is claimed. Existing
NAT/CGNAT and party restart limitations remain. This experimental release adds
no reconstructed engine replacement; original game code still runs at runtime.

## Credits and license

GPL-2.0 project; renderer adapted from shadPS4 v0.18.0, with bbport references.
DHT uses jech/dht (license included). Xbox icons: Julio Cacko, CC0-1.0.
User-supplied launcher branding, transparency edited with OpenAI imagegen;
marks remain their owners' property. Implementation/review assisted by Claude,
Gemini and OpenAI Codex. See the included README and provenance/license files.

---

# CTRNativePorted v0.3.0 — Friends, parties and launcher refresh

## New since v0.2.2

- Refreshed launcher with user-supplied branding and the Brief2 2a friends drawer:
  rounded antialiased edges, shadows and animated opening/closing. The drawer and
  its tab disappear in Settings, Mods and Advanced.
- Persistent friend codes and friend list, Mainline DHT discovery, signed server
  identity verification and background connection checks.
- Invite an online friend, accept an invitation, join a friend's party or create
  a new party. Parties hold up to 8 players including their owner; duplicate
  player names and the ninth member are rejected.
- Settings now needs only Online and a visible player name for networking.
  No manual IP, hosting switch or shared room password. Services start automatically.
- Party membership supplies the game's network selector; authentication checks
  membership and compatible matchmaking waits for the complete party.
- Automatic direct-to-relay fallback, with no manual relay switch.
- Friend-service fixes for verification, persistence, asynchronous dialog work
  and DHT lifecycle; signed party requests reject replayed nonces.

Includes the existing v0.2.2 Pit Stop/Wumpa Challenges support, controller fixes,
PlayStation controller support and rendering options.

## Install and use

Extract `CTRNativePorted-v0.3.0-win64.zip` and run `launcher.exe` at the top level.
Supply your own CUSA14876 v01.00 dump, optionally with the v01.21 update (required
for 60 FPS and Pit Stop/Wumpa Challenges). No game data, saves or identity keys
are included. Keep your existing runtime/user folder to retain saves and identity.

Enable Online, choose a distinct name, exchange friend codes and form the party
from the Home friends drawer before PLAY. Each member then selects
ONLINE > Matchmaking > Go to Lobby in the game with a compatible mode.

## Verification and limits

Launcher/server builds pass; 161 server checks pass, including signed invitations
between two local services, common room selection, capacity, expiry and leave.
Python/C++ protocol comparison: 57 steps, zero differences. Automatic relay was
checked locally with direct traffic deliberately blocked.

Party changes require restarting the game. No host migration. DHT/UPnP/relay do
not guarantee connectivity behind restrictive NAT or CGNAT. No retail online race
between remote PCs has been verified; full online support (R8) is not claimed.
Pit Stop purchases and earning coins from races remain unimplemented.
No new retail runtime milestone or reconstructed function is claimed by this release.

## Credits and license

GPL-2.0 project; renderer adapted from shadPS4 v0.18.0, with bbport references.
DHT uses jech/dht (license included). Xbox icons: Julio Cacko, CC0-1.0.
Launcher branding supplied by the user, transparency edited with OpenAI imagegen;
marks remain their owners' property. Implementation and review assisted by Claude,
Gemini and OpenAI Codex. See included README and license/provenance files.
