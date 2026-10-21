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
