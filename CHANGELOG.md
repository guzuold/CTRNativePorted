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
