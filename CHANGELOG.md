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
